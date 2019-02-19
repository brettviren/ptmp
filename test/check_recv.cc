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
    app.add_option("-n,--ntpsets", count, "Number of TPSet messages to send.  0 (default) runs for ever")->required();

    std::string socktype="PUB";
    app.add_option("-p,--socket-pattern", socktype, "The ZeroMQ socket pattern for endpoint [PUB, PAIR, PUSH]");
    std::string attach="bind";
    app.add_option("-a,--socket-attachment", attach, "The socket attachement method [bind|connect] for this endpoint");
    std::vector<std::string> endpoints;
    app.add_option("-e,--socket-endpoints", endpoints, "The socket endpoint addresses in ZeroMQ format (tcp:// or ipc://)")->expected(-1);

    int timeout=-1;
    app.add_option("-T,--timeout-ms", timeout, "Number of ms to wait for a message, default is indefinite (-1)");

    CLI11_PARSE(app, argc, argv);


    json jcfg;
    jcfg["socket"]["type"] = socktype;
    for (const auto& endpoint : endpoints) {
        jcfg["socket"][attach].push_back(endpoint);
    }
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
