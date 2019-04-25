#include "ptmp/api.h"

#include "json.hpp"

using json = nlohmann::json;

// fixme: this is duplicated from TPSorted.  And, better to have
// header outside of protobuf object.  This needs refactoring.
struct msg_header_t {
    uint32_t count, detid;
    uint64_t tstart;
};
static msg_header_t msg_header(zmsg_t* msg)
{
    zmsg_first(msg);            // msg id
    zframe_t* pay = zmsg_next(msg);
    ptmp::data::TPSet tps;
    tps.ParseFromArray(zframe_data(pay), zframe_size(pay));
    return msg_header_t{tps.count(), tps.detid(), tps.tstart()};
}


// The actor function
void tpreplay_proxy(zsock_t* pipe, void* vargs)
{
    auto config = json::parse((const char*) vargs);
    zsock_t* isock = ptmp::internals::endpoint(config["input"]);
    zsock_t* osock = ptmp::internals::endpoint(config["output"]);
    double speed = 1.0;
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
        
        

    }
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
