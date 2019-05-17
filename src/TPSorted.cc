#include "ptmp/api.h"

#include <czmq.h>
#include <json.hpp>

#include <algorithm>
#include <vector>
#include <unordered_map>


using json = nlohmann::json;

// TPSorted interprets the list of endpoints as being for individual
// sockets while normally, one socket may have many endpoints.  So, we
// do a little private jostling of the config into many confgs.
static
std::vector<zsock_t*> make_sockets(json jcfg)
{
    const std::vector<std::string> bcs{"bind","connect"};
    std::vector<zsock_t*> ret;
    for (auto bc : bcs) {
        for (auto jaddr : jcfg[bc]) {
            json cfg = {
                { "socket", {
                        { "type", jcfg["type"] },
                        { "hwm", jcfg["hwm"] },
                        { bc, jaddr } } } };
            std::string cfgstr = cfg.dump();
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

// keep EOT fitting in a signed int64
static const uint64_t EOT = 0x7FFFFFFFFFFFFFFF;

// keep track of state for each input
struct SockInfo {
    size_t index;
    zsock_t* sock;     // the socket
    zpoller_t* poller; 
    int64_t seen_time; // wall clock time when last message arrived
    msg_header_t msg_header;
    zmsg_t* msg;       // most recent message popped from input queue or NULL
    uint64_t nrecved;
    uint64_t ntardy;
};

// Get the next message.   Return true if successful.
static
bool recv_prompt(SockInfo& si, int64_t last_msg_time, bool drop_tardy)
{
    assert(!si.msg);            // we are not allowed to overwrite
    void* which = zpoller_wait(si.poller, 0);
    if (!which) {
        return false;
    }

    zmsg_t* msg = zmsg_recv(si.sock);
    if (!msg) {                 // interrupted
        return false;
    }
    ++si.nrecved;

    auto header = msg_header(msg);
    bool tardy = last_msg_time < EOT and header.tstart < EOT and header.tstart < last_msg_time;
    if (tardy) {
        ++si.ntardy;
        zsys_debug("tardy: msg=%ld last=%ld %d/%d",
                   header.tstart, last_msg_time, si.ntardy, si.nrecved); 
    }
    if (drop_tardy and tardy) {
        // Message is tardy and policy is drop.
        header_dump("tardy", header);
        zmsg_destroy(&msg);
        // next one might be on time
        return recv_prompt(si, last_msg_time, drop_tardy);
    }
    si.msg = msg;
    si.msg_header = header;
    //header_dump("precv", header);
    return true;
}

// The actor function
void tpsorted_proxy(zsock_t* pipe, void* vargs)
{

    auto config = json::parse((const char*) vargs);
    auto input = make_sockets(config["input"]["socket"]);
    auto output = make_sockets(config["output"]["socket"]);

    auto tardy_policy = config["tardy_policy"];
    bool drop_tardy = true;
    if (tardy_policy.is_string() and tardy_policy == "send") {
        drop_tardy = false;
    }

    const size_t ninputs = input.size();

    // We will wait no more than this long for any new input.  If an
    // input stream fails to provide any input w/in this time, it's
    // subsequent data is at risk of being dropped if other streams
    // are producing.
    const int tardy_ms = config["tardy"]; // msec

    zsock_signal(pipe, 0);      // signal ready

    zsys_info("starting tpsorted proxy with tardy policy: %s",
              (drop_tardy?"drop":"pass"));
        
    // load up control info
    std::vector<SockInfo> sockinfo;
    for (size_t ind=0; ind<ninputs; ++ind) {
        zsock_t* s = input[ind];
        sockinfo.push_back({ind, s, zpoller_new(s, NULL), 0,
                            {0,0,EOT}, NULL, 0,0});
    }
    zpoller_t* pipe_poller = zpoller_new(pipe, NULL);

    uint64_t last_msg_time = EOT;

    int wait_time_ms = 0;

    while (!zsys_interrupted) {


        void* which = zpoller_wait(pipe_poller, wait_time_ms);
        if (which) {
            zsys_info("TPSorted proxy got quit");
            break;
        }

        const int64_t now = zclock_usecs();

        int nwait = 0;          // how many we lack but need to wait for

        for (size_t ind=0; ind<ninputs; ++ind) {
            SockInfo& si = sockinfo[ind];
            if (!si.msg) {
                void* which = zpoller_wait(si.poller, 0);
                if (which) {
                    si.seen_time = now;
                    recv_prompt(si, last_msg_time, drop_tardy);
                    continue;
                }
                if (si.seen_time == 0) {
                    continue; // never yet seen
                }

                const int since_ms = (now - si.seen_time)/1000;
                const int wait_needed_ms = tardy_ms - since_ms;
                if (wait_needed_ms >= 0 ) {
                    // zsys_debug("wait for #%d %d ms (now=%ld us, seen=%ld us)",
                    //            ind, wait_needed_ms, now, si.seen_time);
                    if (wait_needed_ms <= tardy_ms) {
                        wait_time_ms = std::max(wait_needed_ms, wait_time_ms);
                        ++nwait;
                    }
                }
            }

        } // get inputs
        
        if (nwait == ninputs) {
            wait_time_ms = tardy_ms;
            zsys_debug("all input waiting, wait for %d ms", wait_time_ms);
            continue;
        }
        if (nwait) {
            wait_time_ms = std::min(wait_time_ms, tardy_ms);
            // don't spin
            if (wait_time_ms == 0) wait_time_ms = 1;
            zsys_debug("waiting on %d input for at most %d ms", nwait, wait_time_ms);
            continue;
        }

        // find min time message
        int64_t min_msg_time = EOT;
        size_t min_msg_ind = ninputs;
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
            wait_time_ms = tardy_ms;
            zsys_debug("no input, waiting %d ms", wait_time_ms);

            for (size_t ind=0; ind<ninputs; ++ind) {
                SockInfo& si = sockinfo[ind];
                if (si.msg) {
                    zsys_debug("\t%d %ld %ld",
                               ind, si.msg_header.tstart, min_msg_time);
                }
            }

            continue;
        }

        // zsys_debug("got input on %d at %ld", min_msg_ind, min_msg_time);

        // got something, assume there is more so next time around,
        // don't wait at all.
        wait_time_ms = 0;

        // vacate min time message and set to poll corresponding queue
        // for next time
        SockInfo& si = sockinfo[min_msg_ind];
        // header_dump("psend", si.msg_header);
        // zsys_info("\ttardy:%d/%d", si.ntardy, si.nrecved);

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
        zsys_debug("%d: %d tardy out of %d recved",
                   ind, si.ntardy, si.nrecved);
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
