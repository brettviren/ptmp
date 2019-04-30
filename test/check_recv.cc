#include "ptmp/api.h"

#include "json.hpp"
#include "CLI11.hpp"

#include <czmq.h>
#include <iostream>

using namespace std;
using json = nlohmann::json;

int main(int argc, char* argv[])
{
    CLI::App app{"Receive TPSets"};

    int count=0;
    app.add_option("-n,--ntpsets", count,
                   "Number of TPSet messages to send.  0 (default) runs for ever")->required();

    int hwm = 1000;
    app.add_option("-m,--socket-hwm", hwm,
                   "The ZeroMQ socket highwater mark");    
    std::string socktype="PUB";
    app.add_option("-p,--socket-pattern", socktype,
                   "The ZeroMQ socket pattern for endpoint [PUB, PAIR, PUSH]");
    std::string attach="bind";
    app.add_option("-a,--socket-attachment", attach,
                   "The socket attachement method [bind|connect] for this endpoint");
    std::vector<std::string> endpoints;
    app.add_option("-e,--socket-endpoints", endpoints,
                   "The socket endpoint addresses in ZeroMQ format (tcp:// or ipc://)")->expected(-1);

    int timeout=-1;
    app.add_option("-T,--timeout-ms", timeout,
                   "Number of ms to wait for a message, default is indefinite (-1)");

    CLI11_PARSE(app, argc, argv);


    json jcfg;
    jcfg["socket"]["hwm"] = hwm;
    jcfg["socket"]["type"] = socktype;
    for (const auto& endpoint : endpoints) {
        jcfg["socket"][attach].push_back(endpoint);
    }
    //cerr << jcfg << endl;

    ptmp::TPReceiver recv(jcfg.dump());

    ptmp::data::TPSet tps;

    auto tbeg = zclock_usecs();
    int last_seen = 0;
    int got = 0;
    int ntimeout = 3;
    uint64_t last_time=0;
    int ind = -1;
    while (true) {
        if (got >= count) {
            break;
        }
        ++ind;
        bool ok = recv(tps, timeout);
        if (!ok) {
            zsys_info("timeout at loop %d after %d ms",
                      ind, timeout);
            --ntimeout;
            if (ntimeout <= 0) {
                break;
            }
            continue;
        }
        ntimeout = 3;
        ++got;
        const int now = tps.count();
        int64_t dt=0;
        if (last_time == 0) {
            last_time = tps.tstart();
        }
        else {
            dt = tps.tstart() - last_time;
            last_time = tps.tstart();
        }
        assert (dt >= 0);

        zsys_debug("recv: dt=%ld #%d, have %d/%d : %f s",
                   dt, now, got, count, (zclock_usecs()-tbeg)*1e-6);
        last_seen = now;
    }
    auto tend = zclock_usecs();

    const double dt = (tend-tbeg)*1e-6;
    const double khz = 0.001*count/dt;
    zsys_info("recv %d/%d in %.3f s or %.1f kHz",
              got, count, dt, khz);

    std::cerr << "check_recv exiting after "
              << got << " / " << count << "\n";
    return (got == 0);
}
