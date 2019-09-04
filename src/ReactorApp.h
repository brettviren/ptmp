// an internal base class to reduce PTMP components reactor app
// classes.  not intended for general application.

#ifndef PRIVATE_PTMP_REACTORAPP
#define PRIVATE_PTMP_REACTORAPP

#include "SocketStats.h"
#include "ptmp/metrics.h"
#include "json.hpp"
#include <czmq.h>
#include <string>

namespace ptmp {
    namespace noexport {
        class ReactorApp {
        public:

            ReactorApp(zsock_t* pipe, nlohmann::json& config, const std::string& name = "ptmp");
            virtual ~ReactorApp();

            // subclass must provide
            virtual int add(ptmp::data::TPSet& tpset) = 0;

            // subclass may provide in order to fill in jmet with
            // additional info.  The stats collected in the base are
            // finalized prior to call so subclass may utilize them.
            virtual void metrics(nlohmann::json& jmet) {}

            // start reactor
            void start();

        protected:

            // Subclass should call:
            void set_isock(zsock_t* sock);
            void set_osock(zsock_t* sock);

            // Subclass may call to send out a tpset
            int send(ptmp::data::TPSet& tpset);

            std::string name{""};
            int verbose{0};
            int detid{-1};
            ptmp::data::real_time_t start_time;
            uint64_t out_tpset_count{0};
            uint32_t tickperus{50};
            uint32_t last_in_count{0};

            ptmp::metrics::Metric* met{nullptr};

            zsock_t *pipe{nullptr}, *isock{nullptr}, *osock{nullptr};
            zloop_t *looper{nullptr};
            
            struct wait_stats_t {
                int count{0}, time_ms{0};
            };
            struct stats_t {
                socket_stats_t iss, oss;
                wait_stats_t waits;
                uint32_t n_in_lost{0};
            };

            stats_t stats;


        public:                 // so looper handlers can call
            int add_base(ptmp::data::TPSet& tpset);
            int metrics_base();

        };                      // ReactorApp
    }
}

#endif
