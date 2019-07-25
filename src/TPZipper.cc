/**
   The zipper (ne' sorted) synchronizes a number of input streams into
   one output stream.  Each input is assumed to be strictly ordered by
   their TPSet.tstart data times and ouput stream is likewise so
   ordered.

   A real-time/system-clock synchronization buffer time is applied in
   order to allow for variations in the pacing of the input streams.
   The most recently received message from each input is held for no
   longer than this sync time before sending out.

   Any input message with a TPSet.tstart data time which is smaller
   than the TPSet.tstart data time of the last output message is
   dropped in order to preserve output tstart order.  However, this
   may be violated if the tardy policy is "send" instead of the
   default "drop".

 */

#include "ptmp/api.h"
#include "ptmp/internals.h"
#include "ptmp/factory.h"
#include "ptmp/actors.h"

#include <json.hpp>
#include <vector>

PTMP_AGENT(ptmp::TPZipper, zipper)


using json = nlohmann::json;

struct source_info_t {
    size_t index;               // index in vector of sources
    zsock_t* sock;              // the socket....

    zmsg_t* msg{NULL};          // most recent message
    ptmp::data::TPSet tpset;    // unpacked payload

    ptmp::data::real_time_t trecv;
    uint64_t nrecv{0}, ntardy{0};     // stats

    zmsg_t* release() {         // caller takes ownership;
        zmsg_t* ret = msg;
        msg = NULL;
        tpset.Clear();
        return ret;
    }

    void recv() {
        if (msg) {
            zsys_warning("zipper: source %d discarding message", index);
            zmsg_destroy(&msg);
        }
        msg = zmsg_recv(sock);
        ++nrecv;
        trecv = ptmp::data::now();
        zmsg_first(msg);        // msg id
        zframe_t* pay = zmsg_next(msg);
        tpset.ParseFromArray(zframe_data(pay), zframe_size(pay));
    }
    ~source_info_t() {
        if (msg) {
            zmsg_t* m = release();
            zmsg_destroy(&m);
        }
        if (sock) {
            zsock_destroy(&sock);
        }
    }
        
};

// The priority queue has three time related aspects:
//
// 1. it is ordered by tstart time.  Smallest tstart has highest priority.
// 2. it is drained by recved time in priority order such that no remaining entries have tardy trecv values.
//
struct si_greater_t {
    bool operator()(const source_info_t* a, const source_info_t* b) {
        // give priority to smaller times
        return a->tpset.tstart() > b->tpset.tstart();
    }
};

class ZipperQueue 
{
    // the priority queue is kept as a reverse sorted vector.  The
    // *back* of the vector will have highest priority (smallest
    // tstart).
    std::vector<source_info_t*> m_queue;
public:
    // return the number of high priority elements counted up to the
    // least priority element with trecv >= tsel.  That is, if tsel is
    // "now + tardy" then you should pop and send the this number of
    // waiting messages.
    size_t ready_to_go(ptmp::data::real_time_t tsel) const
    {
        auto it = m_queue.rbegin();
        while (it != m_queue.rend()) {
            if ( (*it)->tpset.tstart() > tsel) {
                break;
            }
            ++it;
        }
        return it - m_queue.rbegin();
    }

    // Return the oldest recved time
    ptmp::data::real_time_t oldest_trecv() const {
        ptmp::data::real_time_t tret = -1;
        for (const auto& si : m_queue) {
            const ptmp::data::real_time_t t = si->trecv;
            if (t > tret) tret = t;
        }
        return tret;
    }

    // eumulate queue interface
    bool empty() {
        return m_queue.empty();
    }
    source_info_t* top() {
        return m_queue.back();
    }
    void pop() {
        m_queue.pop_back();
    }
    void push(source_info_t* si) {
        m_queue.push_back(si);
        std::sort(m_queue.begin(), m_queue.end(), si_greater_t());
    }
};

struct sender_t {
    std::vector<zsock_t*> outputs;
    sender_t(std::vector<zsock_t*> outputs) : outputs(outputs) {}
    // send message to all outputs and destroy it.  
    void operator()(zmsg_t** msg) {
        for (auto& s : outputs) {
            zmsg_t* tosend = *msg;
            if (s != outputs.back()) {   // dup all but last
                tosend = zmsg_dup(*msg);
            }
            zmsg_send(&tosend, s); // destroys msg
        }
    }
    void destroy() {
        for (auto& s : outputs) {
            zsock_destroy(&s);
        }
        outputs.clear();
    }
    ~sender_t() { destroy(); }

    bool blocked() {
        for (size_t ind=0; ind < outputs.size(); ++ind) {
            if (! (zsock_events(outputs[ind]) & ZMQ_POLLOUT)) {
                // zsys_debug("zipper: blocked on output %d", ind);
                return true;
            }
        }
        return false;
    }

};

// The actor function
void ptmp::actor::zipper(zsock_t* pipe, void* vargs)
{
    auto config = json::parse((const char*) vargs);

    std::string name = "zipper";
    if (config["name"].is_string()) {
        name = config["name"];
    }
    ptmp::internals::set_thread_name(name);

    // - sync time :: Typical network transport latency is 20-50us.
    //     Minimal message processing code incurs <1ms latency.
    //     Maximum variations should be on same order.  Drift time
    //     variation is 2.25ms (SP) to 10ms (DP).
    int sync_ms = 10;
    if (config["tardy"].is_number()) { // obsolete param name
        sync_ms = config["tardy"];
    }
    else if (config["sync_time"].is_number()) {
        sync_ms = config["sync_time"];
    }

    // - tardy policy :: an option string.  Any message arriving later
    //     than a peer by more than the tardy time is subject to the
    //     tardy policy.  Default policy is "drop" which will cause
    //     late messages to be discarded.  Alternative policy is
    //     "send" which will immediately send out late messages, thus
    //     destroying output ordering requirements, and violating
    //     general data flow assumptions.
    bool drop_tardy = true;
    if (config["tardy_policy"].is_string() and config["tardy_policy"].get<std::string>() == "send") {
        drop_tardy = false;
    }

    // A source info flip-flops between being polled and being in the queue.
    ZipperQueue si_queue;
    zpoller_t* poller = zpoller_new(pipe, NULL);

    sender_t sender(ptmp::internals::perendpoint(config["output"].dump()));
    auto inputs = ptmp::internals::perendpoint(config["input"].dump());
    const size_t ninputs = inputs.size();
    std::unordered_map<void*, source_info_t*> source_infos;
    for (size_t ind=0; ind<ninputs; ++ind) {
        zsock_t* sock = inputs[ind];
        source_infos[(void*)sock] = new source_info_t{ind, sock};
        zpoller_add(poller, sock);
    }


    zsock_signal(pipe, 0);      // signal ready


    ptmp::data::data_time_t last_tstart = 0;
    int wait_time_ms = -1;
    bool got_quit = false;
    while (!zsys_interrupted) {

        void* which = zpoller_wait(poller, wait_time_ms);

        if (zpoller_terminated(poller)) {
            goto cleanup;
        }
        if (which == pipe) {
            got_quit = true;
            goto cleanup;
        }


        if (which) {          // got input

            auto si = source_infos[which];
            si->recv();

            // dispatch as tardy or enqueue

            while (si->tpset.tstart() < last_tstart) {
                ++ si->ntardy;
                zmsg_t* msg = si->release();
                if (drop_tardy) { // enact correct tardy policy
                    zmsg_destroy(&msg);
                }
                else {          // destroy output time ordering,

                    // fixme: kludge to avoid output block stopping shutdown
                    while (sender.blocked()) {
                        if (zsock_events(pipe) & ZMQ_POLLIN) {
                            got_quit = true;
                            goto cleanup;
                        }
                        zclock_sleep(1); // ms
                    }
                    sender(&msg);
                }

                // Go again if this input has more.
                if (zsock_events(which) & ZMQ_POLLIN) {
                    si->recv(); 
                    continue;
                }

                break;

            } // loop can leave si empty or populated

            if (si->msg) {
                si_queue.push(si);
                zpoller_remove(poller, si->sock);
            }

        }
        

        // process queue.

        {
            const ptmp::data::real_time_t now = ptmp::data::now();
            const ptmp::data::real_time_t tsel = now + sync_ms*1000;
            size_t ntopop = si_queue.ready_to_go(tsel);
            while (ntopop) {
                --ntopop;
                auto si = si_queue.top();

                si_queue.pop();
                zpoller_add(poller, si->sock);

                last_tstart = si->tpset.tstart();
                zmsg_t* msg = si->release();

                // fixme: kludge to avoid output block stopping shutdown
                while (sender.blocked()) {
                    if (zsock_events(pipe) & ZMQ_POLLIN) {
                        got_quit = true;
                        goto cleanup;
                    }
                    zclock_sleep(1); // ms
                }

                sender(&msg);
                si = NULL;
            }

            if (si_queue.empty()) {
                wait_time_ms = -1; // wake when we have input
            }
            else {
                // We have queued data so next time sleep no more than
                // would make the oldest waiting message late.
                wait_time_ms = sync_ms - (now - si_queue.oldest_trecv())/1000;
            }
        }

        continue;               // back to polling...
    }


  cleanup:

    zsys_debug("zipper: finishing");
    zpoller_destroy(&poller);
    for (auto& siit : source_infos) {
        auto& si = siit.second;
        zsys_debug("zipper: %d: %d tardy out of %d recved",
                   si->index, si->ntardy, si->nrecv);
        delete si;
    }
    sender.destroy();
    if (got_quit) {
        return;
    }
    zsys_debug("zipper: waiting for quit");
    zsock_wait(pipe);

}

ptmp::TPZipper::TPZipper(const std::string& config)
    : m_actor(zactor_new(ptmp::actor::zipper, (void*)config.c_str()))
{

}

ptmp::TPZipper::~TPZipper()
{
    zsys_debug("zipper: signaling done");
    zsock_signal(zactor_sock(m_actor), 0); // signal quit
    zactor_destroy(&m_actor);
}
