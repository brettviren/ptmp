#include "ptmp/api.h"
#include "ptmp/internals.h"

#include <cstdio>

#include <iostream>

#include "json.hpp"

using json = nlohmann::json;

void tpcat(zsock_t* pipe, void* vargs)
{
    zsys_init();

    auto config = json::parse((const char*) vargs);

    // std::cerr << config.dump(4) << std::endl;

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
            zsys_info("isock: %s", cfg.c_str());
        }
    }
    zsock_t* osock = NULL;
    if (!config["output"].is_null()) {
        std::string cfg = config["output"].dump();
        osock = ptmp::internals::endpoint(cfg);
        if (isock) {
            zsys_info("isock: %s", cfg.c_str());
        }
    }
    FILE* ifp = NULL;
    if (!config["ifile"].is_null()) {
        std::string fname = config["ifile"];
        ifp = fopen(fname.c_str(), "r");
        if (ifp) {
            zsys_info("ifile: %s", fname.c_str());
        }
    }
    FILE* ofp = NULL;
    if (!config["ofile"].is_null()) {
        std::string fname = config["ofile"];
        ofp = fopen(fname.c_str(), "w");
        if (ofp) {
            zsys_info("ofile: %s", fname.c_str());
        }
    }
    zsock_signal(pipe, 0); // signal ready    


    if (! (isock or ifp)) {
        zsys_error("TPCat: no input, why bother?");
        return;
    }

    int count = 0;
    while (!zsys_interrupted) {
        if (number > 0 and count >= number) {
            break;
        }
        ++count;

        zmsg_t* msg=NULL;

        if (ifp) {
            msg = ptmp::internals::read(ifp);
            if (!msg) {
                zsys_info("end of file after %d", count);
                break;
            }
        }
        if (isock) {
            msg = zmsg_recv(isock);
            if (!msg) {
                zsys_warning("interupted stream after %d", count);
                break;
            }
        }
        
        //zsys_debug("%d", count);

        if (delayus > 0) {
            ptmp::internals::microsleep(delayus);
        }

        if (ofp) {
            ptmp::internals::write(ofp, msg);
        }
        if (osock) {
            int rc = zmsg_send(&msg, osock);
            if (rc != 0) {
                zsys_warning("send failed after %d", count);
                break;
            }
        }

        if (msg) {
            zmsg_destroy(&msg);
        }

    }

    if (isock) zsock_destroy(&isock);
    if (osock) zsock_destroy(&osock);
    if (ifp) fclose(ifp);
    if (ofp) fclose(ofp);

}
ptmp::TPCat::TPCat(const std::string& config)
    : m_actor(zactor_new(tpcat, (void*)config.c_str()))
{
}

ptmp::TPCat::~TPCat()
{
    zsock_signal(zactor_sock(m_actor), 0); // signal quit
    zactor_destroy(&m_actor);
}
