#ifndef PTMP_FACTORY
#define PTMP_FACTORY

#include "ptmp/api.h"
#include "ptmp/upif.h"

namespace ptmp {

    class AgentFactory {
        upif::cache m_cache;
    public:
        AgentFactory();
        ~AgentFactory();
        ptmp::TPAgent* make(const std::string& alias, const std::string& config);

        upif::cache& plugin_cache() { return m_cache; }
        
    };

    AgentFactory& agent_factory();

}

#define PTMP_AGENT(TYPE,ALIAS) \
extern "C" { void* ptmp_make_##ALIAS##_agent(const char* config) { return new TYPE(config); } }


#endif 
