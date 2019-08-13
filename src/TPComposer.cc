// 

#include "ptmp/api.h"
#include "ptmp/data.h"
#include "ptmp/internals.h"
#include "ptmp/factory.h"
#include "ptmp/actors.h"

#include <cstdio>

#include "json.hpp"
using json = nlohmann::json;

PTMP_AGENT(ptmp::TPComposer, composer)

void ptmp::actor::composer(zsock_t* pipe, void* vargs)
{
    auto config = json::parse((const char*) vargs);

    std::string name = "composer";
    if (config["name"].is_string()) {
        name = config["name"];
    }
    ptmp::internals::set_thread_name(name);


    zpoller_t* poller = zpoller_new(pipe, NULL);

    zsock_signal(pipe, 0); // signal ready    

    std::vector<ptmp::TPAgent*> agents;
    std::unordered_map<ptmp::TPAgent*, std::string> agent_name;
    for (auto& jprox : config["proxies"]) {
        const std::string name = jprox["name"];
        const std::string type = jprox["type"];
        const std::string data = jprox["data"].dump();

        ptmp::TPAgent* agent = ptmp::factory::make<ptmp::TPAgent>(type, data);
        if (!agent) {
            zsys_error("composer: failed to create agent \"%s\" of type \"%s\"",
                       name.c_str(), type.c_str());
            throw std::runtime_error("composer failed to create agent");
        }
        agents.push_back(agent);
        agent_name[agent] = name;
    }
    

    // wait forever or until we get interupt or quit
    void* which = zpoller_wait(poller, -1);

    for (auto& agent : agents) {
        zsys_debug("composer: deleting %s", agent_name[agent].c_str());
        delete agent;
    }

    zpoller_destroy(&poller);
    
    if (which) {                // got quit
        return;
    }
    zsys_debug("composer: waiting for quit");
    zsock_wait(pipe);
}


ptmp::TPComposer::TPComposer(const std::string& config)
    : m_actor(zactor_new(ptmp::actor::composer, (void*)config.c_str()))
{
}

ptmp::TPComposer::~TPComposer()
{
    zsys_debug("composer: signalling shutdown");
    zsock_signal(zactor_sock(m_actor), 0); // signal quit
    zsys_debug("composer destroying actor");
    zactor_destroy(&m_actor);
}
