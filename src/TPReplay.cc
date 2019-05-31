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
    zsys_debug("replay: %s %d %d %4d %ld",
               label, head.detid, head.count, head.ntps, head.tstart);
}


// The actor function
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

    zsock_signal(pipe, 0);      // signal ready

    zpoller_t* pipe_poller = zpoller_new(pipe, isock, NULL);

    int wait_time_ms = -1;

    int64_t last_send_time = 0;
    int64_t last_mesg_time = 0;

    while (!zsys_interrupted) {

        void* which = zpoller_wait(pipe_poller, wait_time_ms);
        if (!which) {
            zsys_info("TPReplay proxy interrupted");
            break;
        }
        if (which == pipe) {
            zsys_info("TPReplay proxy got quit");
            break;
        }

        // got input

        zmsg_t* msg = zmsg_recv(isock);
        if (!msg) {
            zsys_info("TPReplay proxy interrupted");
            break;
        }
        auto header = msg_header(msg);

        if (header.tstart == 0xffffffffffffffff) {
            dump_header("kill", header);
            zmsg_destroy(&msg);
            continue;
        }
        
        if (last_send_time == 0) { // first message
            last_send_time = zclock_usecs();
            last_mesg_time = header.tstart;
            int rc = zmsg_send(&msg, osock);
            if (rc  != 0) {
                zsys_error("send failed");
                break;
            }
            zsys_debug("replay: first");
            continue;
        }

        dump_header("send", header);

        int64_t delta_tau = header.tstart - last_mesg_time;
        int64_t t_now = zclock_usecs();
        int64_t delta_t = last_send_time + delta_tau/speed - t_now;
        if (delta_t < 0) {
            // tardy!
            delta_t = 0;
        }
        if (delta_t > 0 ) {
            int zzz_ms = delta_t / 1000;
            zsys_debug("replay: sleep %d ms after t=%ld/dt=%d, tau=%ld/dtau=%d",
                      zzz_ms, last_send_time, delta_t, last_mesg_time, delta_tau);
            zclock_sleep(zzz_ms);
            last_mesg_time = header.tstart;
        }
        last_send_time = zclock_usecs();
        int rc = zmsg_send(&msg, osock);
        if (rc  != 0) {
            zsys_error("send failed");
            break;
        }
    }

    zpoller_destroy(&pipe_poller);
    zsock_destroy(&isock);
    zsock_destroy(&osock);
    
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
