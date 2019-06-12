// run a TPSorted

#include "ptmp/api.h"
#include "ptmp/cmdline.h"

#include <vector>
#include <string>
#include <iostream>

using namespace std;

using namespace ptmp::cmdline;

int main(int argc, char* argv[])
{
    CLI::App app{"Tap into a TPSet message stream"};

    int countdown = -1;         // forever
    app.add_option("-c,--count", countdown,
                   "Number of seconds to count down before exiting, simulating real app work");
    int verbose = 0;         
    app.add_option("-v,--verbosity", verbose, "Verbose level");
    int number = 0;         
    app.add_option("-n,--number", number, "Number of captures");
    
    CLI::App* isocks = app.add_subcommand("input", "Input socket specification");
    CLI::App* osocks = app.add_subcommand("output", "Output socket specification");
    app.require_subcommand(2);

    sock_options_t iopt{1000,"SUB", "connect"}, oopt{1000,"PUB", "bind"};
    add_socket_options(isocks, iopt);
    add_socket_options(osocks, oopt);

    CLI11_PARSE(app, argc, argv);
    
    zsys_init();

    zactor_t* tap = zactor_new(zproxy, NULL);
    if (verbose) {
        zstr_send (tap, "VERBOSE");
        zsock_wait (tap);
    }

    std::string eps = "";
    std::string comma = "";
    for (const auto ep : iopt.endpoints) {
        eps += comma + ep;
        comma = ",";
    }
    zstr_sendx (tap, "FRONTEND", iopt.socktype.c_str(), eps.c_str(), NULL);
    zsock_wait (tap);

    eps = comma = "";
    for (const auto ep : oopt.endpoints) {
        eps += comma + ep;
        comma = ",";
    }
    zstr_sendx (tap, "BACKEND", oopt.socktype.c_str(), eps.c_str(), NULL);
    zsock_wait (tap);

    zsock_t* capture = zsock_new_pull("inproc://capture");
    assert(capture);
    zstr_sendx (tap, "CAPTURE", "inproc://capture", NULL);
    zsock_wait (tap);

    int count = 0;
    zpoller_t* poller = zpoller_new(capture, NULL);
    while (!zsys_interrupted) {

        void* which = zpoller_wait(poller, -1);
        if (!which) {
            zsys_info("tpset-tap: interrupted");
            break;
        }

        zmsg_t* msg = zmsg_recv(capture);
        if (!msg) {
            zsys_info("tpset-tap: interrupted");
            zmsg_destroy(&msg);
            break;
        }

        ptmp::data::TPSet tps;
        ptmp::internals::recv(msg, tps); // throws
        int64_t latency = zclock_usecs() - tps.created();

        ptmp::data::dump(tps,"");
        ++count;
        if (number and number == count) {
            if (verbose) {
                zsys_debug("ending after %d messages", count);
            }
            break;
        }
    }

    zsys_debug("destroy tap");
    zactor_destroy(&tap);
    zsys_debug("destroy capture");
    zsock_destroy(&capture);
    zsys_debug("destroy poller");
    zpoller_destroy(&poller);
    zsys_debug("done....");
    return 0;
}
