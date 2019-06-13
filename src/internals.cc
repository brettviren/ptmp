#include "ptmp/data.h"
#include "ptmp/internals.h"
#include "json.hpp"

#include <string> 
#include <iostream>             // debug
#include <algorithm>
#include <unordered_map>
#include <unistd.h>

using json = nlohmann::json;

using namespace ptmp::internals;

void ptmp::internals::microsleep(ptmp::data::real_time_t microseconds)
{
    usleep(microseconds);
}

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
    int n = zmqtypes[name];
    zsys_info("socket type \"%s\" %d", name.c_str(), n);
    return n;
}


zsock_t* ptmp::internals::endpoint(const std::string& config)
{
    auto jcfg = json::parse(config);
    auto jsock = jcfg["socket"];
    std::string stype = jsock["type"];
    int hwm = 1000;
    if (jsock["hwm"].is_number()) {
        hwm = jsock["hwm"];
    }
    int socktype = socket_type(stype);
    zsock_t* sock = zsock_new(socktype);
    if (!sock) {
        zsys_error("failed to make socket of type %s #%d", stype.c_str(), socktype);
        return sock;
    }
    zsys_info("endpoint of type %s #%d, hwm:%d",
              stype.c_str(), socktype, hwm);

    zsock_set_rcvhwm(sock, hwm);
    if (socktype == 2) {        // fixme: support topics?
        zsock_set_subscribe(sock, "");
    }

    for (auto jaddr : jsock["bind"]) {
        std::string addr = jaddr;
        zsys_info("binding \"%s\"", addr.c_str());
        int port = zsock_bind(sock, addr.c_str(), NULL);
        if (port<0) {
            zsys_error(zmq_strerror (errno));
            throw std::runtime_error("failed to bind");
        }
    }
    for (auto jaddr : jsock["connect"]) {
        std::string addr = jaddr;
        zsys_info("connecting \"%s\"", addr.c_str());
        int rc = zsock_connect(sock, addr.c_str(), NULL);
        if (rc<0) {
            zsys_error(zmq_strerror (errno));
            throw std::runtime_error("failed to connect");
        }
    }
    return sock;
}

ptmp::internals::Socket::Socket(const std::string& config)
    : m_sock(endpoint(config))
    , m_poller(zpoller_new(m_sock, NULL))
{
}

ptmp::internals::Socket::~Socket()
{
    std::cerr << "Socket destroy " << zsock_endpoint(m_sock) << std::endl;
    zpoller_destroy(&m_poller);
    zsock_destroy(&m_sock);

}
zmsg_t* ptmp::internals::Socket::msg(int timeout_msec)
{
    void* which = zpoller_wait(m_poller, timeout_msec);
    if (!which) return NULL;

    return zmsg_recv((zsock_t*)which);
}


// frames:
// [1] a message type ID (only ID 0 currently supported).
// [2] the payload as serialized TPSet

void ptmp::internals::recv(zmsg_t* &msg, ptmp::data::TPSet& tps)
{
    zframe_t* fid = zmsg_first(msg);
    if (!fid) {
        zsys_error(zmq_strerror (errno));
        throw std::runtime_error("null id frame");
    }
    int topic = *(int*)zframe_data(fid);

    zframe_t* pay = zmsg_next(msg);
    if (!pay) {
        zsys_error(zmq_strerror (errno));
        throw std::runtime_error("null payload frame");
    }
    bool ok = tps.ParseFromArray(zframe_data(pay), zframe_size(pay));
    if (!ok) {
        zsys_error("failed to parse TPSet");
        throw std::runtime_error("failed to parse TPSet");
    }

    zmsg_destroy(&msg);
}

void ptmp::internals::send(zsock_t* sock, const ptmp::data::TPSet& tps)
{
    const int topic = 0;

    // the message ID:
    zmsg_t* msg = zmsg_new();
    if (!msg) {
        zsys_error(zmq_strerror (errno));
        throw std::runtime_error("new msg failed");
    }
    zframe_t* fid = zframe_new(&topic, sizeof(int));
    if (!fid) {
        zsys_error(zmq_strerror (errno));
        throw std::runtime_error("new frame failed");
    }
    int rc = zmsg_append(msg, &fid);
    if (rc) {
        zsys_error(zmq_strerror (errno));
        throw std::runtime_error("msg append failed");
    }

    size_t siz = tps.ByteSize();
    zframe_t* pay = zframe_new(NULL, siz);
    tps.SerializeToArray(zframe_data(pay), zframe_size(pay));
    rc = zmsg_append(msg, &pay);
    if (rc) {
        zsys_error(zmq_strerror (errno));
        throw std::runtime_error("msg append failed");
    }
        
    rc = zmsg_send(&msg, sock);
    if (rc) {
        zsys_error(zmq_strerror (errno));
        throw std::runtime_error("msg send failed");
    }
    
}

