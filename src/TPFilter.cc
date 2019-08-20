#include "ReactorApp.h"
#include "ptmp/api.h"
#include "ptmp/factory.h"
#include "ptmp/actors.h"
#include "ptmp/filter.h"


using json = nlohmann::json;

class FilterApp : public ptmp::noexport::ReactorApp {
    ptmp::filter::engine_t* engine{0};
    int count_calls{0}, count_tps{0}, count_tpsets{0};
    uint64_t call_time{0};

public:
    FilterApp(zsock_t* pipe, json& config) 
        : ReactorApp(pipe, config, config["engine"]) {

        engine = ptmp::factory::make<ptmp::filter::engine_t>(name, config.dump());
        if (!engine) {
            zsys_error("No such filter engine: \"%s\"", name.c_str());
            throw std::runtime_error("filter: failed to locate engine");
            return;
        }

        set_isock(ptmp::internals::endpoint(config["input"].dump()));
        set_osock(ptmp::internals::endpoint(config["output"].dump()));
    }

    virtual int add(ptmp::data::TPSet& tpset) {

        std::vector<ptmp::data::TPSet> output_tpsets;
        auto then = ptmp::data::now();
        (*engine)(tpset, output_tpsets);
        call_time += ptmp::data::now() - then;
        ++count_calls;
        if (output_tpsets.empty()) {
            return 0;
        }
        count_tpsets += output_tpsets.size();
        for (const auto& ot : output_tpsets) {
            count_tps += ot.tps_size();
        }

        if (!osock) {            // allow null for debugging
            zsys_debug("filter got %ld TPSets", output_tpsets.size());
            return 0;
        }

        for (auto& otpset : output_tpsets) {
            int rc = this->send(otpset);
            if (rc != 0) {
                return rc;
            }
        }
        return 0;
    }

    virtual void metrics(json& jmet) {
        if (count_calls == 0) {
            return;
        }
        double n = count_calls;

        jmet["percall"]["tpsets"] = count_tpsets/n;
        jmet["percall"]["tps"] = count_tps/n;
        jmet["percall"]["runtime"] = 1e-6*call_time/n;

        count_tpsets = count_tps = count_calls = 0;
        call_time = 0;
    }

    virtual ~FilterApp() {
        delete engine; engine=nullptr;
    }
        

};


// The actor function (reactor pattern)
void ptmp::actor::filter(zsock_t* pipe, void* vargs)
{
    auto jcfg = json::parse((const char*) vargs);
    FilterApp app(pipe, jcfg);
    zsock_signal(pipe, 0);      // signal ready
    app.start();
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

