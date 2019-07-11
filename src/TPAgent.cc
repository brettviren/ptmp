#include "ptmp/api.h"
#include "ptmp/upif.h"
#include "json.hpp"
#include <cstdlib>

using json = nlohmann::json;

struct AgentFactory {
    upif::cache cache;
    AgentFactory(): cache() {
        const std::string delim = ",";
        std::string pis = "ptmp" + delim;
        const char* cpis = std::getenv("PTMP_PLUGINS"); // comma separated, please
        if (cpis) {
            pis += cpis + delim;
        }

        auto beg=0U;
        auto end=pis.find(delim);
        while (end != std::string::npos) {
            std::string pi = pis.substr(beg, end-beg);
            zsys_debug("adding plugin: \"%s\"", pi.c_str());
            cache.add(pi);
            beg = end + delim.length();
            end = pis.find(delim, beg);
        }
    }

    typedef void* (*agent_maker)(const char*);

    ptmp::TPAgent* make(const std::string& alias, const std::string& config) {
        std::string fname = "ptmp_make_" + alias + "_agent";
        auto pi = cache.find(fname);
        if (!pi) {
            zsys_error("failed to find symbol: \"%s\"", fname.c_str());
            return nullptr;
        }

        agent_maker am;
        bool ok = pi->symbol(fname.c_str(), am);
        if (!ok) { return nullptr; }
        return (ptmp::TPAgent*)((*am)(config.c_str()));
    }
};



ptmp::TPAgent::~TPAgent() {}


ptmp::TPAgent* ptmp::agent_factory(const std::string& alias, const std::string& config)
{
    static AgentFactory* af = nullptr;
    if (!af) {
        af = new AgentFactory;
    }
    zsys_debug("got agent factory");

    ptmp::TPAgent* agent = af->make(alias, config);
    if (!agent) {
        zsys_error("failed to construct agent %s", alias.c_str());
        return nullptr;
    }
    zsys_debug("agent %s with %s", alias.c_str(), config.c_str());
    return agent;
}

#if 0

ptmp::TPAgent* ptmp::agent_factory(const std::string& type, const std::string& config)
{
    // A simple factory to return an instance based on type name.  A
    // type name matching the C++ class name returns an instance of
    // that class.  A lower case "common" type name returns the
    // prefered instance when there are multiple implementations of
    // the same concept.

    zsys_debug("agent %s with %s", type.c_str(), config.c_str());
    if (type == "TPSorted" or type == "sorted") {
        return new ptmp::TPSorted(config.c_str());
    }
    if (type == "TPZipper" or type == "zipper") {
        return new ptmp::TPZipper(config.c_str());
    }
    if (type == "TPReplay" or type == "replay") {
        return new ptmp::TPReplay(config.c_str());
    }
    if (type == "TPWindow" or type == "window") {
        return new ptmp::TPWindow(config.c_str());
    }
    if (type == "TPMonitorz" or type == "monitor") {
        return new ptmp::TPMonitorz(config.c_str());
    }
    if (type == "TPMonitor") {
        return new ptmp::TPMonitor(config.c_str());
    }
    if (type == "TPMonitorcz" or type == "monitorcz") {
        return new ptmp::TPMonitorcz(config.c_str());
    }
    if (type == "TPCat" or type == "cat" or type == "czmqat") {
        return new ptmp::TPCat(config.c_str());
    }
    return nullptr;
}
#endif


/// this is used for testing in test/test_upif
extern "C" {
    int funcs_test(int x);
}
int funcs_test(int x)
{
    return x;
}
