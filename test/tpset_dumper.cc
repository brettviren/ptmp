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

    std::string filename = "/dev/stdout";
    app.add_option("-o,--output", filename,
                   "Output filename (default is stdout)");
    
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

    std::fstream out(filename);
    if (!out) {
        std::cerr << "Failed to open file for writing: "
                  << filename << "\n";
        return -1;
    }

    ptmp::TPReceiver recv(jcfg.dump());
    ptmp::data::TPSet tps;

    while (!zsys_interrupted) {
        bool ok = recv(tps, timeout);
        if (!ok) {
            break;
        }


    }
    return 0;
}
