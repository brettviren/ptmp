#include "ptmp/api.h"
#include "ptmp/testing.h"

#include "json.hpp"

#include <czmq.h>

#include <algorithm>

using json = nlohmann::json;

const int ticks_per_us = 50;

const int64_t K = 1000;
const int64_t M = 1000*K;
const int64_t G = 1000*M;

namespace units {
    const int64_t ns = 1;
    const int64_t us = 1000*ns;
    const int64_t ms = 1000*us;
    const int64_t s = 1000*ms;
}

void ptmp::testing::init(ptmp::data::TPSet& tps)
{
    tps.set_count(0);
    tps.set_created(zclock_usecs());
    tps.set_detid(4); // guaranteed random by roll of dice, but call only once
}

void ptmp::testing::set_count_clock(ptmp::data::TPSet& tps)
{

    const int oldcount = tps.count();
    tps.set_count(oldcount + 1);

    // The "data time" is in units of "ticks" of the electronics
    // hardware clock.  This is the 50 MHz clock in ProtoDUNE.  Here,
    // just for testing, we plop ZMQ's monotonic system clock number
    // and we fake some latency by moving it into the past a little
    // bit.

    int64_t us = zclock_usecs();
    uint64_t ticks = us * ticks_per_us;
    const int bogus_latency = 100*ticks_per_us;
    tps.set_tstart(ticks - bogus_latency);

    // Set the created time which really is just the system wall clock
    // "now" in Unix time_t.  No fake here.
    time_t unix_t = us/1000000;
    tps.set_created(unix_t);
}


ptmp::testing::make_tps_t::make_tps_t(int ntps, int vartps)
    : normal_dist(ntps, vartps)
{
}

void ptmp::testing::make_tps_t::operator()(ptmp::data::TPSet& tps) {
    tps.clear_tps();
    ptmp::testing::set_count_clock(tps);
    int ntps = std::max(0, (int)normal_dist(generator));
    const auto& t0 = tps.tstart();
    uint64_t tend;
    for (int ind=0; ind<ntps; ++ind) {
        auto tp = tps.add_tps();

        // note, these are physically bogus values.  The values are
        // chosen to somewhat represent realistic values.  In
        // particular, zeros are avoided so that protobuf compression
        // doesn't give an unrealistic size.

        const int tspan = ((1+ind)*10/2)*ticks_per_us;
        const uint64_t tstart = t0 + ind*10*ticks_per_us;
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
    int64_t created = zclock_usecs();
    tps.set_created(created);
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

