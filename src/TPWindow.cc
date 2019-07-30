#include "ptmp/api.h"
#include "ptmp/internals.h"
#include "ptmp/factory.h"
#include "ptmp/actors.h"

#include "json.hpp"

#include <queue>

PTMP_AGENT(ptmp::TPWindow, window)


using json = nlohmann::json;


// lil helper for window operations.  A window is defined by
//
// - wind :: an index locating the window absolutely in time.  The start of wind=0 is at toff.
// 
// - tspan :: the duration of the window in HW clock tricks
//
// - toff :: an offset in HW clock ticks from if t=0 was a boundary.
//
struct time_window_t {

    ptmp::data::data_time_t wind;
    const ptmp::data::data_time_t toff, tspan;

    time_window_t(ptmp::data::data_time_t tspan, ptmp::data::data_time_t toff, ptmp::data::data_time_t wind = 0)
        : tspan(tspan), toff(toff%tspan), wind(wind) { }

    // return the begin time of the window
    ptmp::data::data_time_t tbegin() const {
        return wind*tspan + toff;
    }

    // Return true if t is in time window
    bool in(ptmp::data::data_time_t t) const {
        const ptmp::data::data_time_t tbeg = tbegin();
        return tbeg <= t and t < tbeg+tspan;
    }

    int cmp(ptmp::data::data_time_t t) const {
        const ptmp::data::data_time_t tbeg = tbegin();
        if (t < tbeg) return -1;
        if (t >= tbeg+tspan) return +1;
        return 0;
    }

    void set_bytime(ptmp::data::data_time_t t) {
        wind = (t-toff) / tspan;
    }
    void advance() {
        ++wind;
    }

};

struct tp_greater_t {
    bool operator()(const ptmp::data::TrigPrim& a, const ptmp::data::TrigPrim& b) {
        // give priority to smaller times
        return a.tstart() > b.tstart();
    }
};

// a priority queue of TPs ordered by tstart that also keeps track of
// the highest tstart its seen.
class priority_tp_span_t {
public:
    typedef ptmp::data::TrigPrim value_type;
    typedef typename std::vector<value_type> collection_type;

    ptmp::data::data_time_t span() const {
        if (empty()) { return 0; }
        return m_recent - m_pqueue.top().tstart();
    }

    ptmp::data::data_time_t covers(ptmp::data::data_time_t t) const {
        if (empty()) { return 0; }
        return m_recent - t;
    }

    // Add a TP to the queue.  
    void add(const ptmp::data::TrigPrim& tp) {
        ptmp::data::data_time_t tstart = tp.tstart();
        m_recent = std::max(m_recent, tstart);
        m_pqueue.push(tp);
    }

    // forward methods
    bool empty() const { return m_pqueue.empty(); }
    size_t size() const { return m_pqueue.size(); }
    const value_type& top() const { return m_pqueue.top(); }
    void pop() { m_pqueue.pop(); }

private:
    std::priority_queue<value_type, collection_type, tp_greater_t> m_pqueue;

    // most recent (largest) hw clock seen
    ptmp::data::data_time_t m_recent{0};
};


// The actor function
void ptmp::actor::window(zsock_t* pipe, void* vargs)
{
    auto config = json::parse((const char*) vargs);
    std::string name = "window";
    if (config["name"].is_string()) {
        name = config["name"];
    }
    ptmp::internals::set_thread_name(name);

    int verbose=0;              // fixme, add log level support in internals.
    if (config["verbose"].is_number()) {
        verbose = config["verbose"];
    }

    int detid = -1;
    if (config["detid"].is_number()) {
        detid = config["detid"];
    }
    ptmp::data::data_time_t toff=0;
    if (config["toffset"].is_number()) {
        toff = config["toffset"];
    }
    ptmp::data::data_time_t tspan=0;
    if (config["tspan"].is_number()) {
        tspan = config["tspan"];
    }
    if (!tspan) {
        zsys_error("window: requires finite tspan");
        throw std::runtime_error("tpwindow requires finite tspan");

    }
    ptmp::data::data_time_t tbuf=0;
    if (config["tbuf"].is_number()) {
        tbuf = config["tbuf"].get<int>();
    }
    tbuf = std::max(tspan,tbuf);

    zsock_t* isock = ptmp::internals::endpoint(config["input"].dump());
    zsock_t* osock = ptmp::internals::endpoint(config["output"].dump());
    if (!isock or !osock) {
        zsys_error("window: requires socket configuration");
        throw std::runtime_error("tpwindow requires socket configuration");

    }
    
    zsock_signal(pipe, 0);      // signal ready
    zpoller_t* poller = zpoller_new(pipe, isock, NULL);

    // note, this starts at wind=0 which is way way in the past.
    time_window_t window(tspan, toff);
    priority_tp_span_t buffer;

    bool got_quit = false;
    size_t count_in = 0;
    size_t count_out = 0;

    while (!zsys_interrupted) {

        void* which = zpoller_wait(poller, -1);
        if (zpoller_terminated(poller)) {
            zsys_info("window: interrupted in poll");
            break;
        }
        if (which == pipe) {
            zsys_info("window: got quit after %ld", count_in);
            got_quit = true;
            goto cleanup;
        }

        zmsg_t* msg = zmsg_recv(isock);
        if (!msg) {
            zsys_info("window: interrupted in recv");
            break;
        }
        ++count_in;

        {                       // input handling
            ptmp::data::TPSet tpset;
            ptmp::internals::recv(&msg, tpset); // throws
            if (tpset.tps_size() == 0) {
                zsys_error("receive empty TPSet from det #0x%x", tpset.detid());
                continue;
            }

            if (detid < 0) {        // forward if user doesn't provide
                detid = tpset.detid();
            }            

            // If we don't know when wer are, buffer takes precedence and sets window.
            if (window.wind == 0) { 
                for (const auto& tp : tpset.tps()) {
                    buffer.add(tp);
                }
                window.set_bytime(buffer.top().tstart());
            }
            else {
                for (const auto& tp : tpset.tps()) {
                    if (window.cmp(tp.tstart()) < 0) {
                        if (verbose) {
                            zsys_debug("window: channel %d tardy TP at -%ld + %ld data time ticks",
                                       tp.channel(), window.tbegin()-tp.tstart(), tp.tspan());
                        }
                        // tardy
                        continue;
                    }
                    buffer.add(tp);
                }
            }
        }

        // Drain if we have buffered past the current window by tbuf amount
        while (buffer.covers(window.tbegin()) >= tbuf+tspan) {
            
            ptmp::data::TPSet tpset;
            tpset.set_count(count_out++);
            tpset.set_detid(detid);
            tpset.set_created(ptmp::data::now());
            tpset.set_tstart(window.tbegin());
            tpset.set_tspan(tspan);
            tpset.set_totaladc(0);
            bool first = true;

            // fill outgoing TPSet
            while (!buffer.empty()) {
                const ptmp::data::TrigPrim& tp = buffer.top();
                if (!window.in(tp.tstart())) {
                    break;
                }
                ptmp::data::TrigPrim* newtp = tpset.add_tps();
                *newtp = tp;
                buffer.pop();
                tpset.set_totaladc(tpset.totaladc() + newtp->adcsum());
                const auto chan = newtp->channel();
                if (first or tpset.chanbeg() > chan) {
                    tpset.set_chanbeg(chan);
                }
                if (first or tpset.chanend() < chan) {
                    tpset.set_chanend(chan);
                }
                first = false;
            }
            if (buffer.empty() or tpset.tps_size() == 0) {
                zsys_error("window: logic error, buffer or tpset empty, should not happen");
                assert(tpset.tps_size() > 0); // logic error, should not happen
            }
            window.set_bytime(buffer.top().tstart());
            
            // send carefully in case we are blocked we still want to be able to get shutdown
            while (! (zsock_events(osock) & ZMQ_POLLOUT)) {
                if (zsock_events(pipe) & ZMQ_POLLIN) {
                    got_quit = true;
                    if (verbose) {
                        zsys_debug("window: got quit while waiting to output");
                    }
                    goto cleanup;
                }
                zclock_sleep(1); // ms
            }
            ptmp::internals::send(osock, tpset); // fixme: can throw

        } // go around to see if enough left in the buffer.

    } // message loop

  cleanup:

    if (verbose) {
        zsys_debug("window: finishing with %ld in, %ld out and %ld still in the buffer",
                   count_in, count_out, buffer.size());
        while (buffer.size() > 0) {
            auto tp = buffer.top(); buffer.pop();
            zsys_debug("window:\t channel %d, tstart %ld", tp.channel(), tp.tstart());
        }
    }
    zpoller_destroy(&poller);
    zsock_destroy(&isock);
    zsock_destroy(&osock);

    if (got_quit) {
        return;
    }
    if (verbose) {
        zsys_debug("window: waiting for quit");
    }
    zsock_wait(pipe);
}

ptmp::TPWindow::TPWindow(const std::string& config)
    : m_actor(zactor_new(ptmp::actor::window, (void*)config.c_str()))
{
}

ptmp::TPWindow::~TPWindow()
{
    zsys_debug("window: signaling done");
    zsock_signal(zactor_sock(m_actor), 0); // signal quit
    zactor_destroy(&m_actor);
    zsys_debug("window: destroying");
}


