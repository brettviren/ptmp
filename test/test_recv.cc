#include "ptmp/api.h"

#include "json.hpp"

#include <czmq.h>
#include <iostream>

using namespace std;
using json = nlohmann::json;

int main(int argc, char* argv[])
{
    if (argc < 5) {
        cerr << "usage: test_recv N <type> <bind|connect> <endpoint>" << endl;
        return 0;
    }

    const int count = atoi(argv[1]);

    json jcfg;
    jcfg["socket"]["type"] = argv[2];
    jcfg["socket"][argv[3]][0] = argv[4];
    cerr << jcfg << endl;
    
    ptmp::TPReceiver recv(jcfg.dump());

    ptmp::data::TPSet tps;

    auto tbeg = zclock_usecs();
    for (int ind=0; ind<count; ++ind) {
        recv(tps);
        const int now = tps.count();
        cerr << ind << " - " << now << " = " << ind-now << " : " << (zclock_usecs()-tbeg)*1e-6 << "s\n";
    }
    auto tend = zclock_usecs();

    const double dt = (tend-tbeg)*1e-6;
    const double khz = 0.001*count/dt;
    zsys_info("recv %d in %.3f s or %.1f kHz", count, dt, khz);

    return 0;
}
