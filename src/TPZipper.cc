/**
   The zipper synchronizes a number of input streams of TPSet messages
   into one output stream.  Each input is assumed to be strictly
   ordered by their TPSet.tstart data times and the ouput stream is
   likewise so ordered.

   A real-time/data-time synchronization buffer time (sync time,
   measured in real time) is applied in order to allow for variations
   in the pacing of the input streams.
   
   Messages should be delayed no longer than this sync time.  A
   message will only see this maximum latency when at least one source
   besides the one providig the message has not yet provided a message
   with a yet later (larger) tstart.

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
#include <unordered_map>

PTMP_AGENT(ptmp::TPZipper, zipper)


using json = nlohmann::json;


// little helper to send out messages on a socket while not getting
// hung and insensitive to shutdown commands from the actor pipe if
// the output socket would otherwise want to block.
struct sender_t {
    zsock_t* output;
    zsock_t* pipe;
    sender_t(zsock_t* output, zsock_t* pipe) : output(output), pipe(pipe) {}

    // Send message to all outputs, destroying it.  Return 0 if okay,
    // -1 if we get shutdown while otherwise hung on a HWM'ed socket.
    int operator()(zmsg_t** msg) {
        while (!(zsock_events(output) & ZMQ_POLLOUT)) {
            // if we would block, sleep for a bit and check if we
            // have shutdown command so we avoid hang.
            zclock_sleep(1); // ms
            if (zsock_events(pipe) & ZMQ_POLLIN) {
                zmsg_destroy(msg);
                return -1;
            }
            continue;
        }
        // safe to send to this socket without wait.
        zmsg_send(msg, output);
        return 0;               // okay
    }

    void destroy() {
        zsock_destroy(&output);
    }
    ~sender_t() { destroy(); }
};

// and minimizing buffer time.
struct meta_msg_t {
    // the real time at which this message is considered overdue based
    // on the sync time.
    ptmp::data::real_time_t toverdue;
    // The TPSet.tstart of the message
    ptmp::data::data_time_t tstart;
    // Use detid as a stream/source identifier.
    int detid;
    // The message
    zmsg_t* msg;
};

// Used to order a vector of meta_msg_t's in increasing tstart.
struct zq_lesser_t {
    bool operator()(const meta_msg_t& a, const meta_msg_t& b) {
        return a.tstart < b.tstart;
    }
};

// A helper which receives and collects messages and can process this
// collection into ordered vectors of tardy, punctual and leftovers.
struct zipper_queue_t {
    int sync_ms;

    // keep track of how many messages are currently held from each
    // source.  
    std::unordered_map<int, int> counts;

    // Keep track of last TPSet.count() seen to detect dropped messages
    std::unordered_map<int, int> last_in_count;

    // Expected number of sources
    int nsources;

    // A home made priority queue of messages.
    std::vector<meta_msg_t> messages;
    bool messages_dirty{true};

    // keep track of highest tstart time of returned punctual
    // messages.
    ptmp::data::data_time_t last_tstart{0};

    // A temporary, reuse it for some optimization.
    ptmp::data::TPSet tpset;

    zipper_queue_t(int ninputs, int sync_ms) : nsources(ninputs), sync_ms(sync_ms) {}

    ~zipper_queue_t() {
        // clean up any leftovers.
        for (auto& mm : messages) {
            zmsg_destroy(&mm.msg); 
        }
    }

    // Receive a message from the socket and enqueue it.  We want to
    // hold onto the msg for sending out so we have to do the
    // unpacking ourselves instead of relying on stuff in ptmp::internal
    //void recv(zsock_t* sock) {
    void recv(zmsg_t* msg) {    
        const ptmp::data::real_time_t trecv = ptmp::data::now();
        const ptmp::data::real_time_t tod = trecv + 1000*sync_ms;

        zframe_t* fid = zmsg_first(msg);        // msg id
        int version = *(int*)zframe_data(fid);

        assert (version == 0);
        zframe_t* pay = zmsg_next(msg);
        tpset.ParseFromArray(zframe_data(pay), zframe_size(pay));
        const ptmp::data::data_time_t tstart = tpset.tstart();
        const int detid = tpset.detid();

        {
            const auto this_count = tpset.count();
            auto& last_count = last_in_count[detid];
            if (last_count == 0) {
                last_count = this_count;
            }
            else {
                const int n_missed = this_count - last_count - 1;
                if (n_missed > 0) {
                    zsys_warning("zipper: detid: 0x%x lost %d", detid, n_missed);
                }
                last_count = this_count;
            }
        }

        meta_msg_t mm = {trecv+1000*sync_ms, tstart, detid, msg};
        messages.push_back(mm);
        messages_dirty = true;
        ++counts[detid];
    }

    // Return true if all other sources have non-zero messages queued.
    bool have_all_other(size_t given) {
        int nother = 1; // counting the given
        for (const auto& cc : counts) {
            if (cc.first == given) {
                continue;
            }
            if (cc.second == 0) {
                return false;
            }
            ++nother;
        }
        return nother == nsources;
    }

    // Process pending messages, fill punctual and tardy and keep any
    // leftovers.  Return suggested poll timeout.
    int process(std::vector<meta_msg_t>& punctual, std::vector<meta_msg_t>& tardy) {
        if (messages.empty()) {
            return -1;
        }
        ptmp::data::real_time_t now = ptmp::data::now();
        // Order by increasing tstart.
        if (messages_dirty) {
            std::sort(messages.begin(), messages.end(), zq_lesser_t());
            messages_dirty = false;
        }
        const size_t nmessages = messages.size();

        // partition the ordered vector of messages

        // 1. tardy: any which have tstart < last_tstart
        std::vector<meta_msg_t>::iterator tardy_end = messages.begin(), mend = messages.end();
        while (tardy_end != mend and tardy_end->tstart < last_tstart) {
            --counts[tardy_end->detid];
            ++tardy_end;
        }

        // 2. overdue: highest tstart which is overdue
        std::vector<meta_msg_t>::iterator overdue_end = mend;
        while (overdue_end > tardy_end) {
            --overdue_end;
            if ( overdue_end->toverdue < now) {
                //zsys_debug("found overdue dt=%ld", now - overdue_end->toverdue);
                ++overdue_end;
                break;
            }
        }
        for (auto it=tardy_end; it!=overdue_end; ++it) {
            --counts[it->detid];
            last_tstart = it->tstart;
        }
        //size_t noverdue = std::distance(tardy_end, overdue_end);
        //zsys_debug("noverdue: %ld", noverdue);

        // 3. ready: from the remaining, we can send addtional output
        // even if they are not yet overdue but only if there are no
        // empty inputs so that we know for sure fresh future data
        // won't have yet earlier tstarts.
        std::vector<meta_msg_t>::iterator ready_end = overdue_end;
        while (ready_end != mend) {
            if (!have_all_other(ready_end->detid)) {
                break;
            }
            --counts[ready_end->detid];
            last_tstart = ready_end->tstart;
            ++ready_end;
        }
        // size_t nready = std::distance(overdue_end, ready_end);
        // zsys_debug("nready: %ld", nready);


        // pack up
        tardy.insert(tardy.end(), messages.begin(), tardy_end);
        punctual.insert(punctual.end(), tardy_end, ready_end);

        // erase those to be returned, keep leftovers for next time 
        messages.erase(messages.begin(), ready_end);
        //zsys_debug("nleftover: %ld", messages.size());
        if (messages.empty()) {
            return -1;
        }
        ptmp::data::real_time_t toverdue=-1;
        for (size_t ind=0; ind<messages.size(); ++ind) {
            if (toverdue<0) toverdue = messages[ind].toverdue;
            else toverdue = std::min(toverdue, messages[ind].toverdue);
        }
        return std::max(0, (int)((toverdue - now)/1000));
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
    int verbose = 0;
    if (config["verbose"].is_number()) {
        verbose = config["verbose"];
    }

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
    if (sync_ms <= 0) {
        zsys_error("sync_time must be positive definite number of ms");
        throw std::runtime_error("sync_time must be positive definite number of ms");
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
        zsys_warning("zipper: user wants to send tardy messages, this destroys output ordering contract");
    }

    sender_t sender(ptmp::internals::endpoint(config["output"].dump()), pipe);

    auto input = ptmp::internals::endpoint(config["input"].dump());
    auto jinsock = config["input"]["socket"];
    const int ninputs = jinsock["bind"].size() + jinsock["connect"].size();

    zsock_signal(pipe, 0);      // signal ready

    // This will all fall down if sources produce unexpected detids!
    // Should maybe add configuration to explicitly give expected detids.
    zipper_queue_t zq(ninputs, sync_ms);

    struct counters_t {
        uint64_t total{0}, tardy{0};
    };
    std::unordered_map<int, counters_t> counters;

    zpoller_t* poller = zpoller_new(pipe, input, NULL);

    int loop_count{0};
    ptmp::data::real_time_t start_time = ptmp::data::now();
    ptmp::data::real_time_t t1, t2, time_poll{0}, time_recv{0}, time_zip{0}, time_tardy{0}, time_send{0};

    int wait_ms = -1;
    bool got_quit = false;
    while (!zsys_interrupted) {

        ++loop_count;

        t1 = ptmp::data::now();

        ptmp::internals::microsleep(100);
        //if (wait_ms == 0) { wait_ms = 1; }
        void *which = zpoller_wait(poller, wait_ms);
        //zsys_debug("zipper: wait_ms=%d, which=%lx", wait_ms, which);

        t2 = ptmp::data::now();
        time_poll += t2-t1;
        t1=t2;

        if (which) {
            //zsys_debug("zipper: got input");
            if (which == pipe) {
                zsys_debug("zipper: got quit on input");
                got_quit = true;
                goto cleanup;
            }
            if (which == input) {
                do {
                    zmsg_t* msg = zmsg_recv(input);
                    zq.recv(msg);
                } while (zsock_events(input) & ZMQ_POLLIN);
            }
        }
        else {
            // if (zpoller_expired(poller)) {
            //     zsys_debug("zipper: poll expired");
            //     goto cleanup;
            // }
            if (zpoller_terminated(poller)) {
                zsys_debug("zipper: poll terminated");
                goto cleanup;
            }
        }

        t2 = ptmp::data::now();
        time_recv += t2-t1;
        t1=t2;

        // do the zipping
        std::vector<meta_msg_t> punctual, tardy;
        wait_ms = zq.process(punctual, tardy);
        if (verbose>1) {
            if (punctual.size() or tardy.size()) {
                zsys_debug("punctual:%ld, tardy:%ld wait:%d ms",
                           punctual.size(), tardy.size(), wait_ms);
            }
        }

        t2 = ptmp::data::now();
        time_zip += t2-t1;
        t1=t2;

        // dispatch any tardy messages per policy
        for (auto& mm : tardy) {
            auto& c = counters[mm.detid];
            ++c.tardy;
            ++c.total;
            if (drop_tardy) {
                zmsg_destroy(&mm.msg);
            }
            else { // warning: not recomended option, destroys output ordering contract
                int rc = sender(&mm.msg);
                if (rc == -1) {
                    zsys_debug("zipper: got quit on output");
                    got_quit = true;
                    goto cleanup;
                }
            }
        }

        t2 = ptmp::data::now();
        time_tardy += t2-t1;
        t1=t2;

        // dispatch normal output
        for (auto& mm : punctual) {
            ++counters[mm.detid].total;
            int rc = sender(&mm.msg);
            if (rc == -1) {
                zsys_debug("zipper: got quit on output");
                got_quit = true;
                goto cleanup;
            }
        }

        t2 = ptmp::data::now();
        time_send += t2-t1;
        t1=t2;

        if (loop_count %100000 == 1) {
            double n = loop_count;
            double t = ptmp::data::now() - start_time;
            zsys_debug("poll: %.1fus (%.1f%%) recv: %.1fus (%.1f%%) "
                       "zip: %.1fus (%.1f%%) tardy: %.1fus (%.1f%%) send: %.1fus (%.1f%%)",
                       time_poll/n,
                       100.0*time_poll/t,
                       time_recv/n,
                       100.0*time_recv/t,
                       time_zip/n,
                       100.0*time_zip/t,
                       time_tardy/n,
                       100.0*time_tardy/t,
                       time_send/n,
                       100.0*time_send/t);
        }

    }

  cleanup:

    zsys_debug("zipper: finishing");
    for (const auto& c : counters) {
        zsys_debug("zipper: source %d: %ld tardy out of %ld recved",
                   c.first, c.second.tardy, c.second.total);
    }
    zsock_destroy(&input);
    sender.destroy();
    if (got_quit) {
        zsys_debug("zipper: got quit");
        return;
    }
    zsys_debug("zipper: waiting for quit");
    zsock_wait(pipe);
    zsys_debug("zipper: actor done");
}


ptmp::TPZipper::TPZipper(const std::string& config)
    : m_actor(zactor_new(ptmp::actor::zipper, (void*)config.c_str()))
{

}

ptmp::TPZipper::~TPZipper()
{
    zsys_debug("zipper: signaling done");
    zsock_signal(zactor_sock(m_actor), 0); // signal quit
    zclock_sleep(1000);
    zsys_debug("zipper: destroying actor");
    zactor_destroy(&m_actor);
}
