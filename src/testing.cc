#include "ptmp/api.h"
#include "ptmp/testing.h"

#include "json.hpp"

#include <czmq.h>

#include <algorithm>

using json = nlohmann::json;

const int ticks_per_us = 50;


void ptmp::testing::init(ptmp::data::TPSet& tps)
{
    tps.set_count(0);
    tps.set_created(ptmp::data::now());
    tps.set_detid(4); // guaranteed random by roll of dice, but call only once
}

void ptmp::testing::set_count_clock(ptmp::data::TPSet& tps)
{

    const int oldcount = tps.count();
    tps.set_count(oldcount + 1);

    // Note, in general one can NOT compare real and data time in an
    // absolute sense (unless one knows the epoch for the data time).
    // This is a bodge just to set something for testing.
    ptmp::data::real_time_t now = ptmp::data::now();
    ptmp::data::data_time_t ticks = now * ticks_per_us;
    ptmp::data::real_time_t bogus_latency = 100*ticks_per_us;
    tps.set_tstart(ticks - bogus_latency);

    tps.set_created(now);
}


ptmp::testing::make_tps_t::make_tps_t(int ntps, int vartps)
    : normal_dist(ntps, vartps)
{
}

void ptmp::testing::make_tps_t::operator()(ptmp::data::TPSet& tps) {
    tps.clear_tps();
    ptmp::testing::set_count_clock(tps);
    int ntps = std::max(0, (int)normal_dist(generator));
    const ptmp::data::data_time_t t0 = tps.tstart();
    ptmp::data::data_time_t tend=0;
    for (int ind=0; ind<ntps; ++ind) {
        auto tp = tps.add_tps();

        // note, these are physically bogus values.  The values are
        // chosen to somewhat represent realistic values.  In
        // particular, zeros are avoided so that protobuf compression
        // doesn't give an unrealistic size.

        const int tspan = ((1+ind)*10/2)*ticks_per_us;
        const ptmp::data::data_time_t tstart = t0 + ind*10*ticks_per_us;
        const int adc = ind+10;

        // required values
        tp->set_channel(ind);
        tp->set_tstart(tstart);

        tend = tstart + tspan;

        tp->set_adcsum(adc*tspan/2);
        tp->set_adcpeak(ind/tspan);
        tp->set_flags(0);
    }
    tps.set_tspan(tend - t0);
    tps.set_created(ptmp::data::now());
}

ptmp::testing::uniform_sleeps_t::uniform_sleeps_t(int t, int s)
    : usleeptime(t), usleepskip(s), count(0)
{
}

void ptmp::testing::uniform_sleeps_t::operator()()
{
    ++count;
    
    if (usleeptime == 0) { return; }
    if (usleepskip>0 and count % usleepskip != 1) { return; }
    usleep(usleeptime);
}

ptmp::testing::exponential_sleeps_t::exponential_sleeps_t(int t, int s)
    : usleeptime(t), usleepskip(s), count(0), expo_dist(1.0/t)
{
}

void ptmp::testing::exponential_sleeps_t::operator()() {
    ++count;
    if (usleeptime == 0) { return; }
    if (usleepskip>0 and count % usleepskip != 1) { return; }
    double us = expo_dist(generator);
    usleep((int)us);
}

