// a variant on TPMonitor where we use zproxy instead of DIY

#include "ptmp/api.h"
#include "ptmp/data.h"
#include "ptmp/internals.h"

#include <cstdio>

#include "json.hpp"
using json = nlohmann::json;

static
std::string get_zmq_addrs(json jsock)
{
    std::string ret = "";
    std::string comma="";
    const std::map<std::string, std::string> bcmap{{"bind","@"}, {"connect",">"}};

    for (const auto& bcit : bcmap) {
        const std::string bc = bcit.first;
        if (jsock[bc].is_null()) {
            continue;
        }
        const std::string prefix = bcit.second;
        for (const auto& jaddr : jsock[bc]) {
            ret += comma + prefix + jaddr.get<std::string>();
            comma = ",";
        }
    }

    return ret;
}

static
int attach_end(zactor_t* proxy, std::string whichend, json jsock)
{
    std::string stype = jsock["type"];
    std::string addr = get_zmq_addrs(jsock);

    int rc = zstr_sendx(proxy, whichend.c_str(), stype.c_str(), addr.c_str(), NULL);
    if (rc != 0) { return -1; }
    rc = zsock_wait(proxy);
    if (rc < 0) { return -1; }

    zsys_debug("monitor: %s %s %s", whichend.c_str(), stype.c_str(), addr.c_str());
    return 0;
}


static
void tpmonitorcz(zsock_t* pipe, void* vargs)
{
    auto config = json::parse((const char*) vargs);

    std::string filename = config["filename"];

    struct tapinfo_t {
        zactor_t* proxy;
        zsock_t* tap;
        int id;
        int nrecv{0};
    };

    std::vector<zactor_t*> taps;

    // map tap socket pointer to info about the tap
    std::unordered_map<void*, tapinfo_t> tapinfos;
    zpoller_t* poller = zpoller_new(pipe, NULL);

    for (const auto& jtap : config["taps"]) {
        const int tapid = jtap["id"];
        char* addr = zsys_sprintf("inproc://capture%05d", tapid);

        tapinfo_t ti {zactor_new(zproxy, NULL), zsock_new_pull(addr), tapid};
        assert (ti.proxy and ti.tap);
        zsys_debug("monitor: CAPTURE PULL %s", addr);

        zpoller_add(poller, ti.tap);

        int rc = 0;
        rc = attach_end(ti.proxy, "FRONTEND", jtap["input"]["socket"]);
        assert (rc == 0);
        rc = attach_end(ti.proxy, "BACKEND", jtap["output"]["socket"]);
        assert (rc == 0);
        rc = zstr_sendx(ti.proxy, "CAPTURE", addr, NULL);
        assert(rc == 0);
        zsock_wait(ti.proxy);

        tapinfos[(void*)ti.tap] = ti;
        zsys_debug("monitor: made tap %d", tapid);
        freen (addr);
    }
    
    zsock_signal(pipe, 0); // signal ready    

    // open file...
    FILE* fp = fopen(filename.c_str(), "w");
    if (!fp) {
        zsys_error("monitor: failed to open file %s", filename.c_str());
        return;
    }

    int nrecv_tot=0;
    bool got_quit = false;
    while (!zsys_interrupted) {

        void* which = zpoller_wait(poller, -1);
        if (!which) {
            zsys_info("monitor: interrupted");
            break;
        }
        if (which == pipe) {
            zsys_info("monitor: got quit");
            got_quit = true;
            break;
        }

        auto tiit = tapinfos.find(which);
        if (tiit == tapinfos.end()) {
            zsys_error("monitor: unknown tap");
            continue;
        }
        auto& ti = tiit->second;

        ptmp::data::TPSet tpset;
        zmsg_t* msg = zmsg_recv(ti.tap);
        ptmp::internals::recv(msg, tpset);
        int64_t now = ptmp::data::now();
        ++nrecv_tot;
        ++ti.nrecv;

        fprintf(fp, "%d %ld %d %d %d %d %ld %ld %d %d\n",
                ti.id, now, ti.nrecv, nrecv_tot,
                tpset.count(), tpset.detid(), tpset.created(),
                tpset.tstart(), tpset.tspan(), tpset.tps_size());
    }


    // close file ...
    fclose(fp); fp=NULL;

    zsys_debug("monitor: signalling taps to shutdown");
    zpoller_destroy(&poller);
    for (auto tiit : tapinfos) {
        auto& ti = tiit.second;
        if (!ti.tap) {
            zsys_error("null tap");
            continue;
        }

        zsys_debug("monitor: tap ID %d", ti.id);
        zsock_destroy(&ti.tap);

        zsys_debug("monitor: proxy for tap ID %d", ti.id);
        zactor_destroy(&ti.proxy);
    }
    if (got_quit) {
        return;
    }
    zsys_debug("monitor: waiting for quit");
    zsock_wait(pipe);
}


ptmp::TPMonitorcz::TPMonitorcz(const std::string& config)
    : m_actor(zactor_new(tpmonitorcz, (void*)config.c_str()))
{
}

ptmp::TPMonitorcz::~TPMonitorcz()
{
    zsock_signal(zactor_sock(m_actor), 0); // signal quit
    zactor_destroy(&m_actor);
}
