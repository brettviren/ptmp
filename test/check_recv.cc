#include "ptmp/api.h"

#include "json.hpp"
#include "CLI11.hpp"

#include <czmq.h>
#include <iostream>

using namespace std;
using json = nlohmann::json;

namespace lvl {
    enum verbose_level {quiet, error, warn, notice, info, debug };
}

int main(int argc, char* argv[])
{
    CLI::App app{"Receive TPSets"};

    int count=0;
    app.add_option("-n,--ntpsets", count,
                   "Number of TPSet messages to send.  0 (default) runs for ever");

    int verbose=0;
    app.add_option("-v,--verbose-level", verbose,
                   "How verbose to be.  0:quiet, 1:error, 2:warn, 3:notice, 4:info, 5:debug");    

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

    cerr << "verbose: " << verbose << endl;

    ptmp::TPReceiver recv(jcfg.dump());

    ptmp::data::TPSet tps;

    const int64_t tbeg = zclock_usecs();
    int ntardy=0, nbogus=0;
    int nn_sum=0, nn2_sum=0, nn_max=0, nn_zero=0;
    int64_t dt_sum=0, dt2_sum=0;
    int last_seen = 0;
    int got = 0;
    int ntimeout = 3;
    uint64_t last_time=0;
    int ind = -1;
    while (true) {
        if (count > 0 and got >= count) {
            if (verbose >= lvl::notice) {
                zsys_notice("got %d messages ending loop", got);
            }
            break;
        }
        ++ind;
        bool ok = recv(tps, timeout);
        if (!ok) {
            if (verbose >= lvl::notice) {
                zsys_notice("timeout at loop %d after %d ms",
                            ind, timeout);
            }
            --ntimeout;
            if (ntimeout <= 0) {
                if (verbose >= lvl::notice) {
                    zsys_notice("reached number of tieouts");
                }
                break;
            }
            continue;
        }
        if (tps.tstart() == -1) {
            zsys_notice("skipping bogus tstart, detid:%d #%d", tps.detid(), tps.count());
            ++nbogus;
            continue;
        }
        ntimeout = 3;
        ++got;
        const int now = tps.count();
        int64_t dt=0;
        if (last_time > 0) {
            dt = tps.tstart() - last_time;
            dt_sum += dt;
            dt2_sum += dt*dt;
        }
        if (dt < 0) {
            zsys_error("skipping tardy: detid:%d #%d, dt=%ld, this tstart=%ld, last tstart=%ld",
                       tps.detid(), tps.count(), dt, tps.tstart(), last_time);
            ++ntardy;
            continue;
        }
        last_time = tps.tstart();

        const int latency = (zclock_usecs() - tps.created())/1000;
        const int dt_ms = dt/1000;
        const int ntps = tps.tps().size();
        if (ntps == 0) ++nn_zero;
        if (ntps > nn_max) { nn_max = ntps; }
        nn_sum += ntps;
        nn2_sum += ntps*ntps;
        if (verbose >= lvl::debug or (verbose >= lvl::notice and ind==0)) {
            zsys_debug("recv: det=%d n=%d dt=%d ms #%d, have %d/%d : %f s, tstart=%ld, latency=%d ms",
                       tps.detid(), ntps,
                       dt_ms, now, got, count, (zclock_usecs()-tbeg)*1e-6,
                       tps.tstart(),
                       latency);
        }

        last_seen = now;
    }
    const int64_t tend = zclock_usecs();

    const double dt = (tend-tbeg)*1e-6;
    const double khz = 0.001*count/dt;
    const double ngot = got;
    const double dtmean = dt_sum /ngot;
    const double dtvar = sqrt(dt2_sum/ngot - dtmean*dtmean);
    const double nmean = nn_sum/ngot;
    const double nsig = sqrt(nn2_sum/ngot - nmean*nmean);
    zsys_info("recv %d/%d in %.3f s or %.1f kHz, ntardy=%d, nbogus=%d, n_zero=%d, n_sum=%d, max(n)=%d <n>=%f stdev(n)=%f <dt>=%f stdev(dt)=%f",
              got, count, dt, khz, ntardy, nbogus, nn_zero, nn_sum, nn_max, nmean, nsig, dtmean, dtvar);

    std::cerr << "check_recv exiting after "
              << got << " / " << count << "\n";
    return 0;
}
