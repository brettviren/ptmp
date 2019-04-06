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
    // zsys_warning("%s: #%d from %d t=%.1f",
    //              s.c_str(), h.count, h.detid, 1e-6*(h.tstart-t0));
    zsys_info("%s: count:%-4d detid:%-2d tstart:%-8ld",
              s.c_str(), h.count, h.detid, h.tstart);

}

// keep track of state for each input
struct SockInfo {
    size_t index;
    zsock_t* sock;     // the socket
    zpoller_t* poller; 
    int64_t seen_time; // wall clock time when last message arrived
    msg_header_t msg_header;
    zmsg_t* msg;       // most recent message popped from input queue or NULL
};

// Get the first prompt (not tardy) message.   Return true if successful.
bool recv_prompt(SockInfo& si, uint64_t last_msg_time)
{
    void* which = zpoller_wait(si.poller, 0);
    if (!which) {
        return false;
    }

    zmsg_t* msg = zmsg_recv(si.sock);
    if (!msg) {                 // interrupted
        return false;
    }
    auto header = msg_header(msg);
    //header_dump("precv", header);
    if (header.tstart < last_msg_time) {
        // Message is tardy.
        // fixme: may want to notify somehow besides dumping the header.
        header_dump("tardy", header);
        zmsg_destroy(&msg);
        return recv_prompt(si, last_msg_time);
    }
    si.msg = msg;
    si.msg_header = header;
    return true;
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

        
    // load up control info
    std::vector<SockInfo> sockinfo;
    for (size_t ind=0; ind<ninputs; ++ind) {
        zsock_t* s = input[ind];
        sockinfo.push_back({ind, s, zpoller_new(s, NULL), 0, {0,0,EOT}, NULL});
    }
    zpoller_t* pipe_poller = zpoller_new(pipe, NULL);

    uint64_t last_msg_time = 0;

    int wait_time = tardy;

    while (!zsys_interrupted) {

        void* which = zpoller_wait(pipe_poller, wait_time);
        if (which) {
            zsys_info("TPSorted proxy got quit");
            break;
        }

        const int now = zclock_usecs();
        int nwait = 0;          // how many we lack but need to wait for

        int64_t min_msg_time = EOT;
        size_t min_msg_ind = ninputs;

        for (size_t ind=0; ind<ninputs; ++ind) {
            SockInfo& si = sockinfo[ind];
            if (!si.msg) {

                void* which = zpoller_wait(si.poller, 0);
                if (!which) {
                    int wait_needed = now - si.seen_time;
                    if (wait_needed <= tardy) {
                        ++nwait;
                    }
                    continue;
                }
                si.seen_time = now;
                recv_prompt(si, last_msg_time);
                
                // zmsg_t* msg = zmsg_recv(si.sock);
                // auto header = msg_header(msg);
                // header_dump("precv", header);
                // if (header.tstart < last_msg_time) {
                //     header_dump("tardy", header);
                //     zmsg_destroy(&msg);
                //     // fixme: we should try more than just once.
                //     // Here, this should loop on this input socket
                //     // until we get a fresh message or the input is
                //     // drained.
                //     continue;
                // }
                // si.msg = msg;
                // si.msg_header = header;
            }

        } // get inputs
        

        if (nwait) {
            //zsys_info("waiting on %d", nwait);
            continue;
        }

        // find min time message
        for (size_t ind=0; ind<ninputs; ++ind) {
            SockInfo& si = sockinfo[ind];
            if (si.msg) {
                if (si.msg_header.tstart < min_msg_time) {
                    min_msg_time = si.msg_header.tstart;
                    min_msg_ind = ind;
                }
            }
        }

        if (min_msg_ind == ninputs) {
            //zsys_info("got none");
            continue;
        }

        // vacate min time message and set to poll corresponding queue for next time
        SockInfo& si = sockinfo[min_msg_ind];
        // header_dump("psend", si.msg_header);

        zmsg_t* msg = si.msg;
        si.msg = NULL;
        last_msg_time = si.msg_header.tstart;
        si.msg_header.tstart = EOT;

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
    } // main loop

    // clean up
    zpoller_destroy(&pipe_poller);
    for (size_t ind=0; ind < ninputs; ++ind) {
        SockInfo& si = sockinfo[ind];
        if (si.msg) {
            zmsg_destroy(&si.msg);
        }
        zpoller_destroy(&si.poller);
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
