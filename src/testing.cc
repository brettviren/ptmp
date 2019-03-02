#include "ptmp/api.h"
#include "ptmp/testing.h"

#include "json.hpp"

#include <czmq.h>

#include <algorithm>

using json = nlohmann::json;

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
    tps.set_detid(4); // guaranteed random by roll of dice, but call only once
}

void ptmp::testing::set_timestamp(ptmp::data::Timestamp& ts, int64_t t, int64_t latency)
{
    if (t == 0) {
        t = zclock_usecs() * units::us;
    }

    t += latency;

    ts.set_seconds(t/G);
    ts.set_nanosecs(t%G);
}

int64_t ptmp::testing::timestamp_span(const ptmp::data::Timestamp& ta,
                                      const ptmp::data::Timestamp& tb)
{
    const int64_t s = ta.seconds() - tb.seconds();
    const int64_t ns = ta.nanosecs() - tb.nanosecs();
    return s*G + ns;
}
void ptmp::testing::set_relative_timestamp(ptmp::data::Timestamp& ts,
                                           const ptmp::data::Timestamp& t0,
                                           int64_t delay)
{
    const int64_t full_ns = t0.nanosecs() + delay%G;
    const int64_t s = t0.seconds() + (full_ns + delay)/G;
    const int32_t ns = full_ns % G;
    ts.set_seconds(s);
    ts.set_nanosecs(ns);    
}

void ptmp::testing::set_count_clock(ptmp::data::TPSet& tps)
{

    const int oldcount = tps.count();
    tps.set_count(oldcount + 1);

    // The "data time" is some electronics hardware clock shared in
    // some way by all messages.  Whatever units it may be in, we
    // shall store the time in an absolute number of seconds from the
    // Unix epoch and, if applicable, the number of nanoseconds within
    // the second.  Here, we just plop ZMQ's monotonic system clock
    // number and we fake some latency by moving it into the past a
    // little bit.

    // We fake some latency between data clock and wall clock by
    // setting the tstart a bit before wall clock's "now".
    auto tstart = tps.mutable_tstart();
    set_timestamp(*tstart, 0, -100*units::us);

    // Set the created time which really should be the system wall
    // clock "now".  No fake here.
    auto created = tps.mutable_created();
    set_timestamp(*created);
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
    ptmp::data::Timestamp tend;
    for (int ind=0; ind<ntps; ++ind) {
        auto tp = tps.add_tps();

        // note, these are physically bogus values.  The values are
        // chosen to somewhat represent realistic values.  In
        // particular, zeros are avoided so that protobuf compression
        // doesn't give an unrealistic size.

        const int tspan = ((1+ind)*10/2)*units::us;
        const int adc = ind+10;

        // required values
        tp->set_channel(ind);
        auto tstart = tp->mutable_tstart();
        set_relative_timestamp(*tstart, t0, ind*10*units::us);

        set_relative_timestamp(tend, *tstart, tspan);

        // optional values
        tp->set_adcsum(adc*tspan/2);
        tp->set_adcpeak(ind/tspan);
        tp->set_flags(0);
    }

    tps.set_tspan(timestamp_span(t0, tend));
}

ptmp::testing::uniform_sleeps_t::uniform_sleeps_t(int t, int s)
    : usleeptime(t), usleepskip(s), count(0)
{
}

void ptmp::testing::uniform_sleeps_t::operator()()
{
    ++count;
    if (usleeptime == 0) { return; }
    if (usleepskip == 0) { return; }
    if (count % usleepskip != 1) { return; }
    usleep(usleeptime);
}

ptmp::testing::exponential_sleeps_t::exponential_sleeps_t(int t, int s)
    : usleeptime(t), usleepskip(s), count(0), expo_dist((double)t)
{
}

void ptmp::testing::exponential_sleeps_t::operator()() {
    ++count;
    if (usleepskip == 0) { return; }
    if (count % usleepskip != 1) { return; }
    int us = expo_dist(generator);
    usleep(us);
}

