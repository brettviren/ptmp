#include "ptmp/api.h"
#include "ptmp/internals.h"

#include <cstdio>


#include "json.hpp"

using json = nlohmann::json;

void tpcat(zsock_t* pipe, void* vargs)
{
    auto config = json::parse((const char*) vargs);


    int number = -1;
    if (config["number"].is_number()) {
        number = config["number"];
    }
    int delayus = 0;
    if (config["delayus"].is_number()) {
        delayus = config["delayus"];
    }

    zsock_t* isock = NULL;
    if (!config["isock"].is_null()) {
        isock = ptmp::internals::endpoint(config["input"].dump());
    }
    zsock_t* osock = NULL;
    if (!config["osock"].is_null()) {
        osock = ptmp::internals::endpoint(config["output"].dump());
    }
    FILE* ifp = NULL;
    if (!config["ifile"].is_null()) {
        std::string fname = config["ifile"];
        ifp = fopen(fname.c_str(), "r");
    }
    FILE* ofp = NULL;
    if (!config["ofile"].is_null()) {
        std::string fname = config["ofile"];
        ofp = fopen(fname.c_str(), "w");
    }

    zsock_signal(pipe, 0); // signal ready    

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
                break;
            }
        }
        if (isock) {
            msg = zmsg_recv(isock);
            if (!msg) {
                zsys_warning("interupted");
                break;
            }
        }
        
        if (delayus > 0) {
            ptmp::internals::microsleep(delayus);
        }

        if (ofp) {
            ptmp::internals::write(ofp, msg);
        }
        if (osock) {
            int rc = zmsg_send(&msg, osock);
            if (rc != 0) {
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
