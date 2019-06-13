// run a TPSorted

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
    CLI::App app{"A proxy accept async TPSet streams and emit a single, time-sorted stream"};

    int tardy = 1000;
    app.add_option("-t,--tardy", tardy,
                   "Time in msec before any given stream is considered tardy");
    int countdown = -1;         // forever
    app.add_option("-c,--count", countdown,
                   "Number of seconds to count down before exiting, simulating real app work");
    
    CLI::App* isocks = app.add_subcommand("input", "Input socket specification");
    CLI::App* osocks = app.add_subcommand("output", "Output socket specification");
    app.require_subcommand(2);

    sock_options_t iopt{1000,"SUB", "connect"}, oopt{1000,"PUB", "bind"};
    add_socket_options(isocks, iopt);
    add_socket_options(osocks, oopt);

    CLI11_PARSE(app, argc, argv);
    
    json jcfg;
    jcfg["tardy"] = tardy;
    jcfg["input"] = to_json(iopt);
    jcfg["output"] = to_json(oopt);
    
    std::cerr << "Using config: " << jcfg.dump(4) << std::endl;

    std::string cfgstr = jcfg.dump();

    {
        ptmp::TPSorted proxy(cfgstr);

        int snooze = 1000;
        while (countdown != 0) {
            -- countdown;
            auto t1 = ptmp::data::now();
            zclock_sleep(snooze);
            auto t2 = ptmp::data::now();
            if (std::abs((t2-t1)/1000-snooze) > 10) {
                std::stringstream ss;
                ss << "check_sorted: sleep interrupted, "
                   << " dt=" << (t2-t1)/1000-snooze
                   <<" t1="<<t1<<", t2="<<t2<<", snooze="<<snooze;
                std::cerr << ss.str() << std::endl;
                zsys_info(ss.str().c_str());
                break;
            }

            std::cerr << "check_sorted: tick " << countdown << "\n";
            zsys_debug("tick %d", countdown);
        }
        std::cerr << "check_sorted exiting\n";
    }

    return 0;
}
