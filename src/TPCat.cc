#include "ptmp/api.h"
#include "ptmp/internals.h"
#include "ptmp/factory.h"
#include "ptmp/actors.h"

#include <cstdio>

#include "json.hpp"

PTMP_AGENT(ptmp::TPCat, czmqat)

using json = nlohmann::json;

void ptmp::actor::cat(zsock_t* pipe, void* vargs)
{
    auto config = json::parse((const char*) vargs);

    std::string name = "czmqat";
    if (config["name"].is_string()) {
        name = config["name"];
    }
    ptmp::internals::set_thread_name(name);

    int number = -1;
    if (config["number"].is_number()) {
        number = config["number"];
    }
    int delayus = 0;
    if (config["delayus"].is_number()) {
        delayus = config["delayus"];
    }

    zsock_t* isock = NULL;
    if (!config["input"].is_null()) {
        std::string cfg = config["input"].dump();
        isock = ptmp::internals::endpoint(cfg);
        if (isock) {
            zsys_info("czmqat: isock: %s", cfg.c_str());
        }
    }
    zsock_t* osock = NULL;
    if (!config["output"].is_null()) {
        std::string cfg = config["output"].dump();
        osock = ptmp::internals::endpoint(cfg);
        if (osock) {
            zsys_info("czmqat: osock: %s", cfg.c_str());
        }
    }
    FILE* ifp = NULL;
    std::string ifname="";
    if (!config["ifile"].is_null()) {
        ifname = config["ifile"];
        ifp = fopen(ifname.c_str(), "r");
        if (ifp) {
            zsys_info("czmqat: ifile: %s", ifname.c_str());
        }
    }
    FILE* ofp = NULL;
    std::string ofname="";
    if (!config["ofile"].is_null()) {
        ofname = config["ofile"];
        ofp = fopen(ofname.c_str(), "w");
        if (ofp) {
            zsys_info("czmqat: ofile: %s", ofname.c_str());
        }
    }
    zsock_signal(pipe, 0); // signal ready    

    if (! (isock or ifp)) {
        zsys_error("czmqat: no input, why bother?");
        return;
    }

    const int output_timeout = 100;
    zmq_pollitem_t pollitems[2];
    int npollitems = 1;
    pollitems[0].socket = zsock_resolve(pipe);
    pollitems[0].events = ZMQ_POLLIN;
    if (osock) {
        void* vsock = zsock_resolve(osock);
        assert(vsock);
        pollitems[1].socket = vsock;
        pollitems[1].events = ZMQ_POLLOUT;
        ++npollitems;
    }

    zpoller_t* poller = zpoller_new(pipe, NULL);
    int timeout = 0;
    if (isock) {
        timeout = -1;
        zpoller_add(poller, isock);
    }

    int count = 0;
    bool got_quit = false;
    while (!zsys_interrupted) {
        if (number > 0 and count >= number) {
            break;
        }
        ++count;

        void* which = zpoller_wait(poller, timeout);
        if (which == pipe) {
            zsys_info("czmqat: got quit");
            got_quit = true;
            goto cleanup;
        }

        zmsg_t* msg=NULL;

        if (isock and which == isock) {
            msg = zmsg_recv(isock);
            if (!msg) {
                zsys_warning("czmqat: interupted stream after %d", count);
                break;
            }
        }
        else if (ifp) {
            msg = ptmp::internals::read(ifp);
            if (!msg) {
                zsys_info("czmqat: end of file %s after %d", ifname.c_str(), count);
                break;
            }
        }
        else {
            zsys_warning("czmqat: got something weird");
        }
        
        // zsys_debug("czmqat: message %d", count);

        if (delayus > 0) {
            zsys_debug("czmqat: sleeping %d us", delayus);
            ptmp::internals::microsleep(delayus);
        }

        if (ofp) {
            ptmp::internals::write(ofp, msg);
        }
        if (osock) {
            // Note, this will block if using eg PUSH and then this
            // actor will hang even if there is a shutdown waiting.
            
            while (true) {
                int rc = zmq_poll(pollitems, 2, output_timeout);
                if (rc == 0) {
                    continue;   // keep waiting
                }
                if (pollitems[1].revents & ZMQ_POLLOUT) {
                    int rc = zmsg_send(&msg, osock);
                    if (rc != 0) {
                        zsys_warning("czmqat: send failed after %d", count);
                        break;
                    }
                    //zsys_debug("czmqat: send %d", count);
                    break;
                }
                if (pollitems[0].revents & ZMQ_POLLIN) {
                    zsys_info("czmqat: got quit");
                    got_quit = true;
                    goto cleanup;
                }
            }
        }

        if (msg) {
            zmsg_destroy(&msg);
        }

    }

  cleanup:
    
    zsys_debug("czmqat: finished after %d", count);

    zpoller_destroy(&poller);
    if (isock) zsock_destroy(&isock);
    if (osock) zsock_destroy(&osock);
    if (ifp) fclose(ifp);
    if (ofp) fclose(ofp);
    if (got_quit) {
        return;
    }
    zsys_debug("czmqat: waiting for quit");
    zsock_wait(pipe);
}
ptmp::TPCat::TPCat(const std::string& config)
    : m_actor(zactor_new(ptmp::actor::cat, (void*)config.c_str()))
{
}

ptmp::TPCat::~TPCat()
{
    // actor may already have exited so sending this signal can hang.
    zsys_debug("czmqat: sending actor quit message");
    zsock_signal(zactor_sock(m_actor), 0); // signal quit
    zactor_destroy(&m_actor);
}
