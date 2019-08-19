#ifndef PRIVATE_PTMP_SOCKETSTATS
#define PRIVATE_PTMP_SOCKETSTATS
#include "ptmp/data.h"

#include "json.hpp"

namespace ptmp {
    namespace noexport {

        // generic fodder for metrics based on TPSet in -> TPSet out
        struct time_stats_t {
            // time stored in microseconds
            int64_t t0{0}, tf{0}, dt{0}, lat{0}, lat2{0};

            // derived

            // 1.0/(time span of stats) in Hz
            double hz{0.0};
            double lat_mean{0.0}, lat_rms{0.0};

            void update(int64_t now, int64_t then);
            void finalize(double n);
                
            nlohmann::json jsonify();
        };

        // stats collected at a socket (in or out)
        struct socket_stats_t {

            uint64_t ntps{0}, ntpsets{0};
            time_stats_t real, data;

            void update(ptmp::data::TPSet& tpset, int tickperus = 50);
            void finalize();

            nlohmann::json jsonify();
        };
        
    }
}

#endif 
