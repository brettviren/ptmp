#include "ptmp/api.h"

#include "json.hpp"

using json = nlohmann::json;

// fixme: this is duplicated from TPSorted.  And, better to have
// header outside of protobuf object.  This needs refactoring.
struct msg_header_t {
    uint32_t count, detid;
    uint64_t tstart;
    int ntps;
};
static msg_header_t msg_header(zmsg_t* msg)
{
    zmsg_first(msg);            // msg id
    zframe_t* pay = zmsg_next(msg);
    ptmp::data::TPSet tps;
    tps.ParseFromArray(zframe_data(pay), zframe_size(pay));
    return msg_header_t{tps.count(), tps.detid(), tps.tstart(), tps.tps_size()};
}

static
void dump_header(const char* label, msg_header_t& head)
{
    zsys_debug("replay: %s 0x%x #%d %4d %ld",
               label, head.detid, head.count, head.ntps, head.tstart);
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

    ptmp::data::real_time_t last_send_time = 0;
    ptmp::data::data_time_t last_mesg_time = 0;

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
        auto header = msg_header(msg);

        // dump_header("send", header);

        if (header.tstart == 0xffffffffffffffff) {
            dump_header("kill", header);
            zmsg_destroy(&msg);
            continue;
        }
        

        if (last_send_time == 0) { // first message
            last_send_time = ptmp::data::now();
            last_mesg_time = header.tstart;
            ptmp::data::TPSet tpset;
            ptmp::internals::recv(msg, tpset);
            tpset.set_created(last_send_time);
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

        const ptmp::data::real_time_t delta_tau = (header.tstart - last_mesg_time)/speed;
        const ptmp::data::real_time_t t_now = ptmp::data::now();
        ptmp::data::real_time_t delta_t = last_send_time + delta_tau - t_now;
        if (delta_t < 0) {
            // tardy!
            delta_t = 0;
        }
        if (delta_t > 0 ) {
            ptmp::internals::microsleep(delta_t);
            last_mesg_time = header.tstart;
        }
        last_send_time = ptmp::data::now();
        {
            ptmp::data::TPSet tpset;
            ptmp::internals::recv(msg, tpset);
            tpset.set_created(last_send_time);
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
