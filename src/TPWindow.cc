#include "ReactorApp.h"
#include "ptmp/api.h"

#include "ptmp/factory.h"
#include "ptmp/actors.h"

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
    ptmp::data::data_time_t toff, tspan;

    time_window_t(ptmp::data::data_time_t tspan_,
                  ptmp::data::data_time_t toff_,
                  ptmp::data::data_time_t wind_ = 0)
        : tspan(tspan_), toff(toff_%tspan_), wind(wind_) { }
    time_window_t() : tspan(0), toff(0), wind(0) { }
    void init(ptmp::data::data_time_t tspan_,
              ptmp::data::data_time_t toff_,
              ptmp::data::data_time_t wind_ = 0) {
        tspan = tspan_;
        toff = toff_%tspan_;
        wind = wind_;
    }

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

class WindowApp : public ptmp::noexport::ReactorApp {
    ptmp::data::data_time_t tspan{0}, toff{0}, tbuf{0};

    time_window_t window;
    priority_tp_span_t buffer;

    // fodder for additional metrics with window semantics not held in
    // base
    size_t count_tardy_tps{0};

public:

    WindowApp(zsock_t* pipe, json& config)
        : ReactorApp(pipe, config, "window") {

        if (config["toffset"].is_number()) {
            toff = config["toffset"];
        }
        if (config["tspan"].is_number()) {
            tspan = config["tspan"];
        }
        if (!tspan) {
            zsys_error("window (%s): requires finite tspan", name.c_str());
            throw std::runtime_error("tpwindow requires finite tspan");
        }
        if (config["tbuf"].is_number()) {
            tbuf = config["tbuf"].get<int>();
        }
        tbuf = std::max(tspan, tbuf);
        window.init(tspan, toff);

        set_osock(ptmp::internals::endpoint(config["output"].dump()));
        set_isock(ptmp::internals::endpoint(config["input"].dump()));

    } // ctor

    int add_input(ptmp::data::TPSet& tpset) {
        

        // If we don't know when we are, buffer takes precedence and sets window.
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
                    ++count_tardy_tps;
                    return 0;
                }
                buffer.add(tp);
            }
        }
        // finished processing fresh input.
        return 0;
    }

    int send_output() {
        // Drain if we have buffered past the current window by tbuf amount
        while (buffer.covers(window.tbegin()) >= tbuf+tspan) {

            ptmp::data::TPSet otpset;
            otpset.set_tstart(window.tbegin());
            otpset.set_tspan(tspan);
            otpset.set_totaladc(0);
            bool first = true;

            // fill outgoing TPSet
            while (!buffer.empty()) {
                const ptmp::data::TrigPrim& tp = buffer.top();
                if (!window.in(tp.tstart())) {
                    break;
                }
                ptmp::data::TrigPrim* newtp = otpset.add_tps();
                *newtp = tp;
                buffer.pop();
                otpset.set_totaladc(otpset.totaladc() + newtp->adcsum());
                const auto chan = newtp->channel();
                if (first or otpset.chanbeg() > chan) {
                    otpset.set_chanbeg(chan);
                }
                if (first or otpset.chanend() < chan) {
                    otpset.set_chanend(chan);
                }
                first = false;
            }
            if (buffer.empty() or otpset.tps_size() == 0) {
                zsys_error("window: logic error, buffer or tpset empty, should not happen");
                assert(otpset.tps_size() > 0); // logic error, should not happen
            }
            window.set_bytime(buffer.top().tstart());
            
            int rc = this->send(otpset);
            if (rc != 0) {
                return rc;
            }
        } // go around to see if enough left in the buffer.

        return 0;
    }

    virtual int add(ptmp::data::TPSet& tpset) {

        int rc = this->add_input(tpset);
        if (rc != 0) {
            return rc;
        }

        return this->send_output();
    }

    virtual void metrics(json& jmet, ptmp::data::real_time_t dtr, ptmp::data::data_time_t dtd) {
        double dt = dtd;
        jmet["rates"]["tardytps"] = count_tardy_tps/dt;
        count_tardy_tps = 0;
    }

    virtual ~WindowApp () {
    }
};


// The actor function (reactor pattern)
void ptmp::actor::window(zsock_t* pipe, void* vargs)
{
    auto jcfg = json::parse((const char*) vargs);
    WindowApp app(pipe, jcfg);
    zsock_signal(pipe, 0);      // signal ready
    app.start();
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


