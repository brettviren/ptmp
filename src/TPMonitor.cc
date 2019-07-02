#include "ptmp/api.h"
#include "ptmp/data.h"
#include "ptmp/internals.h"


#include <cstdio>

#include "json.hpp"

using json = nlohmann::json;

static
void tptap(zsock_t* pipe, void* vargs)
{
    auto config = json::parse((const char*) vargs);

    uint32_t nrecv = 0;
    const uint32_t tapid = config["id"];
    
    zsock_t* isock = ptmp::internals::endpoint(config["input"].dump());
    zsock_t* osock = ptmp::internals::endpoint(config["output"].dump());
    zpoller_t* poller = zpoller_new(pipe, isock, osock, NULL);

    zsock_signal(pipe, 0); // signal ready    

    const int output_timeout = 100;
    int count = 0;
    bool got_quit = false;

    while (!zsys_interrupted) {

        // zsys_debug("tptap: waiting");
        void* which = zpoller_wait(poller, -1);
        if (!which) {
            zsys_info("tap: interrupted");
            break;
        }
        if (which == pipe) {
            zsys_info("tap: got quit with %d seen", nrecv);
            got_quit = true;
            break;
        }
        zmsg_t* msg = zmsg_recv(which);

        // zsys_debug("tptap: got message");

        zmsg_t* mymsg = NULL;

        // Forward original message
        if (which == isock) {
            mymsg = zmsg_dup(msg);

            zmq_pollitem_t pi[2];
            pi[0].socket = zsock_resolve(pipe);
            pi[0].events = ZMQ_POLLIN;
            pi[1].socket = zsock_resolve(osock);
            pi[1].events = ZMQ_POLLOUT;
            int rc = zmq_poll(pi, 2, -1);
            if (rc <= 0) {
                zsys_debug("tap: interupted in poll on osock");
                break;
            }
            if (pi[0].revents & ZMQ_POLLIN) {
                zsys_debug("tap: got quit while polling output");
                got_quit = true;
                break;
            }
            zmsg_send(&msg, osock);
        }
        if (which == osock) {
            zmq_pollitem_t pi[2];
            pi[0].socket = zsock_resolve(pipe);
            pi[0].events = ZMQ_POLLIN;
            pi[1].socket = zsock_resolve(isock);
            pi[1].events = ZMQ_POLLOUT;
            int rc = zmq_poll(pi, 2, -1);
            if (rc <= 0) {
                zsys_debug("tap: interupted in poll on isock");
                break;
            }
            if (pi[0].revents & ZMQ_POLLIN) {
                zsys_debug("tap: got quit while polling input");
                got_quit = true;
                break;
            }
            zmsg_send(&msg, isock); // fixme: POLLOUT?
            continue;           // only tap into "input" messages
        }

        ptmp::data::TPSet tpset;
        ptmp::internals::recv(mymsg, tpset);
        int64_t now = ptmp::data::now();
        ++nrecv;
        auto* summary = new ptmp::TPMonitor::Summary{
            tapid,
            now,
            nrecv,
            tpset.count(),
            tpset.detid(),
            tpset.created(),
            tpset.tstart(),
            tpset.tspan(),
            (uint32_t)tpset.tps_size()
        };
        int rc = zsock_send(pipe, "p", summary); // receiver must delete!
        if (rc != 0) {
            zsys_error("tap: failed to send to pipe");
            break;
        }
        ++count;
    }
    zsys_debug("tap: finishing after %d", count);
    zpoller_destroy(&poller);
    zsock_destroy(&isock);
    zsock_destroy(&osock);
    if (got_quit) {
        return;
    }
    zsys_debug("tap: waiting for quit");
    zsock_wait(pipe);
}

// Main actor.  It creates the tap proxies, receives TPSets from them
// and writes summary to file.
static
void tpmonitor(zsock_t* pipe, void* vargs)
{
    auto config = json::parse((const char*) vargs);

    std::string filename = config["filename"];

    std::vector<zactor_t*> taps;
    zpoller_t* poller = zpoller_new(pipe, NULL);


    /// could make monitor spit out a message instead of writing a file....
    // const std::string spigot_addr = "inproc://" + filename;
    // zsock_t* spigot = zsock_new_pull(spigot_addr.c_str());

    for (const auto& jtap : config["taps"]) {
        const std::string cfg = jtap.dump();
        zactor_t* tap = zactor_new(tptap, (void*)cfg.c_str());
        taps.push_back(tap);
        zpoller_add(poller, tap);
        zsys_debug("monitor: made tap %d", jtap["id"].get<int>());
    }
    
    zsock_signal(pipe, 0); // signal ready    

    // open file...
    FILE* fp = fopen(filename.c_str(), "w");
    if (!fp) {
        zsys_error("monitor: failed to open file %s", filename.c_str());
        return;
    }

    bool got_quit = false;
    while (!zsys_interrupted) {

        void* which = zpoller_wait(poller, -1);
        if (!which) {
            zsys_info("monitor: interrupted");
            break;
        }
        if (which == pipe) {
            zsys_info("monitor: got quit");
            got_quit = true;
            break;
        }

        void* vptr = 0;
        int rc = zsock_recv(which, "p", &vptr);
        // write summary to file....
        // binary:
        // fwrite(vptr, sizeof(ptmp::TPMonitor::Summary), 1, fp);
        {
            ptmp::TPMonitor::Summary* s = (ptmp::TPMonitor::Summary*)vptr;
            fprintf(fp, "%d %ld %d %d %d %ld %ld %d %d\n",
                    s->tapid, s->trecv, s->nrecv, s->count, s->detid, s->created, s->tstart, s->tspan, s->ntps);
            delete (s);
        }
    }

    // close file ...
    fclose(fp); fp=NULL;

    zsys_debug("monitor: signalling taps to shutdown");
    zpoller_destroy(&poller);
    for (zactor_t* tap : taps) {
        zsock_signal(zactor_sock(tap), 0);
        zactor_destroy(&tap);
    }
    if (got_quit) {
        return;
    }
    zsys_debug("monitor: waiting for quit");
    zsock_wait(pipe);
}


ptmp::TPMonitor::TPMonitor(const std::string& config)
    : m_actor(zactor_new(tpmonitor, (void*)config.c_str()))
{
}

ptmp::TPMonitor::~TPMonitor()
{
    zsock_signal(zactor_sock(m_actor), 0); // signal quit
    zactor_destroy(&m_actor);
}
