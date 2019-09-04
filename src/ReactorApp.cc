#include "ReactorApp.h"
#include "ptmp/internals.h"
#include "ptmp/data.h"
using namespace ptmp::noexport;
using json = nlohmann::json;

static
int handle_pipe(zloop_t* loop, zsock_t* sock, void* varg)
{
    // any message on pipe means "shutdown"
    return -1;
}
static
int handle_timer_link(zloop_t* loop, int timer_id, void* varg)
{
    ReactorApp * app = (ReactorApp*)varg;
    return app->metrics_base();
}
static
int handle_input(zloop_t* loop, zsock_t* sock, void* varg)
{
    ReactorApp * app = (ReactorApp*)varg;
    zmsg_t* msg = zmsg_recv(sock);
    ptmp::data::TPSet tpset;
    ptmp::internals::recv(&msg, tpset);
    return app->add_base(tpset);
}


ptmp::noexport::ReactorApp::ReactorApp(zsock_t* pipe_, json& config, const std::string& defname)
    : pipe(pipe_)
{
    name = defname;
    if (!config["name"].is_null()) {
        name = config["name"];
    }
    ptmp::internals::set_thread_name(name);
    if (!config["verbose"].is_null()) {
        verbose = config["verbose"];
    }
    if (!config["detid"].is_null()) {
        detid = config["detid"];
    }

    looper = zloop_new();
    zloop_reader(looper, pipe, handle_pipe, this);

    if (config["metrics"].is_object()) {
        met = new ptmp::metrics::Metric(config["metrics"].dump());
        int integ_time_ms = 10000;
        if (!config["metrics"]["period"].is_null()) {
            integ_time_ms = config["metrics"]["period"];
        }
        if (!config["tickperus"].is_null()) {
            tickperus = config["tickperus"];
        }
        zsys_debug("metrics from %s on 0x%x every %d ms", name.c_str(), detid, integ_time_ms);
        zloop_timer(looper, integ_time_ms, 0, handle_timer_link, this);
    }
}
void ptmp::noexport::ReactorApp::set_isock(zsock_t* isock_)
{
    assert(!isock);
    isock = isock_;
    if (!isock) {
        zsys_error("%s: input socket required", name.c_str());
        throw std::runtime_error("input socket required");
    }
    zloop_reader(looper, isock, handle_input, this);
}
void ptmp::noexport::ReactorApp::set_osock(zsock_t* osock_)
{
    assert(!osock);
    osock = osock_;
    if (!osock) {
        zsys_error("%s: output socket required", name.c_str());
        throw std::runtime_error("output socket required");
    }
}

ptmp::noexport::ReactorApp::~ReactorApp()
{
    zloop_destroy(&looper);
    zsock_destroy(&isock);
    zsock_destroy(&osock);
}

void ptmp::noexport::ReactorApp::start()
{
    start_time = ptmp::data::now();
    zloop_start(looper);
}

int ptmp::noexport::ReactorApp::add_base(ptmp::data::TPSet& tpset)
{
    const auto this_count = tpset.count();
    if (last_in_count == 0) {
        last_in_count = this_count;
    }
    else {
        const auto n_missed = this_count - last_in_count - 1;
        if (n_missed) {
            stats.n_in_lost += n_missed;
        }
        last_in_count = this_count;
    }

    if (met) {
        stats.iss.update(tpset, tickperus);
    }
    if (tpset.tps_size() == 0) {
        zsys_error("receive empty TPSet from det #0x%x", tpset.detid());
        return 0;
    }
    if (detid < 0) {        // forward if user doesn't provide
        detid = tpset.detid();
    }            

    return this->add(tpset);
}


int ReactorApp::metrics_base()
{
    if (!met) {
        return 0;
    }

    stats.iss.finalize();
    stats.oss.finalize();

    json j;
    j["in"] = stats.iss.jsonify();
    j["out"] = stats.oss.jsonify();
    j["waits"] = json{
        { "count", stats.waits.count}
        ,{"duty", (stats.waits.time_ms/1000.0)* stats.oss.real.hz}
    };
    j["lost"]["input"] = stats.n_in_lost;


    //zsys_debug("metric %d from 0x%x", out_tpset_count, detid);

    this->metrics(j);
    (*met)(j);

    stats = stats_t();          // resets

    return 0;
}

int ptmp::noexport::ReactorApp::send(ptmp::data::TPSet& tpset)
{
    // send carefully in case we are blocked we still want to be able to get shutdown
    uint64_t nwait = 0;
    ptmp::data::real_time_t then = ptmp::data::now();
    while (! (zsock_events(osock) & ZMQ_POLLOUT)) {
        if (zsock_events(pipe) & ZMQ_POLLIN) {
            if (verbose) {
                zsys_debug("window: got quit while waiting to output");
            }
            return -1;          // want to stop
        }
        zclock_sleep(1); // ms
        ++nwait;
    }
    if (nwait) {
        ++stats.waits.count;
        stats.waits.time_ms += (ptmp::data::now() - then)/1000;
    }
    tpset.set_count(out_tpset_count++);
    tpset.set_detid(detid);
    tpset.set_created(ptmp::data::now());
    ptmp::internals::send(osock, tpset); // fixme: may throw
    if (met) {
        stats.oss.update(tpset, tickperus);
    }

    if (verbose>0) {
        if (out_tpset_count % 100000 == 1) {
            double dt = ptmp::data::now() - start_time;
            double hz = 1e6 * out_tpset_count/dt;
            zsys_debug("%s: output at %.1f Hz", name.c_str(), hz);
        }
    }

    return 0;
}
