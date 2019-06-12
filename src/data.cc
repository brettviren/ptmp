#include "ptmp/data.h"

#include <czmq.h>

using namespace ptmp::data;

void ptmp::data::dump(const ptmp::data::TPSet& tpset, const std::string& msg)
{
    const auto& tps = tpset.tps();
    const size_t ntps = tps.size();
    int64_t tmin=0, tmax=0, tbeg_max=0, tend_min=0;


    for (size_t ind=0; ind<ntps; ++ind) {
        const auto& tp = tps[ind];
        const int64_t tstart = tp.tstart();
        const int64_t tspan = tp.tspan();
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

    const int64_t now = zclock_usecs();
    const int64_t lat = now - tpset.created();
    // hardware clock minus real time clock
    const int64_t hmr = tpset.tstart() - now;

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

