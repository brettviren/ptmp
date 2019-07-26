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

PTMP_AGENT(ptmp::TPZipper, zipper)


using json = nlohmann::json;


// little helper to send out messages on a socket while not getting
// hung and insensitive to shutdown commands from the actor pipe if
// the output socket would otherwise want to block.
struct sender_t {
    std::vector<zsock_t*> outputs;
    zsock_t* pipe;
    sender_t(std::vector<zsock_t*> outputs, zsock_t* pipe) : outputs(outputs), pipe(pipe) {}

    // Send message to all outputs, destroying it.  Return 0 if okay,
    // -1 if we get shutdown while otherwise hung on a HWM'ed socket.
    int operator()(zmsg_t** msg) {
        for (auto& s : outputs) {
            while (!(zsock_events(s) & ZMQ_POLLOUT)) {
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
            zmsg_t* tosend = *msg;
            if (s != outputs.back()) {   // dup all but last
                tosend = zmsg_dup(*msg);
            }
            zmsg_send(&tosend, s); // destroys msg
        }
        return 0;               // okay
    }

    void destroy() {
        for (auto& s : outputs) {
            zsock_destroy(&s);
        }
        outputs.clear();
    }
    ~sender_t() { destroy(); }
};

// and minimizing buffer time.
struct meta_msg_t {
    // index into a vector of source sockets
    size_t source_index;
    // the real time at which this message is considered overdue based
    // on the sync time.
    ptmp::data::real_time_t toverdue;
    // The TPSet.tstart of the message
    ptmp::data::data_time_t tstart;
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
    std::vector<int> counts;
    std::vector<meta_msg_t> messages;

    // keep track of highest tstart time of returned punctual
    // messages.
    ptmp::data::data_time_t last_tstart{0};

    zipper_queue_t(int ninputs, int sync_ms) : counts(ninputs, 0), sync_ms(sync_ms) {}

    ~zipper_queue_t() {
        // clean up any leftovers.
        for (auto& mm : messages) {
            zmsg_destroy(&mm.msg); 
        }
    }

    // Receive a message from the socket and enqueue it.
    void recv(size_t ind, zsock_t* sock) {
        const ptmp::data::real_time_t trecv = ptmp::data::now();
        const ptmp::data::real_time_t tod = trecv + 1000*sync_ms;
        zmsg_t* msg = zmsg_recv(sock);
        zmsg_first(msg);        // msg id
        zframe_t* pay = zmsg_next(msg);
        ptmp::data::TPSet tpset;
        tpset.ParseFromArray(zframe_data(pay), zframe_size(pay));
        ptmp::data::data_time_t tstart = tpset.tstart();
        meta_msg_t mm = {ind, trecv+1000*sync_ms, tstart, msg};
        messages.push_back(mm);
        ++counts[ind];
    }

    // Return true if there is at least one message from all other
    // inputs besides the given one.
    bool have_all_other(size_t given) {
        const size_t n = counts.size();
        for (size_t ind=0; ind<n; ++ind) {
            if (ind == given) { continue; }
            if (counts[ind] == 0) {
                return false;
            }
        }
        return true;
    }

    // Process pending messages, fill punctual and tardy and keep any
    // leftovers.  Return suggested poll timeout.
    int process(std::vector<meta_msg_t>& punctual, std::vector<meta_msg_t> tardy) {
        if (messages.empty()) {
            return -1;
        }
        ptmp::data::real_time_t now = ptmp::data::now();
        // Order by increasing tstart.
        std::sort(messages.begin(), messages.end(), zq_lesser_t());
        const size_t nmessages = messages.size();

        // partition the ordered vector of messages

        // 1. tardy: any which have tstart < last_tstart
        std::vector<meta_msg_t>::iterator tardy_end = messages.begin(), mend = messages.end();
        while (tardy_end != mend and tardy_end->tstart < last_tstart) {
            --counts[tardy_end->source_index];
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
            --counts[it->source_index];
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
            if (!have_all_other(ready_end->source_index)) {
                break;
            }
            --counts[ready_end->source_index];
            last_tstart = ready_end->tstart;
            ++ready_end;
        }
        // size_t nready = std::distance(overdue_end, ready_end);
        // zsys_debug("nready: %ld", nready);


        // pack up
        tardy.insert(tardy.end(), messages.begin(), tardy_end);
        punctual.insert(punctual.end(), tardy_end, ready_end);

        // erase all but the leftovers for next time 
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
    }

    sender_t sender(ptmp::internals::perendpoint(config["output"].dump()), pipe);

    auto inputs = ptmp::internals::perendpoint(config["input"].dump());
    const size_t ninputs = inputs.size();

    // use low level poller to get all available in one go.  The +1 is for the pipe
    zmq_pollitem_t* pollitems = new zmq_pollitem_t[ninputs+1]; 

    for (size_t ind=0; ind<ninputs; ++ind) {
        pollitems[ind].socket = zsock_resolve(inputs[ind]);
        pollitems[ind].events = ZMQ_POLLIN;
    }
    pollitems[ninputs].socket = zsock_resolve(pipe);
    pollitems[ninputs].events = ZMQ_POLLIN;

    zsock_signal(pipe, 0);      // signal ready

    zipper_queue_t zq(ninputs, sync_ms);
    std::vector<size_t> total_counts(ninputs, 0);
    std::vector<size_t> tardy_counts(ninputs, 0);

    int wait_ms = -1;
    bool got_quit = false;
    while (!zsys_interrupted) {

        int rc = zmq_poll(pollitems, ninputs+1, wait_ms);
        //zsys_debug("zipper: poll returns %d", rc);

        if (rc < 0) {
            zsys_error("zipper: error on poll: %s", strerror(errno));
            goto cleanup;
        }

        if (rc > 0) {           // got something
            if (pollitems[ninputs].revents & ZMQ_POLLIN) {
                //zsys_debug("zipper: got quit on input");
                got_quit = true;
                goto cleanup;
            }

            // slurp in all available
            for (size_t ind=0; ind<ninputs; ++ind) {
                if (pollitems[ind].revents & ZMQ_POLLIN) {
                    //zsys_debug("got input on %d", ind);
                    zq.recv(ind, inputs[ind]);
                }
            }
        }

        std::vector<meta_msg_t> punctual, tardy;
        wait_ms = zq.process(punctual, tardy);
        // zsys_debug("punctual:%ld, tardy:%ld wait:%d",
        //            punctual.size(), tardy.size(), wait_ms);

        // dispatch any tardy messages per policy
        for (auto& mm : tardy) {
            ++tardy_counts[mm.source_index];
            ++total_counts[mm.source_index];
            if (drop_tardy) {
                zmsg_destroy(&mm.msg);
            }
            else {           // this destroys output ordering contract
                int rc = sender(&mm.msg);
                if (rc == -1) {
                    zsys_debug("zipper: got quit on output");
                    got_quit = true;
                    goto cleanup;
                }
            }
        }

        for (auto& mm : punctual) {
            ++total_counts[mm.source_index];
            int rc = sender(&mm.msg);
            if (rc == -1) {
                zsys_debug("zipper: got quit on output");
                got_quit = true;
                goto cleanup;
            }
        }
    }

  cleanup:

    zsys_debug("zipper: finishing");
    delete [] pollitems;
    for (size_t ind=0; ind<ninputs; ++ind) {
        zsys_debug("zipper: source %ld: %ld tardy out of %ld recved",
                   ind, tardy_counts[ind], total_counts[ind]);
        zsock_destroy(&inputs[ind]);
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
