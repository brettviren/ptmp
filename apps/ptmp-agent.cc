// generic ptmp application to manage actors

#include "upif.h"

#include "CLI11.hpp"
#include "json.hpp"

#include <czmq.h>

#include <fstream>
#include <string>
#include <vector>

using json = nlohmann::json;

int main(int argc, char* argv[])
{
    // Define command line interface
    CLI::App app{"PTMP agent manages PTMP actors"};

    std::string filename="";
    app.add_option("-c,--config", filename, "A configuration file");

    std::vector<std::string> plugins;
    app.add_option("-p,--plugin", plugins, "User plugins");

    int timeout=-1;             // -1=wait forever
    app.add_option("--timeout", timeout, "Timeout in msec to wait for any readout");
    // fixme: maybe add generic options to inject parameter from CLI
    // to actors outside of config file mechanism.

    CLI11_PARSE(app, argc, argv);


    // Slurp configuration file
    json jcfg;
    if (filename.empty()) {
        std::cerr << "warning: no configuration file, try --help?\n";
    }
    else {
        std::ifstream fp(filename);
        fp >> jcfg;
    }

    // Load ptmp and any user plugins 
    plugins.push_back("ptmp");
    if (jcfg["plugins"].is_array()) {
        std::vector<std::string> extra_plugins = jcfg["plugins"];
        plugins.insert(plugins.end(), extra_plugins.begin(), extra_plugins.end());
    }
    for (auto plugin : plugins) {
        upif::add(plugin);
    }
    

    if (jcfg["actors"].empty()) {
        std::cerr << "No actors configured.  Bye.\n";
        return 0;
    }

    // Instantiate actors as bare functions.  This is low level in
    // that it's up to the various actor function implementors to
    // figure out how to create actor sockets and coordinate endpoint
    // configuration.  The next level to be taken from dexnet is
    // abstracting the concept of ported graph nodes.  If/when that's
    // done, bare functions may coexist with ported graph nodes - in
    // fact, the ported graph nodes will be constructed and executed
    // from a bare actor function.

    typedef void (*zactor_fnp)(zsock_t* pipe, void* args);
    std::vector<zactor_t*> actors;
    std::vector<std::string> bucket;
    for (auto jactor: jcfg["actors"]) {
        std::string funcname = jactor["function"];
        zactor_fnp actorfuncptr;
        bool ok = upif::symbol(funcname, actorfuncptr);
        if (!ok) {
            std::cerr << "No such actor function: \"" << funcname << "\".  Load a plugin?\n";
            return -1;
        }
        const std::string cfg = jactor.dump();
        bucket.push_back(cfg);
        auto a = zactor_new(*actorfuncptr, (void*)bucket.back().c_str());
        if (!a) {
            std::cerr << "Failed to make actor for \"" << funcname << "\"\n";
            return -1;
        }
        zsys_info("start actor: %s", bucket.back().c_str());
        zsys_info("%p", a);
        actors.push_back(a);
    }


    // Send any initial notification messages to actors
    // ...


    // set up poller to read actor pipe messages
    zpoller_t* poller = zpoller_new(NULL);
    for (auto actor : actors) {
        int rc = zpoller_add(poller, zactor_sock(actor));
        if (rc) {
            std::cerr << "Failed to poll on actor\n";
            return -1;
        }
        std::cerr << "added actor\n";
    }

    // main loop
    while (true) {
        void* which = zpoller_wait(poller, timeout);
        if (!which) {
            if (zpoller_terminated(poller)) {
                zsys_error("terminated");
                return -1;
            }
            if (zpoller_expired(poller)) {
                zsys_info("no data for %d msec", timeout);
                continue;
            }
            zsys_error("unknown error");
            return -1;
        }
        zsys_info("ptmp got message from %p", which);
        zmsg_t* msg = zmsg_recv((zsock_t*)which);
        zmsg_destroy(&msg);
        zsys_info("continuing");

        // Big Fat FIXME: it's right here that any inter-actor
        // protocol would be enacted.  There are two major categories
        // of what this might provide:

        // 1) proxy to Zyre actor

        // 2) proxy to config/control/monitor

        // This latter may include creating new and terminating
        // existing actors.  In which case the constructional code
        // above this loop should be refactored so that the
        // configuration provided at constructor time is provided via
        // messaging.

        // It may also include dynamic reconfiguration of existing
        // actors which requires being able to address actors and that
        // ties into the info provided to Zyre.

        // An actor may decide to exit and we should get notice of
        // that here.  User hits Ctrl-C and we should forward that to
        // the actors.
    }

    // shutdown 
    zpoller_destroy(&poller);
    for (auto actor : actors) {
        zactor_destroy(&actor);
    }
    actors.clear();

    return 0;
}
