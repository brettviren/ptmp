#include "ptmp/api.h"
#include "ptmp/internals.h"
#include "ptmp/factory.h"
#include "ptmp/actors.h"
#include "json.hpp"

using json = nlohmann::json;

// fixme: internalize this
const int json_message_type = 0x4a534f4e; // 1246973774 in decimal

// Accumulate per channel statistics
struct chan_stats_t {
    uint64_t tstart{0}, tstart_span{0};

    uint32_t ntps{0};
    uint64_t span_us{0};        // integral of TP tspans in us
    uint64_t adcsum{0};         // integral of TP ADC
    uint64_t adcpeak{0};        // integral of TP ADC peak values
};
static
void to_json(json& j, const chan_stats_t& cs)
{
    const double dtdata_s = cs.tstart_span*1.0e-6;
    const double chdtspan_s = cs.span_us*1e-6;
    j = json{
        {"integ", {
                {"ntps", cs.ntps},
                {"span_us", cs.span_us},
                {"adcsum", cs.adcsum},
                {"adcpeak", cs.adcpeak}
            }},
        {"avg", {
                {"ntps", cs.ntps/dtdata_s}, 
                {"occupancy", chdtspan_s/dtdata_s},
                {"adcsum", cs.adcsum/dtdata_s},
                {"adcpeak", cs.adcpeak/dtdata_s}
            }},
        {"inst", {
                {"ntps", cs.ntps/chdtspan_s}, 
                {"adcsum", cs.adcsum/chdtspan_s},
                {"adcpeak", cs.adcpeak/chdtspan_s}
            }}
    };
}

struct link_stats_t {
    uint64_t ntpsets{0}, ntps{0}, nskipped{0}, last_seqno{0};
    uint64_t totaladc{0}, adcsum{0};
    uint64_t tstart{0}, tstart_span{0};
    int64_t tcreated{0}, tcreated_span{0};
    int64_t treceived{0}, treceived_span{0};
    // sum and sum squared used to calculate mean and rms
    // sum of trecieved-tstart, tcreated-tstart.  
    int64_t dtr_sum{0}, dtc_sum{0};           
    // sum of (trecieved-tstart)^2, (tcreated-tstart)^2.
    int64_t dtr_sum2{0}, dtc_sum2{0};           
    // min/max time between recieved and created relative to tstart
    int64_t dtr_min{0}, dtr_max{0}, dtc_min{0}, dtc_max{0};
    std::unordered_set<int> ch_seen;
    // how much data received on from the link
    uint64_t nbytes{0};

};
static
void to_json(json& j, const link_stats_t& ls)
{
    const int nchannels = ls.ch_seen.size();
    const double dtrecv_s = ls.treceived_span*1.0e-6;
    const double ntpsets = ls.ntpsets;
    const double ntps = ls.ntps;
    const double dtr_mean = ls.dtr_sum / ntpsets;
    const double dtc_mean = ls.dtc_sum / ntpsets;
    const double dtr_rms = sqrt(ls.dtr_sum2 / ntpsets - dtr_mean*dtr_mean);
    const double dtc_rms = sqrt(ls.dtc_sum2 / ntpsets - dtc_mean*dtc_mean);

    j = json{
        {"now", (time_t)(ls.tstart/1000000)}, // the representative time.
        {"time", {
                {"received", ls.treceived * 1.0e-6},
                {"created", ls.tcreated * 1.0e-6},
                {"data", ls.tstart * 1.0e-6},
            }},
        {"counts", {
                {"ntpsets", ls.ntpsets},
                {"ntps", ls.ntps},
                {"ndropped", ls.nskipped},
                {"nchannels", nchannels},
                {"nbytes", ls.nbytes}
            }},

        {"rates", {
                {"tps", ls.ntps/dtrecv_s},
                {"tpsets", ls.ntpsets/dtrecv_s},
                {"dropped", ls.nskipped/dtrecv_s},
                {"byteps", ls.nbytes/dtrecv_s}
            }},

        {"adc", {
                {"tpsets", ls.totaladc},
                {"tps", ls.adcsum},
                {"rate", ls.adcsum/dtrecv_s},
                {"mean", ls.adcsum/ntps}
            }},

        {"latency", {
                {"data", {
                        {"mean",    dtr_mean},
                        {"rms",     dtr_rms},
                        {"min",  ls.dtr_min},
                        {"max",  ls.dtr_max}
                    }},
                {"trans", {
                        {"mean",   dtc_mean},
                        {"rms",    dtc_rms},
                        {"min", ls.dtc_min},
                        {"max", ls.dtc_max}}}}}
    };
    assert (j.is_object());
}

struct app_data_t {
    std::string topkey{"tpsets"};
    int tick_per_us{0}, tick_off_us{0};

    zsock_t* isock{NULL}, *osock{NULL};
    uint64_t link_seqno{0}, chan_seqno{0}; // count outgoing messages

    std::unordered_map<int, link_stats_t> links;
    std::unordered_map<int, chan_stats_t> chans;
    
    void add(ptmp::data::TPSet& tpset) {
        const int detid = tpset.detid();
        auto& ls = links[detid];
        auto seqno_in = tpset.count();

        auto tstart = tpset.tstart()/tick_per_us - tick_off_us;
        auto tcreat = tpset.created();
        auto trecv = ptmp::data::now();

        ls.totaladc += tpset.totaladc();
        ls.nbytes += tpset.ByteSize();

        if (ls.ntpsets == 0) {
            ls.last_seqno = seqno_in;
            ls.tstart = tstart;
            ls.tcreated = tcreat;
            ls.treceived = trecv;
        }
        ls.tstart_span = tstart - ls.tstart;
        ls.tcreated_span = tcreat - ls.tcreated;
        ls.treceived_span = trecv - ls.treceived;

        if (seqno_in - ls.last_seqno > 1) {
            ++ls.nskipped;
        }
        ls.last_seqno = seqno_in;

        int64_t dtr = trecv - tstart;
        int64_t dtc = trecv - tcreat;

        // zsys_debug("0x%x ntpsets: %ld, tstart: %ld, tstart_span: %ld, dtr:%ld, dct:%ld",
        //            tpset.detid(),
        //            ls.ntpsets, ls.tstart, ls.tstart_span, dtr, dtc);

        ls.dtr_sum += dtr;
        ls.dtc_sum += dtc;
        ls.dtr_sum2 += dtr*dtr;
        ls.dtc_sum2 += dtc*dtc;
        if (ls.ntpsets == 0) {
            ls.dtr_min = ls.dtr_max = dtr;
            ls.dtc_min = ls.dtc_max = dtc;
        }
        else {
            ls.dtr_min = std::min(ls.dtr_min, dtr);
            ls.dtr_max = std::max(ls.dtr_max, dtr);
            ls.dtc_min = std::min(ls.dtc_min, dtc);
            ls.dtc_max = std::max(ls.dtc_max, dtc);
        }
        ls.ntpsets += 1;
        ls.ntps += tpset.tps_size();

        for (auto& tp : tpset.tps()) {
            const int ch = tp.channel();
            auto& cs = chans[ch];

            ls.ch_seen.insert(ch);
            ls.adcsum += tp.adcsum();

            if (cs.ntps == 0) {
                cs.tstart = tstart;
            }
            cs.tstart_span = tstart - cs.tstart;

            cs.ntps += 1;
            cs.span_us += tp.tspan()/tick_per_us;
            cs.adcsum += tp.adcsum();
            cs.adcpeak += tp.adcpeak();
        }
    }

};

static
int handle_pipe(zloop_t* loop, zsock_t* sock, void* varg)
{
    // message on pipe means "shutdown"
    return -1;
}

static
int handle_input(zloop_t* loop, zsock_t* sock, void* varg)
{
    app_data_t& ad = *(app_data_t*)varg;

    zmsg_t* msg = zmsg_recv(sock);
    ptmp::data::TPSet tpset;
    ptmp::internals::recv(&msg, tpset);
    ad.add(tpset);
    return 0;
}

static
int handle_timer_chan(zloop_t* loop, int timer_id, void* varg)
{
    app_data_t& ad = *(app_data_t*)varg;

    json jdat;
    jdat["nchans"] = ad.chans.size();
    for (const auto& cit : ad.chans) {
        const chan_stats_t cs = cit.second;
        json jcs = cs; // magic (uses to_json)
        int chid = cit.first;
        char schan[32];
        snprintf(schan, 32, "ch%d", chid);
        jcs["chan"] = chid;
        jdat[schan] = jcs;
    }
    ad.chans.clear();

    jdat["seqno"] = ad.chan_seqno++;
    json jtop;
    jtop[ad.topkey]["chans"] = jdat;
    std::string sdat = jtop.dump();

    // PTMP protocol puts a type as first frame.
    zsock_send(ad.osock, "is", json_message_type, sdat.c_str());
    return 0;
}

static
int handle_timer_link(zloop_t* loop, int timer_id, void* varg)
{
    // when we are in here, input is piling up!  be brief.

    app_data_t& ad = *(app_data_t*)varg;

    json jdat;
    int nlinks = ad.links.size();
    for (const auto& sit : ad.links) {
        const link_stats_t ls = sit.second;
        json jls = ls; // magic (uses to_json)
        int detid = sit.first;
        char sdetid[32];
        snprintf(sdetid, 32, "0x%x", detid);
        jls["detid"]=detid;
        jdat[sdetid] = jls;
    }
    ad.links.clear();

    jdat["nlinks"] = nlinks;
    jdat["seqno"] = ad.link_seqno++;
    json jtop;
    jtop[ad.topkey]["links"] = jdat;
    std::string sdat = jtop.dump();

    // PTMP protocol puts a type as first frame.
    zsock_send(ad.osock, "is", json_message_type, sdat.c_str());
    return 0;
}

static
void stats(zsock_t* pipe, void* vargs)
{
    app_data_t ad;

    auto config = json::parse((const char*) vargs);

    std::string name = "stats";
    if (config["name"].is_string()) {
        name = config["name"];
    }
    ptmp::internals::set_thread_name(name);

    // All produced JSON will be a value on this top level key. 
    if (config["topkey"].is_string()) {
        ad.topkey = config["topkey"];
    }

    int integ_link_ms{1000}, integ_chan_ms{10000};
    // how long to integrate link level stats
    if (config["link_integration_time"].is_number()) {
        integ_link_ms = config["link_integration_time"];
    }
    // how long to integrate channel level stats
    if (config["chan_integration_time"].is_number()) {
        integ_chan_ms = config["chan_integration_time"];
    }

    // How many data ticks per us.
    if (config["tick_per_us"].is_number()) {
        ad.tick_per_us = config["tick_per_us"];
    }

    // time in us from Unix epoch to data tick 0.
    if (config["tick_off_us"].is_number()) {
        ad.tick_off_us = config["tick_off_us"];
    }

    if (!config["input"].is_null()) {
        std::string cfg = config["input"].dump();
        ad.isock = ptmp::internals::endpoint(cfg);
        if (!ad.isock) {
            throw std::runtime_error("no input socket configured");
        }
    }
    if (!config["output"].is_null()) {
        std::string cfg = config["output"].dump();
        ad.osock = ptmp::internals::endpoint(cfg);
        if (!ad.osock) {
            throw std::runtime_error("no output socket configured");
        }
    }

    zsock_signal(pipe, 0); // signal ready    

    zloop_t *looper = zloop_new();

    zloop_reader(looper, pipe, handle_pipe, &ad);
    zloop_reader(looper, ad.isock, handle_input, &ad);
    if (integ_link_ms) {
        zloop_timer(looper, integ_link_ms, 0, handle_timer_link, &ad);
    }
    else {
        zsys_info("stats: link integral durationn is zero, no link stats"); 
    }
    if (integ_chan_ms > 0) {
        zloop_timer(looper, integ_chan_ms, 0, handle_timer_chan, &ad);
    }
    else {
        zsys_info("stats: channel integral duration is zero, no channel stats"); 
    }
    
    // fixme: how to handle dichotomy of explicit shutdown message vs interupt?
    zloop_start(looper);

    zloop_destroy(&looper);
    zsock_destroy(&ad.isock);
    zsock_destroy(&ad.osock);
}

ptmp::TPStats::TPStats(const std::string& config)
    : m_actor(zactor_new(stats, (void*)config.c_str()))
{
}

ptmp::TPStats::~TPStats()
{
    // actor may already have exited so sending this signal can hang.
    zsys_debug("status: sending actor quit message");
    zsock_signal(zactor_sock(m_actor), 0); // signal quit
    zactor_destroy(&m_actor);
}
PTMP_AGENT(ptmp::TPStats, stats)

