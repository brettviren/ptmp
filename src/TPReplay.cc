#include "ptmp/api.h"
#include "ptmp/factory.h"

#include "json.hpp"

PTMP_AGENT(ptmp::TPReplay, replay)

using json = nlohmann::json;

static
void dump_header(const char* label, ptmp::data::TPSet& tpset)
{
    zsys_debug("replay: %s 0x%x #%d %4d %ld",
               label, tpset.detid(), tpset.count(),
               tpset.tps_size(), tpset.tstart());
}


// The actor function
static
void tpreplay_proxy(zsock_t* pipe, void* vargs)
{
    auto config = json::parse((const char*) vargs);
    zsock_t* isock = ptmp::internals::endpoint(config["input"].dump());
    zsock_t* osock = ptmp::internals::endpoint(config["output"].dump());

    // Note, speed MUST scale HW clock to real time microseconds.  Eg,
    // PDSP's tstart should be counted in 50MHz ticks so speed should
    // be 50.0.
    double speed = 50.0;
    if (config["speed"].is_number()) {
        speed = config["speed"];
    }
    int rewrite_count = 0;
    if (config["rewrite_count"].is_number()) {
        rewrite_count = config["rewrite_count"];
    }

    zsock_signal(pipe, 0);      // signal ready

    zpoller_t* poller = zpoller_new(pipe, isock, NULL);

    int wait_time_ms = -1;

    ptmp::data::real_time_t first_created = 0, last_created = 0;
    ptmp::data::data_time_t first_tstart = 0, last_tstart = 0;

    int count = 0;
    bool got_quit = false;
    while (!zsys_interrupted) {

        // zsys_debug("replay: waiting");

        void* which = zpoller_wait(poller, wait_time_ms);
        if (!which) {
            zsys_info("replay: interrupted in poll");
            break;
        }
        if (which == pipe) {
            zsys_info("replay: got quit");
            got_quit = true;
            goto cleanup;
        }
        
        // got input

        zmsg_t* msg = zmsg_recv(isock);
        if (!msg) {
            zsys_info("replay: interrupted in recv");
            break;
        }

        // unpack message to get timing
        ptmp::data::TPSet tpset;
        ptmp::internals::recv(msg, tpset);
        const ptmp::data::data_time_t tstart = tpset.tstart();

        if (last_created == 0) { // first message
            first_created = last_created = ptmp::data::now();
            first_tstart = last_tstart = tstart;

            tpset.set_created(last_created);
            if (rewrite_count) {
                tpset.set_count(count);
            }

            // fixme: kludge to avoid output block stopping shutdown
            while (! (zsock_events(osock) & ZMQ_POLLOUT)) {
                if (zsock_events(pipe) & ZMQ_POLLIN) {
                    got_quit = true;
                    goto cleanup;
                }
                zclock_sleep(1); // ms
            }

            ptmp::internals::send(osock, tpset);
            ++count;
            zsys_debug("replay: first");
            continue;
        }

        if (tstart < last_tstart) {
            // tardy
            zsys_debug("replay: tardy %d", tpset.count());
            continue;
        }

        const ptmp::data::real_time_t delta_tau = (tstart - first_tstart)/speed;
        const ptmp::data::real_time_t t_now = ptmp::data::now();
        const ptmp::data::real_time_t delta_t = t_now - first_created;
        const ptmp::data::real_time_t to_sleep = delta_tau - delta_t;
        last_tstart = tstart;
        last_created = t_now;
        if (to_sleep > 0) {
            ptmp::internals::microsleep(to_sleep);
        }
        {
            tpset.set_created(ptmp::data::now());
            if (rewrite_count) {
                tpset.set_count(count);
            }

            // fixme: kludge to avoid output block stopping shutdown
            while (! (zsock_events(osock) & ZMQ_POLLOUT)) {
                if (zsock_events(pipe) & ZMQ_POLLIN) {
                    got_quit = true;
                    goto cleanup;
                }
                zclock_sleep(1); // ms
            }

            ptmp::internals::send(osock, tpset);
            ++count;
        }
    }

  cleanup:
    zsys_debug("replay: finished after %d", count);

    zpoller_destroy(&poller);
    zsock_destroy(&isock);
    zsock_destroy(&osock);
    
    if (got_quit) {
        return;
    }
    zsys_debug("replay: waiting for quit");
    zsock_wait(pipe);
}


ptmp::TPReplay::TPReplay(const std::string& config)
    : m_actor(zactor_new(tpreplay_proxy, (void*)config.c_str()))
{
}

ptmp::TPReplay::~TPReplay()
{
    zsock_signal(zactor_sock(m_actor), 0); // signal quit
    zactor_destroy(&m_actor);
}
