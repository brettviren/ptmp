#include "ptmp/api.h"
#include "ptmp/cmdline.h"

#include <vector>
#include <string>
#include <iostream>

using namespace std;
using json = nlohmann::json;
using namespace ptmp::cmdline;

int main(int argc, char* argv[])
{
    CLI::App app{"A proxy to replay TPSet streams"};

    int rewrite_count = 0;
    app.add_option("-C,--rewrite-count", rewrite_count,
                   "rewrite the TPSet.count values");


    double speed = 50.0;
    app.add_option("-s,--speed", speed,
                   "Hardware clock ticks to replay per real time microsecond");
    int countdown = -1;         // forever
    app.add_option("-c,--count", countdown,
                   "Number of snoozes before exiting");
    int snooze = 5000;         // ms
    app.add_option("-z,--snooze", snooze,
                   "Number of ms to snoze");

    CLI::App* isocks = app.add_subcommand("input", "Input socket specification");
    CLI::App* osocks = app.add_subcommand("output", "Output socket specification");
    app.require_subcommand(2);

    sock_options_t iopt{1000,"SUB", "connect"}, oopt{1000,"PUB", "bind"};
    add_socket_options(isocks, iopt);
    add_socket_options(osocks, oopt);

    CLI11_PARSE(app, argc, argv);
    
    json jcfg;
    jcfg["input"] = to_json(iopt);
    jcfg["output"] = to_json(oopt);
    jcfg["speed"] = speed;
    jcfg["rewrite_count"] = rewrite_count;
    
    std::cerr << "Using config: " << jcfg << std::endl;

    std::string cfgstr = jcfg.dump();

    {
        ptmp::TPReplay proxy(cfgstr);

        while (countdown != 0) {
            -- countdown;
            ptmp::data::real_time_t t1 = ptmp::data::now();
            zclock_sleep(snooze);
            ptmp::data::real_time_t t2 = ptmp::data::now();
            if (std::abs((t2-t1)/1000-snooze) > 10) {
                std::stringstream ss;
                ss << "check_replay: sleep interrupted, "
                   << " dt=" << (t2-t1)/1000-snooze
                   <<" t1="<<t1<<", t2="<<t2<<", snooze="<<snooze;
                std::cerr << ss.str() << std::endl;
                zsys_info(ss.str().c_str());
                break;
            }

            zsys_debug("tick %d", countdown);
        }

        std::cerr << "check_replay exiting\n";
    }

    return 0;
}
