#include "SocketStats.h"

using namespace ptmp::noexport;
using namespace nlohmann;

void time_stats_t::update(int64_t now, int64_t then)
{
    tf = now;
    int64_t lat = now - then;
    lat += lat;
    lat2 += lat*lat;
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
    real.update(real_now, tpset.created());
    data.update(real_now, data_now);
}

void time_stats_t::finalize(double n)
{
    dt = tf - t0;
    hz = 0.0;
    if (dt != 0) {
        hz = 1.0e6/dt;
    }
    if (n <= 0) {
        return;
    }
    lat_mean = lat/n;
    double var = std::max(0.0, lat2/n - lat_mean*lat_mean);
    lat_rms = sqrt(var);
}

void socket_stats_t::finalize()
{
    real.finalize(ntpsets);
    data.finalize(ntpsets);
}

json time_stats_t::jsonify()
{
    return json{ {"mean", lat_mean}, {"rms", lat_rms} };
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
    return j;
}
