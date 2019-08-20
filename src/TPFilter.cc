#include "ptmp/api.h"
#include "ptmp/factory.h"
#include "ptmp/actors.h"
#include "ptmp/filter.h"

#include "json.hpp"

using json = nlohmann::json;

// The actor function
void ptmp::actor::filter(zsock_t* pipe, void* vargs)
{
    auto config = json::parse((const char*) vargs);
    std::string name = config["engine"];
    ptmp::internals::set_thread_name(name);

    ptmp::filter::engine_t* engine =
        ptmp::factory::make<ptmp::filter::engine_t>(name, config.dump());

    if (!engine) {
        zsys_error("No such filter engine: \"%s\"", name.c_str());
        throw std::runtime_error("filter: failed to locate engine");
        return;
    }
    
    zsock_t* isock = ptmp::internals::endpoint(config["input"].dump());
    zsock_t* osock = ptmp::internals::endpoint(config["output"].dump());
    if (!isock or !osock) {
        zsys_error("filter requires socket configuration");
        return;
    }
    
    zpoller_t* pipe_poller = zpoller_new(pipe, isock, NULL);

    zsock_signal(pipe, 0);      // signal ready


    while (!zsys_interrupted) {

        void* which = zpoller_wait(pipe_poller, -1);
        if (!which) {
            zsys_info("filter interrupted");
            break;
        }
        if (which == pipe) {
            zsys_info("filter got quit");
            break;
        }

        zmsg_t* msg = zmsg_recv(isock);
        if (!msg) {
            zsys_info("filter interrupted");
            zmsg_destroy(&msg);
            break;
        }

        ptmp::data::TPSet tps;
        ptmp::internals::recv(&msg, tps); // throws
        int64_t latency = zclock_usecs() - tps.created();

        std::vector<ptmp::data::TPSet> output_tpsets;
        (*engine)(tps, output_tpsets);
        if (output_tpsets.empty()) {
            continue;
        }
        if (osock) {            // allow null for debugging
            for (const auto& otpset : output_tpsets) {
                ptmp::internals::send(osock, otpset); // fixme: can throw
            }
        }
        else {
            zsys_debug("filter got %ld TPSets", output_tpsets.size());
        }
            
    }

    delete engine; engine=nullptr;

    zpoller_destroy(&pipe_poller);
    zsock_destroy(&isock);
    zsock_destroy(&osock);
} 

ptmp::TPFilter::TPFilter(const std::string& config)
    : m_actor(zactor_new(ptmp::actor::filter, (void*)config.c_str()))

{
}

ptmp::TPFilter::~TPFilter()
{
    zsock_signal(zactor_sock(m_actor), 0); // signal quit
    zclock_sleep(1000);
    zactor_destroy(&m_actor);
}
PTMP_AGENT(ptmp::TPFilter, filter)

