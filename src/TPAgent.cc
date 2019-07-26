#include "ptmp/factory.h"

#include "json.hpp"
#include <cstdlib>

using json = nlohmann::json;

ptmp::TPAgent::~TPAgent()
{
}

ptmp::AgentFactory::AgentFactory()
    : m_cache()
{
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
        //zsys_debug("adding plugin: \"%s\"", pi.c_str());
        m_cache.add(pi);
        beg = end + delim.length();
        end = pis.find(delim, beg);
    }
}

ptmp::AgentFactory::~AgentFactory()
{
}

typedef void* (*agent_maker)(const char*);

ptmp::TPAgent* ptmp::AgentFactory::make(const std::string& alias, const std::string& config)
{
    // match what PTMP_AGENT() macro builds
    std::string fname = "ptmp_make_" + alias + "_agent";
    auto pi = m_cache.find(fname);
    if (!pi) {
        zsys_error("failed to find symbol: \"%s\"", fname.c_str());
        return nullptr;
    }

    agent_maker am;
    bool ok = pi->symbol(fname.c_str(), am);
    if (!ok) { return nullptr; }
    return (ptmp::TPAgent*)((*am)(config.c_str()));
}

ptmp::AgentFactory& ptmp::agent_factory()
{
    static AgentFactory af;
    return af;
}




/// this is used for testing in test/test_upif
extern "C" {
    int funcs_test(int x);
}
int funcs_test(int x)
{
    return x;
}
