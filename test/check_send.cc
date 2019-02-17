#include "ptmp/api.h"

#include "json.hpp"

#include <czmq.h>
#include <iostream>

#include <unistd.h>

using namespace std;
using json = nlohmann::json;

int main(int argc, char* argv[])
{
    if (argc < 5) {
        cerr << "usage: test_send N <type> <bind|connect> <endpoint> [begwait] [endwait]" << endl;
        return 0;
    }

    const int count = atoi(argv[1]);
    int begwait=0, endwait = 0;
    if (argc > 5) {
        begwait = atoi(argv[5]);
    }
    if (argc > 6) {
        endwait = atoi(argv[6]);
    }

    json jcfg;
    jcfg["socket"]["type"] = argv[2];
    jcfg["socket"][argv[3]][0] = argv[4];
    cerr << jcfg << endl;
    
    ptmp::TPSender send(jcfg.dump());

    ptmp::data::TPSet tps;

    // Avoid "late joiner" syndrom
    zclock_sleep (begwait);

    auto tbeg = zclock_usecs();
    for (int ind=0; ind<count; ++ind) {
        tps.set_count(ind);
        send(tps);
        //cerr << ind << ": " << (zclock_usecs()-tbeg)*1e-6 << endl;

        // ZeroMQ sends SO fast that if there is no application
        // payload processing slowing it down any hiccup will cause
        // buffers to slam to the HWM.  Artificial loads can be tested
        // with ms or us sleep.  If app provides packets faster than
        // one per about a microsecond, the send/recv HWM can be
        // increased.

        //zclock_sleep(1);

        //usleep(1);
    }
    auto tend = zclock_usecs();

    const double dt = (tend-tbeg)*1e-6;
    const double khz = 0.001*count/dt;
    zsys_info("send %d in %.3f s or %.1f kHz", count, dt, khz);

    zclock_sleep(endwait);

    return 0;
}
