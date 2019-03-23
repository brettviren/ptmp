// run a TPSorted

#include "ptmp/api.h"

#include <vector>
#include <string>
#include <iostream>

#include "CLI11.hpp"
#include "json.hpp"

using namespace std;
using json = nlohmann::json;


struct sock_opt_t {
    std::string socktype;
    std::string attach;
    std::vector<std::string> endpoints;
};

void make_opts(CLI::App* app, sock_opt_t& opt)
{
    app->add_option("-p,--socket-pattern", opt.socktype,
                    "The ZeroMQ socket pattern for endpoint [PUB, PAIR, PUSH]");
    app->add_option("-a,--socket-attachment", opt.attach,
                    "The socket attachement method [bind|connect] for this endpoint");
    app->add_option("-e,--socket-endpoints", opt.endpoints,
                    "The socket endpoint addresses in Zero MQ format (tcp:// or ipc://)")->expected(-1);
}

json make_cfg(sock_opt_t& opt)
{
    json jsock = json::object();
    jsock["type"] = opt.socktype;
    json eps = json::array();
    for (auto ep : opt.endpoints) {
        eps.push_back(ep);
    }
    jsock[opt.attach] = eps;
    json jcfg;
    jcfg["socket"] = jsock;
    return jcfg;
}

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

    sock_opt_t iopt{"SUB", "connect"}, oopt{"PUB", "bind"};
    make_opts(isocks, iopt);
    make_opts(osocks, oopt);

    CLI11_PARSE(app, argc, argv);
    
    json jcfg;
    jcfg["tardy"] = tardy;
    jcfg["input"] = make_cfg(iopt);
    jcfg["output"] = make_cfg(oopt);
    
    std::cerr << "Using config: " << jcfg << std::endl;

    std::string cfgstr = jcfg.dump();

    {
        ptmp::TPSorted proxy(cfgstr);

        if (argc > 2) {
            countdown = atoi(argv[2]);
        }

        int snooze = 1000;
        while (countdown != 0) {
            -- countdown;
            int64_t t1 = zclock_time ();
            zclock_sleep(snooze);
            int64_t t2 = zclock_time ();
            if (std::abs(t2-t1-snooze) > 1) {
                std::cerr << "Sleep interrupted\n";
                break;
            }

            std::cerr << "Tick " << countdown << "\n";
        }
        std::cerr << "Exiting\n";
    }

    return 0;
}
