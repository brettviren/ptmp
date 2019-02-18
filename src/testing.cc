#include "ptmp/testing.h"

#include <czmq.h>

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
