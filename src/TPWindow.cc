#include "ptmp/api.h"
#include "ptmp/internals.h"
#include "ptmp/factory.h"

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
};

struct tp_greater_t {
    bool operator()(const ptmp::data::TrigPrim& a, const ptmp::data::TrigPrim& b) {
        // give priority to smaller times
        return a.tstart() > b.tstart();
    }
};

// a priority queue of TPs that maintains a span.
class priority_tp_span_t {
public:
    typedef ptmp::data::TrigPrim value_type;
    typedef typename std::vector<value_type> collection_type;

    ptmp::data::data_time_t span() const {
        if (empty()) { return 0; }
        return m_recent - m_pqueue.top().tstart();
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

static void dump_window(const time_window_t& window, std::string msg="")
{
    zsys_debug("window: %s #%ld [%ld]+%ld @ %ld",
               msg.c_str(), window.wind, window.toff, window.tspan, window.tbegin());
}
static void dump_tpset(const ptmp::data::TPSet& tps, std::string msg="")
{
    ptmp::data::data_time_t tstart = tps.tstart();
    zsys_debug("window: tpset: %s @ %ld + %d # %d detid: %d",
               msg.c_str(), tstart, tps.tspan(), tps.count(), tps.detid());
    for (const auto& tp : tps.tps()) {
        ptmp::data::data_time_t tp_ts = tp.tstart();
        ptmp::data::data_time_t tp_dt = tp_ts - tstart;
        zsys_debug("\ttp: @ %ld (%8ld) + %d qpeak: %d qtot: %d chan: %d",
                   tp_ts, tp_dt, tp.tspan(), tp.adcpeak(), tp.adcsum(), tp.channel());
    }
}

static void test_time_window()
{
    const ptmp::data::data_time_t tspan = 300, toff=53;

    time_window_t tw(tspan, toff);
    assert(tw.wind == 0);
    assert(toff == tw.tbegin());
    assert(tw.in(toff));
    zsys_debug("cmp: %d" , tw.cmp(tspan+toff-1));
    assert(tw.in(tspan+toff-1));
    assert(!tw.in(toff-1));
    assert(!tw.in(tspan+toff));
    dump_window(tw);

    tw.set_bytime(tspan + toff + 1);
    //zsys_debug("wind: %ld", tw.wind);
    assert(tw.wind == 1L);
    assert(tw.tbegin() == tspan + toff);
    tw.wind = 1000;
    assert(tw.tbegin() == 1000*tspan + toff);
    dump_window(tw);
}
    
struct TPWindower {
    zsock_t* osock;
    time_window_t window;
    ptmp::data::TPSet tpset;
    priority_tp_span_t buffer;
    ptmp::data::data_time_t tbuf;
    
    TPWindower(zsock_t* osock, ptmp::data::data_time_t tspan,
               ptmp::data::data_time_t toff, ptmp::data::data_time_t tbuf, int detid)
        : osock(osock), window(tspan, toff), tbuf(std::max(tspan,tbuf)) {
        tpset.set_count(0);
        tpset.set_tstart(toff);
        tpset.set_tspan(tspan);
        tpset.set_detid(detid);
        tpset.set_created(ptmp::data::now()); // maybe want to override this just before a send()
    }

    // Maybe add (a copy of) the given tp.  Return true if added.
    bool maybe_add(const ptmp::data::TrigPrim& tp) {
        const ptmp::data::data_time_t tstart = tp.tstart();
        const int cmp = window.cmp(tstart);
        if (cmp < 0) {
            // zsys_debug("window: tardy TP @%ld dt=%ld",
            //            tstart, window.tbegin()-tstart);
            // dump_window(window);
            return false;
        }
        buffer.add(tp);

        while (buffer.span() >= tbuf) {  // drain buffer while it's bigger than tbuf.
            reset(buffer.top().tstart());
            while (buffer.size() > 0 and window.in(buffer.top().tstart())) {
                ptmp::data::TrigPrim* newtp = tpset.add_tps();
                *newtp = buffer.top();
                buffer.pop();
                tpset.set_totaladc(tpset.totaladc() + newtp->adcsum());
                const auto chan = newtp->channel();
                if (tpset.chanbeg() == -1 or tpset.chanbeg() > chan) {
                    tpset.set_chanbeg(chan);
                }
                if (tpset.chanend() == -1 or tpset.chanend() < chan) {
                    tpset.set_chanend(chan);
                }
            }
            if (osock) {                       // allow null for testing
                ptmp::internals::send(osock, tpset); // fixme: can throw
            }
            else {
                zsys_debug("window: testing mode, not actually sending a TPSet");
            }
            // note, this leaves tpset full of stale/sent info....

            // dump_window(window, "send");
            // dump_tpset(tpset, "send");
            // ptmp::data::dump(tpset, "send");
        }
        return true;
    }

    // Reset state.  Better send() tpset before calling.
    const time_window_t& reset(ptmp::data::data_time_t t) {
        window.set_bytime(t);
        tpset.set_count(tpset.count()+1);
        tpset.set_created(ptmp::data::now());
        tpset.set_tstart(window.tbegin());
        tpset.set_chanbeg(-1);
        tpset.set_chanend(-1);
        tpset.set_totaladc(0);
        tpset.clear_tps();
        return window;
    }
};

static void test_tpwindower()
{
    ptmp::data::data_time_t tspan=300, toff=53, tbuf=0;
    TPWindower tpw(nullptr, tspan, toff, tbuf, 1234);
    dump_window(tpw.window);
    const auto& tw = tpw.reset(tspan+toff);
    assert(tw.wind == 1);
    assert(tw.in(tspan+toff));
    assert(tw.in(tspan+2*toff-1));

    ptmp::data::TrigPrim tp;

    tp.set_tstart(tspan+toff);  // next window
    tp.set_channel(42);
    bool ok = tpw.maybe_add(tp);
    assert(ok);
    zsys_debug("buffered %d tpsets over time span %ld, tbuf=%ld",
               (int)tpw.buffer.size(), tpw.buffer.span(), tpw.tbuf);
    zsys_debug("tstart=%ld, w.tbeg=%ld w.toff=%ld w.tspan=%ld",
               tpw.buffer.top().tstart(), tpw.window.tbegin(), tpw.window.toff, tpw.window.tspan);
    zsys_debug("window in %d", tpw.window.in(tpw.buffer.top().tstart()));
    assert(tpw.buffer.size() == 1);
    dump_window(tpw.window);

    tp.set_tstart(tp.tstart() + 1);
    tp.set_channel(43);    
    ok = tpw.maybe_add(tp);
    assert(ok);
    zsys_debug("buffered %d tpsets over time span %ld", (int)tpw.buffer.size(), tpw.buffer.span());
    assert(tpw.buffer.size() == 2);
    dump_window(tpw.window);
    
    tp.set_tstart(toff);
    ok = tpw.maybe_add(tp);
    assert(!ok);
    zsys_debug("buffered %d tpsets over time span %ld", (int)tpw.buffer.size(), tpw.buffer.span());
    assert(tpw.buffer.size() == 2);
    dump_window(tpw.window);

    zsys_debug("advancing window");
    tp.set_tstart(toff+2*tspan); // in next window
    ok = tpw.maybe_add(tp);
    assert(ok);
    dump_window(tpw.window);
    zsys_debug("buffered %d tpsets over time span %ld", (int)tpw.buffer.size(), tpw.buffer.span());
    assert(tpw.buffer.size() == 1);

    tp.set_tstart(tp.tstart() + 1);
    ok = tpw.maybe_add(tp);
    assert(ok);
    zsys_debug("buffered %d tpsets over time span %ld", (int)tpw.buffer.size(), tpw.buffer.span());
    assert(tpw.buffer.size() == 2);
    dump_window(tpw.window);
}


// The actor function
static
void tpwindow_proxy(zsock_t* pipe, void* vargs)
{
    auto config = json::parse((const char*) vargs);
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
    if (tbuf == 0) {
        std::string dump = config.dump(4);
        zsys_error("window: requires non-zero tbuf.\n%s", dump.c_str());
        throw std::runtime_error("tpwindow requires non-zero tbuf");        
    }

    zsock_t* isock = ptmp::internals::endpoint(config["input"].dump());
    zsock_t* osock = ptmp::internals::endpoint(config["output"].dump());
    if (!isock or !osock) {
        zsys_error("window: requires socket configuration");
        throw std::runtime_error("tpwindow requires socket configuration");

    }
    
    zsock_signal(pipe, 0);      // signal ready
    zpoller_t* poller = zpoller_new(pipe, isock, NULL);

    // initial window is most likely way before any data.
    TPWindower windower(osock, tspan, toff, tbuf, detid);
    bool got_quit = false;
    int count_in = 0;
    int nfailed_total = 0;
    const int pay_attention = 100;
    while (!zsys_interrupted) {

        void* which = zpoller_wait(poller, -1);
        if (zpoller_terminated(poller)) {
            zsys_info("window: interrupted in poll");
            break;
        }
        if (which == pipe) {
            zsys_info("window: got quit after %d", count_in);
            dump_window(windower.window, "quit");
            got_quit = true;
            goto cleanup;
        }

        // zsys_info("TPWindow got input");
        // dump_window(windower.window, "recv");

        zmsg_t* msg = zmsg_recv(isock);
        if (!msg) {
            zsys_info("window: interrupted in recv");
            break;
        }
        ++count_in;

        ptmp::data::TPSet tpset;
        ptmp::internals::recv(msg, tpset); // throws
        ptmp::data::real_time_t latency = ptmp::data::now() - tpset.created();
        // dump_window(windower.window, "recv");
        // dump_tpset(tpset, "recv");
        // ptmp::data::dump(tpset, "recv");

        if (detid < 0) {        // forward if user doesn't provide
            detid = tpset.detid();
            windower.tpset.set_detid(detid);
        }            

        int ntps_failed = 0;
        // We do not know ordering of TrigPrim inside TPSet
        std::sort(tpset.mutable_tps()->begin(), tpset.mutable_tps()->end(),
                  [](const ptmp::data::TrigPrim& a, const ptmp::data::TrigPrim& b) {
                      return a.tstart() < b.tstart();
                  });

        for (const auto& tp : tpset.tps()) {

            // maybe_add() will block if output queue is full.  do a
            // sleepy loop, checking for shutdown or until we can
            // send.  This is an ugly hack in lieu of restructuring
            // this code to make it clearer.
            while (! (zsock_events(osock) & ZMQ_POLLOUT)) { // if output is full
                if (zsock_events(pipe) & ZMQ_POLLIN) {
                    got_quit = true;
                    zsys_debug("window: got quit while waiting to output");
                    goto cleanup;
                }
                zclock_sleep(1); // ms
            }


            bool ok = windower.maybe_add(tp);
            if (!ok) {
                ++ntps_failed;
            //     zsys_debug("\tfail TP at %ld", tp.tstart());
            // }
            // else {
            //     zsys_debug("\tkeep TP at %ld", tp.tstart());
            }

        }
        if (ntps_failed) {
            //zsys_debug("tpwindow: failed to add %d TPs, latency:%ld", ntps_failed, latency);
            nfailed_total += ntps_failed;
            continue;
        }        


    } // message loop

  cleanup:

    zsys_debug("window: finishing with %d failed after %d with %ld in the buffer",
               nfailed_total, count_in, windower.buffer.size());
    zpoller_destroy(&poller);
    zsock_destroy(&isock);
    zsock_destroy(&osock);

    if (got_quit) {
        return;
    }
    zsys_debug("window: waiting for quit");
    zsock_wait(pipe);
}

ptmp::TPWindow::TPWindow(const std::string& config)
    : m_actor(zactor_new(tpwindow_proxy, (void*)config.c_str()))
{
}

ptmp::TPWindow::~TPWindow()
{
    zsys_debug("window: signaling done");
    zsock_signal(zactor_sock(m_actor), 0); // signal quit
    zactor_destroy(&m_actor);
    zsys_debug("window: destroying");
}

void ptmp::TPWindow::test()
{
    test_time_window();
    test_tpwindower();
}
