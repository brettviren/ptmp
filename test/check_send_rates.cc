// Send TPSets filled with TPs following various models for rates

// See also check_send.cc which sends empty TPSets at regular intervals.

#include "ptmp/api.h"
#include "ptmp/testing.h"

#include "CLI11.hpp"
#include "json.hpp"

#include <czmq.h>
#include <iostream>
#include <random>

#include <unistd.h>



using namespace std;
using json = nlohmann::json;


int main(int argc, char* argv[])
{
    CLI::App app{"Send TPSets given a model of their rates"};
    
    std::string socktype="PUB";
    app.add_option("-p,--socket-pattern", socktype, "The ZeroMQ socket pattern for endpoint [PUB, PAIR, PUSH]");
    std::string attach="bind";
    app.add_option("-a,--socket-attachment", attach, "The socket attachement method [bind|connect] for this endpoint");
    std::vector<std::string> endpoints;
    app.add_option("-e,--socket-endpoints", endpoints, "The socket endpoint addresses in ZeroMQ format (tcp:// or ipc://)")->expected(-1);

    int begwait=0;
    app.add_option("-B,--begin-wait-ms", begwait, "Number of ms to wait between creating socket and starting to send");
    int endwait=0;
    app.add_option("-E,--end-wait-ms", endwait, "Number of ms to wait between completing --count messages and terminating");

    int ntpsets=0;
    app.add_option("-n,--ntpsets", ntpsets, "Number of TPSet messages to send.  0 (default) runs for ever");
    int ntps = 1;
    app.add_option("-N,--ntps", ntps, "Number of (mean or fixed) TPs per TPSet");
    int vartps = 0;
    app.add_option("-V,--vartps", vartps, "Width of distribution of number of TPs per TPSet");

    app.require_subcommand();

    CLI::App *uniform = app.add_subcommand("uniform", "Use a uniform message rate model");

    int usleeptime=0, usleepskip=0;
    uniform->add_option("-t,--time-us", usleeptime, "Number of us to sleep");
    uniform->add_option("-s,--sleep-skip", usleepskip, "Number of sleeps to skip");

    CLI::App *exponential = app.add_subcommand("exponential", "Use a Exponential message rate model");
    exponential->add_option("-t,--time-us", usleeptime, "Mean sleep time in us");
    exponential->add_option("-s,--sleep-skip", usleepskip, "Number of sleeps to skip");

    
    CLI11_PARSE(app, argc, argv);


    std::function<void()> sleepy_time;
    if (app.got_subcommand("exponential")) {
        sleepy_time = ptmp::testing::exponential_sleeps_t(usleeptime, usleepskip);
    }
    if (app.got_subcommand("uniform")) {
        sleepy_time = ptmp::testing::uniform_sleeps_t(usleeptime, usleepskip);
    }
    else {
        sleepy_time = ptmp::testing::uniform_sleeps_t(0,0);
    }

    json jcfg;
    jcfg["socket"]["type"] = socktype;
    for (const auto& endpoint : endpoints) {
        jcfg["socket"][attach].push_back(endpoint);
    }
    cerr << jcfg << endl;
    ptmp::TPSender send(jcfg.dump());

    ptmp::data::TPSet tps;
    ptmp::testing::init(tps);

    // Avoid "late joiner" syndrom
    zclock_sleep (begwait);

    ptmp::testing::make_tps_t make_tps(ntps, vartps);

    auto tbeg = zclock_usecs();
    for (int ind=0; ind<ntpsets; ++ind) {
        sleepy_time();
        make_tps(tps);
        send(tps);
    }
    auto tend = zclock_usecs();

    const double dt = (tend-tbeg)*1e-6;
    const double khz = 0.001*ntpsets/dt;
    zsys_info("send %d in %.3f s or %.1f kHz", ntpsets, dt, khz);

    zclock_sleep(endwait);

    return 0;
}
