#include "ptmp/api.h"
#include "ptmp/data.h"
#include "ptmp/internals.h"


#include <cstdio>

#include "json.hpp"

using json = nlohmann::json;


void tptap(zsock_t* pipe, void* vargs)
{
    auto config = json::parse((const char*) vargs);

    uint32_t nrecv = 0;
    const uint32_t tapid = config["id"];
    
    zsock_t* isock = ptmp::internals::endpoint(config["input"].dump());
    zsock_t* osock = ptmp::internals::endpoint(config["output"].dump());
    zpoller_t* poller = zpoller_new(pipe, isock, osock, NULL);

    zsock_signal(pipe, 0); // signal ready    

    while (!zsys_interrupted) {

        void* which = zpoller_wait(poller, -1);
        if (!which) {
            zsys_info("tptap interrupted");
            break;
        }
        if (which == pipe) {
            zsys_info("tptap got quit with %d seen", nrecv);
            break;
        }
        zmsg_t* msg = zmsg_recv(which);

        zmsg_t* mymsg = NULL;

        // Forward original message
        if (which == isock) {
            mymsg = zmsg_dup(msg);
            zmsg_send(&msg, osock);
        }
        if (which == osock) {
            zmsg_send(&msg, isock);
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
        int rc = zsock_send(pipe, "%p", summary); // receiver must delete!
        if (rc != 0) {
            zsys_error("tptap: failed to send to pipe");
            break;
        }
    }
    zpoller_destroy(&poller);
    zsock_destroy(&isock);
    zsock_destroy(&osock);
}

// Main actor.  It creates the tap proxies, receives TPSets from them
// and writes summary to file.
void tpmonitor(zsock_t* pipe, void* vargs)
{
    auto config = json::parse((const char*) vargs);

    std::string filename = config["filename"];

    std::vector<zactor_t*> taps;
    zpoller_t* poller = zpoller_new(pipe, NULL);

    // Just want something unique.  Is it okay if filename contains "/"?
    const std::string spigot_addr = "inproc://" + filename;
    zsock_t* spigot = zsock_new_pull(spigot_addr.c_str());

    for (const auto& jtap : config["taps"]) {
        const std::string cfg = jtap.dump();
        zactor_t* tap = zactor_new(tptap, (void*)cfg.c_str());
        taps.push_back(tap);
        zpoller_add(poller, tap);
    }
    
    zsock_signal(pipe, 0); // signal ready    

    // open file...
    FILE* fp = fopen(filename.c_str(), "w");
    if (!fp) {
        zsys_error("Failed to open file %s", filename.c_str());
        return;
    }

    while (!zsys_interrupted) {

        void* which = zpoller_wait(poller, -1);
        if (!which) {
            zsys_info("tpmonitor interrupted");
            break;
        }
        if (which == pipe) {
            zsys_info("tpmonitor got quit");
            break;
        }

        void* vptr = 0;
        int rc = zsock_recv(which, "%p", &vptr);
        // write summary to file....
        // type safety?  we don't need no stinking type safety!
        fwrite(vptr, sizeof(ptmp::TPMonitor::Summary), 1, fp);
        delete ((ptmp::TPMonitor::Summary*)vptr); vptr=NULL;
    }

    // close file ...
    fclose(fp); fp=NULL;

    zpoller_destroy(&poller);
    for (zactor_t* tap : taps) {
        zsock_signal(zactor_sock(tap), 0);
        zactor_destroy(&tap);
    }
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
