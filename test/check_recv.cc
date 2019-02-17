#include "ptmp/api.h"

#include "json.hpp"

#include <czmq.h>
#include <iostream>

using namespace std;
using json = nlohmann::json;

int main(int argc, char* argv[])
{
    if (argc < 5) {
        cerr << "usage: test_recv N <type> <bind|connect> <endpoint> [timeout]" << endl;
        return 0;
    }

    const int count = atoi(argv[1]);
    int timeout = -1;
    if (argc > 5) {
        timeout = atoi(argv[5]);
    }

    json jcfg;
    jcfg["socket"]["type"] = argv[2];
    jcfg["socket"][argv[3]][0] = argv[4];
    cerr << jcfg << endl;
    
    ptmp::TPReceiver recv(jcfg.dump());

    ptmp::data::TPSet tps;

    auto tbeg = zclock_usecs();
    int last_seen = 0;
    int missed = 0;
    for (int ind=0; ind<count; ++ind) {
        bool ok = recv(tps, timeout);
        if (!ok) {
            cerr << "timeout at " << ind << " after " << timeout
                 << " missed " << missed
                 << "\n";
            break;
        }
        const int now = tps.count();
        //cerr << ind << " - " << now << " = " << ind-now << " : " << (zclock_usecs()-tbeg)*1e-6 << "s\n";
        const int missed_here = now - last_seen - 1;
        if (missed_here > 0) {
            missed += missed_here;
            // zsys_info("skip %d to %d missed %d (tot missed) %d",
            //           last_seen, now, missed_here, missed);
        }
        last_seen = now;
    }
    auto tend = zclock_usecs();

    const double dt = (tend-tbeg)*1e-6;
    const double khz = 0.001*count/dt;
    zsys_info("recv %d in %.3f s or %.1f kHz, missed %d",
              count, dt, khz, missed);

    return 0;
}
