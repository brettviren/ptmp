#include "ptmp/api.h"
#include "ptmp/internals.h"
#include "ptmp/factory.h"
#include "json.hpp"

PTMP_AGENT(ptmp::TPStats, stats)

using json = nlohmann::json;

// fixme: internalize this
const int json_message_type = 0x4a534f4e; // 1246973774 in decimal

// Accumulate per channel statistics
struct channel_stats_t {
    uint32_t ntps{0};
    uint64_t span{0};           // integral of TP tspans
    uint64_t adcsum{0};         // integral of TP ADC
    uint64_t adcpeak{0};        // integral of TP ADC peak values
};

struct link_stats_t {
    uint32_t ntpsets{0}, ntps{0};
    uint32_t total_adcsum{0}, total_adcpeak{0};
    uint64_t tstart{0}, tstart_span{0}, total_tp_tspan{0};
    int64_t tcreated{0}, tcreated_span{0};
    int64_t treceived{0}, treceived_span{0};
    // sum and sum squared used to calculate mean and rms
    // sum of trecieved-tstart, tcreated-tstart.  
    int64_t dtr_sum{0}, dtc_sum{0};           
    // sum of (trecieved-tstart)^2, (tcreated-tstart)^2.
    int64_t dtr_sum2{0}, dtc_sum2{0};           
    // min/max time between recieved and created relative to tstart
    int64_t dtr_min{0}, dtr_max{0}, dtc_min{0}, dtc_max{0};
    // channel number to channel stats
    std::unordered_map<int, channel_stats_t> ch_stats;
};
static
void to_json(json& j, const link_stats_t& ls)
{
    uint64_t adcsum{0}, adcpeak{0};
    for (const auto& chs : ls.ch_stats) {
        adcsum += chs.second.adcsum;
        adcpeak += chs.second.adcpeak;
    }

    j = json{
        {"ntpsets", ls.ntpsets},
        {"ntpsperlink", ls.ntps},
        {"total_adcsum", adcsum},
        {"total_adcpeak", adcpeak},
        {"tstart", ls.tstart},
        {"tstart_span", ls.tstart_span},
        {"total_tp_tspan", ls.total_tp_tspan},
        {"tcreated", ls.tcreated},
        {"tcreated_span", ls.tcreated_span},
        {"treceived", ls.treceived},
        {"treceived_span", ls.treceived_span},

        {"dtr_sum", ls.dtr_sum},
        {"dtc_sum", ls.dtc_sum},
        {"dtr_sum2",ls.dtr_sum2},
        {"dtc_sum2",ls.dtc_sum2},
        {"dtr_min", ls.dtr_min},
        {"dtc_min", ls.dtc_min},
        {"dtr_max", ls.dtr_max},
        {"dtc_max", ls.dtc_max},
    };

    for (const auto& ccs : ls.ch_stats) {
        const channel_stats_t& ch = ccs.second;
        json jc{
            {"ntps", ch.ntps},
            {"span", ch.span},
            {"adcsum", ch.adcsum},
            {"adcpeak", ch.adcpeak}
        };
        char chname[32];
        snprintf(chname, 32, "ch%d", ccs.first);
        j[chname] = jc;
    }
}

struct app_data_t {
    zsock_t* isock{NULL}, *osock{NULL};
    int tick_per_us{0}, tick_off_us{0};
    int sequence{0};

    std::unordered_map<int, link_stats_t> stats;
    
    void add(ptmp::data::TPSet& tpset) {
        auto& s = stats[tpset.detid()];

        auto tstart = tpset.tstart()/tick_per_us - tick_off_us;
        auto tcreat = tpset.created();
        auto trecv = ptmp::data::now();

        if (s.ntpsets == 0) {
            s.tstart = tstart;
            s.tcreated = tcreat;
            s.treceived = trecv;
        }
        s.tstart_span = tstart - s.tstart;
        s.tcreated_span = tcreat - s.tcreated;
        s.treceived_span = trecv - s.treceived;

        int64_t dtr = trecv - tstart;
        int64_t dtc = tcreat - tstart;

        // zsys_debug("0x%x ntpsets: %ld, tstart: %ld, tstart_span: %ld, dtr:%ld, dct:%ld",
        //            tpset.detid(),
        //            s.ntpsets, s.tstart, s.tstart_span, dtr, dtc);

        s.dtr_sum += dtr;
        s.dtc_sum += dtc;
        s.dtr_sum2 += dtr*dtr;
        s.dtc_sum2 += dtc*dtc;
        if (s.ntpsets == 0) {
            s.dtr_min = s.dtr_max = dtr;
            s.dtc_min = s.dtc_max = dtc;
        }
        else {
            s.dtr_min = std::min(s.dtr_min, dtr);
            s.dtr_max = std::max(s.dtr_max, dtr);
            s.dtc_min = std::min(s.dtc_min, dtc);
            s.dtc_max = std::max(s.dtc_max, dtc);
        }
        s.ntpsets += 1;
        s.ntps += tpset.tps_size();
        for (auto& tp : tpset.tps()) {
            const int ch = tp.channel();
            auto& cs = s.ch_stats[ch];
            cs.ntps += 1;
            cs.span += tp.tspan();
            cs.adcsum += tp.adcsum();
            cs.adcpeak += tp.adcpeak();
        }
    }

};

static
int handle_pipe(zloop_t* loop, zsock_t* sock, void* varg)
{
    //app_data_t* ad = (app_data_t*)varg;
    // message on pipe means "shutdown"
    return -1;
}

static
int handle_input(zloop_t* loop, zsock_t* sock, void* varg)
{
    app_data_t* ad = (app_data_t*)varg;

    zmsg_t* msg = zmsg_recv(sock);
    ptmp::data::TPSet tpset;
    ptmp::internals::recv(&msg, tpset);
    ad->add(tpset);
    return 0;
}

static
int handle_timer(zloop_t* loop, int timer_id, void* varg)
{
    // when we are in here, input is piling up!  be brief.

    app_data_t* ad = (app_data_t*)varg;

    json jdat;
    for (const auto& sit : ad->stats) {
        const link_stats_t ls = sit.second;
        json jls = ls; // magic
        jls["seqno"]=ad->sequence;

        int detid = sit.first;
        char sdetid[32];
        snprintf(sdetid, 32, "0x%x", detid);


        jdat[sdetid] = jls;
    }
    std::string sdat = jdat.dump();
    // zsys_debug("%s", sdat.c_str());

    // PTMP protocol puts a type as first frame.
    zsock_send(ad->osock, "is", json_message_type, sdat.c_str());
    ad->stats.clear();
    ++ ad->sequence;
    return 0;
}

static
void stats(zsock_t* pipe, void* vargs)
{
    auto config = json::parse((const char*) vargs);

    std::string name = "stats";
    if (config["name"].is_string()) {
        name = config["name"];
    }
    ptmp::internals::set_thread_name(name);

    // how long to integrate stats
    int integration_ms = 1000;
    if (config["integration_time"].is_number()) {
        integration_ms = config["integration_time"];
    }
    // How many data ticks per us.
    int tick_per_us = 50;
    if (config["tick_per_us"].is_number()) {
        tick_per_us = config["tick_per_us"];
    }
    // time in us from Unix epoch to data tick 0.
    int tick_off_us = 0;
    if (config["tick_off_us"].is_number()) {
        tick_off_us = config["tick_off_us"];
    }
    zsys_debug("stats: every %d ms, tick/us: %d, tick offset: %d",
               integration_ms, tick_per_us, tick_off_us);

    zsock_t* isock = NULL;
    if (!config["input"].is_null()) {
        std::string cfg = config["input"].dump();
        isock = ptmp::internals::endpoint(cfg);
        if (!isock) {
            throw std::runtime_error("no input socket configured");
        }
    }
    zsock_t* osock = NULL;
    if (!config["output"].is_null()) {
        std::string cfg = config["output"].dump();
        osock = ptmp::internals::endpoint(cfg);
        if (!osock) {
            throw std::runtime_error("no output socket configured");
        }
    }
    zsock_signal(pipe, 0); // signal ready    

    zloop_t *looper = zloop_new();
    app_data_t app_data{isock, osock, tick_per_us, tick_off_us};

    zloop_reader(looper, pipe, handle_pipe, &app_data);
    zloop_reader(looper, isock, handle_input, &app_data);
    int tid = zloop_timer(looper, integration_ms, 0, handle_timer, &app_data);
    
    // fixme: how to handle dichotomy of explicit shutdown message vs interupt?
    zloop_start(looper);

    zloop_destroy(&looper);
    zsock_destroy(&isock);
    zsock_destroy(&osock);
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
