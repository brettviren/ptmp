#include "ptmp/testing.h"

#include <czmq.h>

#include <algorithm>

// This is a bare function used to test upif.
extern "C" {
int ptmp_upif_test(int x)
{
    return x;
}
}


void ptmp::testing::init(ptmp::data::TPSet& tps)
{
    tps.set_count(0);
    tps.set_detid(4); // guaranteed random by roll of dice, but call only once
}

void ptmp::testing::set_count_clock(ptmp::data::TPSet& tps)
{

    const int oldcount = tps.count();
    tps.set_count(oldcount + 1);

    const int64_t M = 1000000;
    const int64_t us = zclock_usecs();
    const int64_t s = us/M;
    const int64_t ns = (us%M) * 1000;


    // normally, "data time" is some electronics hardware clock shared
    // in some whay by all messages.  It is not necessarily expressed
    // in units of seconds/us/ns.  It may be relative to some global
    // DAQ initialization time.  Here, we just plop ZMQ's monotonic
    // system clock number and we fake some latency by moving it into
    // the past a bit.
    tps.set_datatime(us - 100);       

    // system clock 
    tps.set_unixseconds(s);
    tps.set_unixns(ns);
}


ptmp::testing::make_tps_t::make_tps_t(int ntps, int vartps)
    : normal_dist(ntps, vartps)
{
}

void ptmp::testing::make_tps_t::operator()(ptmp::data::TPSet& tps) {
    tps.clear_tps();
    ptmp::testing::set_count_clock(tps);
    int ntps = std::max(0, (int)normal_dist(generator));
    for (int ind=0; ind<ntps; ++ind) {
        auto tp = tps.add_tps();

        // note, these are physically bogus values.  The values are
        // chosen to somewhat represent realistic values.  In
        // particular, zeros are avoided so that protobuf compression
        // doesn't give an unrealistic size.

        const int tspan = (1+ind)*10;
        const int adc = ind+10;

        // required values
        tp->set_channel(ind);
        tp->set_tstart(ind*100);
        tp->set_tspan(tspan);

        // optional values
        tp->set_adcsum(adc*tspan/2);
        tp->set_adcpeak(ind/tspan);
        tp->set_flags(0);
    }
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
