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
    int hwm;
    std::string socktype;
    std::string attach;
    std::vector<std::string> endpoints;
};

void make_opts(CLI::App* app, sock_opt_t& opt)
{
    app->add_option("-m,--socket-hwm", opt.hwm,
                   "The ZeroMQ socket highwater mark");    
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
    jsock["hwm"] = opt.hwm;
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

    sock_opt_t iopt{1000,"SUB", "connect"}, oopt{1000,"PUB", "bind"};
    make_opts(isocks, iopt);
    make_opts(osocks, oopt);

    CLI11_PARSE(app, argc, argv);
    
    json jcfg;
    jcfg["tardy"] = tardy;
    jcfg["input"] = make_cfg(iopt);
    jcfg["output"] = make_cfg(oopt);
    
    //std::cerr << "Using config: " << jcfg << std::endl;

    std::string cfgstr = jcfg.dump();

    {
        ptmp::TPSorted proxy(cfgstr);

        int snooze = 1000;
        while (countdown != 0) {
            -- countdown;
            int64_t t1 = zclock_usecs ();
            zclock_sleep(snooze);
            int64_t t2 = zclock_usecs ();
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
