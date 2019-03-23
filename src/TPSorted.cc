#include "ptmp/api.h"
#include "json.hpp"

#include <algorithm>
#include <vector>
#include <unordered_map>


using json = nlohmann::json;


// For TPSender/TPReceiver, there's just one socket and it may have
// multiple connects or binds.  TPSorted interprets the list of
// endpoints as one socket per endpoint so do a little cfg gymnastics.
std::vector<zsock_t*> make_sockets(json jcfg)
{
    zsys_info("make sockets: %s", jcfg.dump().c_str());
    const std::vector<std::string> bcs{"bind","connect"};
    std::vector<zsock_t*> ret;
    for (auto bc : bcs) {
        for (auto jaddr : jcfg[bc]) {
            json cfg = {
                { "socket", {
                        { "type", jcfg["type"] },
                        { bc, jaddr } } } };
            std::string cfgstr = cfg.dump();
            zsys_info("proxy socket: %s", cfgstr.c_str());
            zsock_t* sock = ptmp::internals::endpoint(cfgstr);
            ret.push_back(sock);
        }
    }
    return ret;
}

// This is probably a painfully slow function that gets run multiple
// times on the same message.  The PTMP message coded design is
// systemically not optimal.  A header with count/detid/tstart should
// be cheap to unpack.  Something like zproto_codec_c would be better.
// The payload could still be in protobuf.  With just one message type
// this is all messy enough but if ptmp expands to multiple message
// types then things will get even hairier.
//
// For now, just forge ahead and see how bad it performs, but keep
// this function static to contain the cancer.
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

static void header_dump(std::string s, msg_header_t& h)
{
    static uint64_t t0 = h.tstart;
    zsys_warning("%s: #%d from %d t=%.1f",
                 s.c_str(), h.count, h.detid, 1e-6*(h.tstart-t0));
}

// The actor function
void tpsorted_proxy(zsock_t* pipe, void* vargs)
{
    // keep EOT fitting in a signed int64
    const uint64_t EOT = 0x7FFFFFFFFFFFFFFF;

    auto config = json::parse((const char*) vargs);
    auto input = make_sockets(config["input"]["socket"]);
    auto output = make_sockets(config["output"]["socket"]);

    const size_t ninputs = input.size();

    // We will wait no more than this long for any new input.  If an
    // input stream fails to provide any input w/in this time, it's
    // subsequent data is at risk of being dropped if other streams
    // are producing.
    const int tardy = config["tardy"]; // msec

    zsock_signal(pipe, 0);      // signal ready

    // we use low-level zmq_poll instead of czmq's zpoller because we
    // want to know what inputs are availble for reading after each
    // poll.
    zmq_pollitem_t* poll_items = new zmq_pollitem_t[ninputs+1];

    // keep track of state for each input
    struct SockInfo {
        size_t index;
        zsock_t* sock;     // the socket
        int64_t seen_time; // wall clock time when last message arrived
        msg_header_t msg_header;
        zmsg_t* msg;       // most recent message popped from input queue or NULL
    };
        
    // load up control info
    std::vector<SockInfo> sockinfo(input.size());
    for (size_t ind=0; ind<ninputs; ++ind) {
        zsock_t* s = input[ind];
        sockinfo[ind] = {ind, s, 0, {0,0,EOT}, NULL};
        poll_items[ind] = {zsock_resolve(s), 0, ZMQ_POLLIN, 0};
    }
    poll_items[ninputs] = {zsock_resolve(pipe), 0, ZMQ_POLLIN, 0};

    uint64_t last_msg_time = 0;

    while (!zsys_interrupted) {

        const int ngot = zmq_poll(poll_items, ninputs+1, tardy);
        if (ngot < 0) {
            zsys_info("TPSorted poll failed: %s", zmq_strerror(errno));
            break;              // what else to do here?
        }

        if (poll_items[ninputs].revents & ZMQ_POLLIN) {
            zsys_info("TPSorted proxy got quit");
            break;
        }


        const int now = zclock_usecs();
        int nwith = 0;
        int nwait = 0;
        int64_t min_msg_time = EOT;
        size_t min_msg_ind = ninputs;

        // update queue states
        for (size_t ind=0; ind<ninputs; ++ind) {
            SockInfo& si = sockinfo[ind];

            if (poll_items[ind].revents & ZMQ_POLLIN) { // input has something
                assert(!si.msg); // we shouldn't be polling on what we already have
                si.seen_time = now;
                zmsg_t* msg = zmsg_recv(si.sock);
                auto header = msg_header(msg);
                if (header.tstart < last_msg_time) {
                    header_dump("tardy", header);
                    zmsg_destroy(&msg);
                    continue;
                }
                si.msg = msg;
                si.msg_header = header;
                poll_items[ind].events = 0; // don't poll this for now
                header_dump("recv", header);
            }

            if (si.msg) {
                ++nwith;
                if (si.msg_header.tstart < min_msg_time) {
                    min_msg_ind = ind;
                }
            }
            else {
                if (now - si.seen_time <= tardy) {
                    ++nwait;
                }
            }
        }

        if (nwait) {
            continue;
        }
        if (!nwith) {
            continue;
        }
        if (min_msg_ind == ninputs) { // impossible?
            continue;
        }



        // vacate min time message and set to poll corresponding queue for next time
        SockInfo& si = sockinfo[min_msg_ind];
        header_dump("send", si.msg_header);

        zmsg_t* msg = si.msg;
        si.msg = NULL;
        last_msg_time = si.msg_header.tstart;
        si.msg_header.tstart = EOT;
        poll_items[min_msg_ind].events = ZMQ_POLLIN;

        // send out message to all outputs
        int sends_left = output.size();
        for (auto s : output) {
            --sends_left;
            zmsg_t* tosend = msg;
            if (sends_left) {   // dup all but last
                tosend = zmsg_dup(msg);
            }
            zmsg_send(&tosend, s);
        }
        msg = NULL;
    }

    // clean up
    delete [] poll_items;
    for (size_t ind=0; ind < ninputs; ++ind) {
        SockInfo& si = sockinfo[ind];
        if (si.msg) {
            zmsg_destroy(&si.msg);
        }
    }
    for (auto s : input) {
        zsock_destroy(&s);
    }
    for (auto s : output) {
        zsock_destroy(&s);
    }
}


ptmp::TPSorted::TPSorted(const std::string& config)
    : m_actor(zactor_new(tpsorted_proxy, (void*)config.c_str()))
{

}

ptmp::TPSorted::~TPSorted()
{
    zsys_info("signal actor to quit");
    zsock_signal(zactor_sock(m_actor), 0); // signal quit

    zactor_destroy(&m_actor);
}
