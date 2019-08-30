#include "SocketStats.h"

using namespace ptmp::noexport;
using namespace nlohmann;

void time_stats_t::update(int64_t now, int64_t then)
{
    ++num;
    int64_t this_lat = now - then;
    lat += this_lat;
    lat2 += this_lat*this_lat;
}
void socket_stats_t::update(ptmp::data::TPSet& tpset, int tickperus)
{
    int64_t real_now = ptmp::data::now();
    int64_t data_now = tpset.tstart() / tickperus;
    if (ntpsets++ == 0) {
        real.t0 = real_now;
        data.t0 = data_now;
    }
    ntps += tpset.tps_size();
    real.tf = real_now;
    real.update(real_now, tpset.created());
    data.tf = data_now;
    data.update(real_now, data_now);
}

void time_stats_t::finalize()
{
    dt = tf - t0;
    hz = 0.0;
    if (dt != 0) {
        hz = 1.0e6/dt;
    }
    if (num <= 0) {
        return;
    }
    const double fnum = num;
    lat_mean = lat/fnum;
    double var = std::max(0.0, lat2/fnum - lat_mean*lat_mean);
    lat_rms = sqrt(var);
}

void socket_stats_t::finalize()
{
    real.finalize();
    data.finalize();
}

json time_stats_t::jsonify()
{
    return json{ {"num", num}, {"mean", lat_mean}, {"rms", lat_rms} };
}

json socket_stats_t::jsonify()
{
    json lat;
    lat["real"] = real.jsonify();
    lat["data"] = data.jsonify();
    json j;
    j["latency"] = lat;
    json rates;
    rates["tps"] = ntps * data.hz;
    rates["tpsets"] = ntpsets * real.hz;
    j["rates"] = rates;
    json counts;
    counts["dtdata"] = data.dt;
    counts["dtreal"] = real.dt;
    counts["tps"] = ntps;
    counts["tpsets"] = ntpsets;
    j["counts"] = counts;
    return j;
}
