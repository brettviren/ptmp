#include "ptmp/api.h"
#include "ptmp/testing.h"

#include "json.hpp"

#include <czmq.h>
#include <iostream>

#include <unistd.h>

using namespace std;
using json = nlohmann::json;

void sender(zsock_t* pipe, void* args)
{
    std::string cfg = *(std::string*)(args);
    ptmp::TPSender send(cfg);
    ptmp::data::TPSet tps;
    ptmp::testing::init(tps);
    
    zsock_signal(pipe, 0);      // ready
    char* go = zstr_recv(pipe); // should actually check what msg is...

    int count = 0;
    while (true) {
        zmsg_t* ctrl = zmsg_recv_nowait(pipe);
        if (ctrl) {
            zsys_info("sender got stop at %d", count);
            break;
        }
        ptmp::testing::set_count_clock(tps);
        send(tps);
        zsock_send(pipe, "44", 0, count);
        //zsys_info("0: %d", count);
        ++count;
        //usleep(1);
    }
    zsys_info("sender exiting");
}
void recver(zsock_t* pipe, void* args)
{
    std::string cfg = *(std::string*)(args);
    ptmp::TPReceiver recv(cfg);
    ptmp::data::TPSet tps;
    ptmp::testing::init(tps);
    
    zsock_signal(pipe, 0);      // ready
    char* go = zstr_recv(pipe); // should actually check what msg is...

    int count=0;
    int got_count = 0;
    while (true) {
        zmsg_t* ctrl = zmsg_recv_nowait(pipe);
        if (ctrl) {
            zsys_info("recver got stop at %d, last got: %d", count, got_count);
            break;
        }
        try {
            bool ok = recv(tps, 1000);
            if (!ok) {
                zsys_info("recver timout at %d count, last got %d",
                          count, got_count);
                break;
            }
        }
        catch (const std::runtime_error& err) {
            zsys_info("recver error at %d count, last got %d: %s",
                      count, got_count, err.what());
            break;
        }
        got_count = tps.count();
        zsock_send(pipe, "44", 1, got_count);
        //zsys_info("1: %d %d", got_count(), count);
        ++count;
    }
    zsys_info("recver exiting");
}

int main(int argc, char* argv[])
{
    if (argc < 5) {
        cerr << "usage: test_sendrecv N <bind|connect> <attachtype> <address>"
             << endl;
        return 0;
    }
    const int count = atoi(argv[1]);
    const std::string bc = argv[2];
    const std::string at = argv[3];
    const std::string address = argv[4];
    json jsend, jrecv;
    if (at == "pair" or at == "pipe") {
        jsend["socket"]["type"] = "PAIR";
        jrecv["socket"]["type"] = "PAIR";
    }
    else if (at == "pubsub") {
        jsend["socket"]["type"] = "PUB";
        jrecv["socket"]["type"] = "SUB";
    }
    else if (at == "pushpull") {
        jsend["socket"]["type"] = "PUSH";
        jrecv["socket"]["type"] = "PULL";
    }
    else {
        cerr << "Unknown attchement type: " << at << endl;
        return 1;
    }
    if (bc == "bind") {
        jsend["socket"]["bind"][0] = address;
        jrecv["socket"]["connect"][0] = address;
    }
    else {
        jsend["socket"]["connect"][0] = address;
        jrecv["socket"]["bind"][0] = address;
    }

    const std::string sendstr = jsend.dump();
    const std::string recvstr = jrecv.dump();
    zactor_t* sactor = zactor_new(sender, (void*)&sendstr);
    zactor_t* ractor = zactor_new(recver, (void*)&recvstr);

    zstr_send(ractor, "start");
    zstr_send(sactor, "start");

    zpoller_t* poller = zpoller_new(zactor_sock(sactor),
                                    zactor_sock(ractor), NULL);
    auto tbeg = zclock_usecs();
    int sn=0, num=0;
    while (true) {
        void* which = zpoller_wait(poller, 1000);
        if (!which) {
            zsys_info("main loop break");
            break;
        }
        zsock_recv((zsock_t*)which, "44", &sn, &num);
        if (num >= count) {
            zsys_info("main loop limit from %d %d", sn, num);
            zstr_send(zactor_sock(sactor), "stop");
            zstr_send(zactor_sock(ractor), "stop");
            break;
        }            
    }
    zpoller_remove(poller, zactor_sock(sactor));
    zpoller_remove(poller, zactor_sock(ractor));
    zpoller_destroy(&poller);


    auto tend = zclock_usecs();
    const double dt = (tend-tbeg)*1e-6;
    const double khz = 0.001*num/dt;
    zsys_info("recv %d in %.3f s or %.1f kHz", num, dt, khz);

    zactor_destroy(&ractor);
    zactor_destroy(&sactor);

    return 0;
}
