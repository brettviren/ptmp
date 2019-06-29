#include "ptmp/data.h"
#include <chrono>
#include <czmq.h>
#include <cstdio>

using namespace ptmp::data;

void ptmp::data::dump(const ptmp::data::TPSet& tpset, const std::string& msg)
{
    const int ntps = tpset.tps_size();
    data_time_t tmin=0, tmax=0, tbeg_max=0, tend_min=0;

    for (int ind=0; ind<ntps; ++ind) {
        const auto& tp = tpset.tps(ind);
        const data_time_t tstart = tp.tstart();
        const data_time_t tspan = tp.tspan();
        if (!ind) {
            tbeg_max = tmin = tstart;
            tend_min = tmax = tstart + tspan;
            continue;
        }
        tmin = std::min(tmin, tstart);
        tmax = std::max(tmax, tstart + tspan);
        tbeg_max = std::max(tbeg_max, tstart);
        tend_min = std::min(tend_min, tstart + tspan);
    }

    const auto tstart = tpset.tstart();
    const auto tspan = tpset.tspan();

    const real_time_t tnow = now();
    const real_time_t lat = tnow - tpset.created();
    // hardware clock minus real time clock
    const real_time_t hmr = tpset.tstart() - tnow;

    zsys_debug("TPSet #%-8d from 0x%x, %5ld TPs, window %-6ld (TP span [%ld,(%ld,%ld),%ld] = %ld) @ %ld lat:%ldus hmr:%ld now:%ld created:%ld %s",
               tpset.count(), tpset.detid(),
               ntps,
               tspan,
               tmin-tstart,
               tbeg_max - tstart,
               tend_min - tstart,
               tmax-tstart,
               tmax-tmin,
               tstart,
               lat, hmr, now, tpset.created(),
               msg.c_str());               
    
}


ptmp::data::real_time_t ptmp::data::now()
{
    // quite the mouthful
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}


