#include "internals.h"
#include "json.hpp"

#include <string> 
#include <iostream>             // debug
#include <algorithm>
#include <unordered_map>

using json = nlohmann::json;

using namespace ptmp::internals;

int ptmp::internals::socket_type(std::string name)
{
    std::transform(name.begin(), name.end(), name.begin(), ::tolower);
    std::unordered_map<std::string, int> zmqtypes {
        {"pair", 0},
        {"pub", 1},
        {"sub", 2},
        {"req", 3},
        {"rep", 4},
        {"dealer", 5},
        {"router", 6},
        {"pull", 7},
        {"push", 8},
        {"xpub", 9},
        {"xsub", 10},
        {"stream", 11}
    };
    return zmqtypes[name];
}


zsock_t* ptmp::internals::endpoint(const std::string& config)
{
    auto jcfg = json::parse(config);
    auto jsock = jcfg["socket"];
    std::string stype = jsock["type"];
    int socktype = socket_type(stype);
    zsock_t* sock = zsock_new(socktype);
    if (socktype == 2) {        // fixme: support topics?
        zsock_set_subscribe(sock, "");
    }
    // if (socktype == 1) {
    //     int old = zsock_sndhwm(sock);
    //     zsys_info("pub hwm was %d", old);
    //     zsock_set_sndhwm(sock, 10*old);
    // }
    for (auto jaddr : jsock["bind"]) {
        std::string addr = jaddr;
        zsock_bind(sock, addr.c_str(), NULL);
    }
    for (auto jaddr : jsock["connect"]) {
        std::string addr = jaddr;
        zsock_connect(sock, addr.c_str(), NULL);
    }
    return sock;
}

ptmp::internals::Socket::Socket(const std::string& config)
    : m_sock(endpoint(config))
{
}

ptmp::internals::Socket::~Socket()
{
    zsock_destroy(&m_sock);
}
