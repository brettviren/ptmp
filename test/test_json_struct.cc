#include "json.hpp"
#include <iostream>

using json = nlohmann::json;
struct stream_stats_t {
    int ntpsets{0}, ntps{0};
    uint32_t total_adcsum{0}, total_adcpeak{0};
    uint64_t tstart{0}, tstart_span{0}, total_tp_tspan{0};
    int64_t tcreated{0}, tcreated_span{0};
    int64_t treceived{0}, treceived_span{0};
    // sum of trecieved-tstart, tcreated-tstart.
    int64_t dtr_sum{0}, dtc_sum{0};           
    // sum of (trecieved-tstart)^2, (tcreated-tstart)^2.
    int64_t dtr_sum2{0}, dtc_sum2{0};           
};

void to_json(json& j, const stream_stats_t& ss)
{
    j = json{
        {"ntpsets", ss.ntpsets},
        {"ntps", ss.ntps},
        {"total_adcsum", ss.total_adcsum},
        {"total_adcpeak", ss.total_adcpeak},
        {"tstart", ss.tstart},
        {"tstart_span", ss.tstart_span},
        {"total_tp_tspan", ss.total_tp_tspan},
        {"tcreated", ss.tcreated},
        {"tcreated_span", ss.tcreated_span},
        {"treceived", ss.treceived},
        {"treceived_span", ss.treceived_span},
        {"dtr_sum", ss.dtr_sum},
        {"dtc_sum", ss.dtc_sum},
        {"dtr_sum2", ss.dtr_sum2},
        {"dtc_sum2", ss.dtc_sum2}
    };
}

int main()
{
    stream_stats_t ss;
    json jss = ss;
    std::cout << jss.dump(4) << std::endl;
}    
