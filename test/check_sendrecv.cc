/*

This test sets up a system of three threads, two "children" that talk
to each other and who both talk to "main".

 - sender :: actor with a TPSender and notifies pipe of each call
 - recver :: actor with a TPReceiver and notifies pipe of each call
 - main :: spawn actors and service their pipes

The protocol is:

- main->children ["start"]
- main->children ["stop"]
- child->main [my ID, my number of messages]
- sender->recver [a message]

Because children send to main on blocking pipes (PAIRs) and
particularly when children themselves are configured with blocking
sockets, it is possible to make this protocol deadlock, especially at
shutdown.  Eg, recver stops, sender fills buffers, main exits.  

If coded incorrectly, a deadlock may or may not occurr depending on
the details of thread scheduling.  For some reason, Travis-CI hosts
are better at tickling deadlocks than are my development hosts.  A
good way to tickled a deadlock even on a development host is to run a
bunch of simultaneous instances of this program.  Eg, something like:

  for n in {01..10} ; do 
    (./build/test/check_sendrecv 100000 bind pipe inproc://sendrecv>log.$n 2>&1 &)
  done

Then, check if any processes hangs and/or log files are different
sizes.  If needed, don't forget to clean up:

  ps -ef|grep check_sendrecv
  killall -9 check_sendrecv

Avoiding deadlock is mostly a matter of recver continuing to drain
after getting "stop" (only needed if sender->recver is a blocking
socket pattern) and for "main" to continue to drain both children
after sending them "stop".

 */

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
    zsys_info("sender starting");
    json jcfg = json::parse((const char*)(args));
    json jsock;
    jsock["socket"] = jcfg["sockets"][0];
    ptmp::TPSender send(jsock.dump());
    ptmp::data::TPSet tps;
    ptmp::testing::init(tps);
    
    zsock_signal(pipe, 0);      // ready
    zsys_info("send ready");

    zpoller_t* poller = zpoller_new(pipe, NULL);
    int count = 0;
    while (true) {
	//zsys_info("0: %d, check pipe", count);
        void* which = zpoller_wait(poller, 0);
        if (which) {
            zsys_info("send got stop at %d", count);
            break;
        }
        ptmp::testing::set_count_clock(tps);

        // depending on socket pattern, this may block if HWM
        send(tps);

        // This will block on HWM
        zsock_send(pipe, "44", 0, count);

        //zsys_info("0: %d", count);
        ++count;
        //usleep(1);
    }

    zsys_info("send exiting");
}

void recver(zsock_t* pipe, void* args)
{
    zsys_info("recv starting");
    json jcfg = json::parse((const char*)(args));
    json jsock;
    jsock["socket"] = jcfg["sockets"][0];
    ptmp::TPReceiver recv(jsock.dump());
    ptmp::data::TPSet tps;
    ptmp::testing::init(tps);
    
    zsock_signal(pipe, 0);      // ready
    zsys_info("recv ready");

    zpoller_t* poller = zpoller_new(pipe, NULL);
    int count=0;
    int got_count = 0;
    const int timeout = 1000;
    while (true) {
	//zsys_info("1: %d, check pipe", count);
        void* which = zpoller_wait(poller, 0);
        if (which) {
            zsys_info("recv got stop at %d, last got: %d", count, got_count);
            break;
        }
        try {
            bool ok = recv(tps, timeout);
            if (!ok) {
                zsys_info("recv timeout at %d count, last got %d, timeout is %d",
                          count, got_count, timeout);
                break;
            }
        }
        catch (const std::runtime_error& err) {
            zsys_info("recv error at %d count, last got %d: %s",
                      count, got_count, err.what());
            break;
        }
        got_count = tps.count();

        // This will block on HWM
        zsock_send(pipe, "44", 1, got_count);

        //zsys_info("1: %d %d", got_count, count);
        ++count;
    }

    bool ok = true;
    int late = 0;
    while (ok) {
        ok = recv(tps, timeout);
        ++late;
    }

    zsys_info("recv exiting after %d late messages", late);
}


int main(int argc, char* argv[])
{
    zsys_init();

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
    json jsock_send, jsock_recv;
    if (at == "pair" or at == "pipe") {
        jsock_send["type"] = "PAIR";
        jsock_recv["type"] = "PAIR";
    }
    else if (at == "pubsub") {
        jsock_send["type"] = "PUB";
        jsock_recv["type"] = "SUB";
    }
    else if (at == "pushpull") {
        jsock_send["type"] = "PUSH";
        jsock_recv["type"] = "PULL";
    }
    else {
        cerr << "Unknown attchement type: " << at << endl;
        return 1;
    }
    if (bc == "bind") {
        jsock_send["bind"][0] = address;
        jsock_recv["connect"][0] = address;
    }
    else {
        jsock_send["connect"][0] = address;
        jsock_recv["bind"][0] = address;
    }
    jsend["sockets"][0] = jsock_send;
    jrecv["sockets"][0] = jsock_recv;

    const std::string sendstr = jsend.dump();
    const std::string recvstr = jrecv.dump();
    zactor_t *sactor=NULL, *ractor=NULL;

    
    if (bc == "bind") {		// sender binds
	sactor = zactor_new(sender, (void*)sendstr.c_str());
	ractor = zactor_new(recver, (void*)recvstr.c_str());
    }
    else {			// recv binds
	ractor = zactor_new(recver, (void*)recvstr.c_str());
	sactor = zactor_new(sender, (void*)sendstr.c_str());
    }

    zpoller_t* poller = zpoller_new(zactor_sock(sactor),
                                    zactor_sock(ractor), NULL);
    auto tbeg = ptmp::data::now();
    int sn=0, num=0;
    const int timeout = 1000;
    while (true) {
        void* which = zpoller_wait(poller, timeout);
        if (!which) {
	    zsys_info("main loop break at loop %d with timeout %d ms", num, timeout);
            break;
        }
        zsock_recv((zsock_t*)which, "44", &sn, &num);

        if (num == count) {     // reach end
            zsys_info("main loop limit from %d %d", sn, num);
            zsys_info("main loop telling sender to stop");
            zstr_send(zactor_sock(sactor), "stop");
            zsys_info("main loop telling recver to stop");
            zstr_send(zactor_sock(ractor), "stop");
            zsys_info("main loop break from normal operation");
            break;
        }            
    }

    // now enter shutdown phase where we drain any notices from children.
    while (true) {
        void* which = zpoller_wait(poller, timeout);
        if (!which) {
	    zsys_info("main loop break at loop %d with timeout %d ms", num, timeout);
            break;
        }
        zsock_recv((zsock_t*)which, "44", &sn, &num);
    }

    zpoller_remove(poller, zactor_sock(sactor));
    zpoller_remove(poller, zactor_sock(ractor));
    zpoller_destroy(&poller);


    auto tend = ptmp::data::now();
    const double dt = (tend-tbeg)*1e-6;
    const double khz = 0.001*num/dt;
    zsys_info("main %d in %.3f s or %.1f kHz", num, dt, khz);

    zactor_destroy(&ractor);
    zactor_destroy(&sactor);

    return 0;
}
// Local Variables:
// mode: c++
// c-basic-offset: 4
// End:
