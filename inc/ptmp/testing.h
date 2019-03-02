#ifndef PTMP_TESTING_H
#define PTMP_TESTING_H

#include "ptmp/data.h"

#include <czmq.h>
#include <random>

namespace ptmp {
    namespace testing {

        // Initialize a TPSet to some default values
        void init(ptmp::data::TPSet& tps);

        // Update just the count and time values.  Needs count initialized
        // to something.
        void set_count_clock(ptmp::data::TPSet& tps);

        struct make_tps_t {

            std::normal_distribution<> normal_dist;
            std::default_random_engine generator;

            make_tps_t(int ntps, int vartps);

            void operator()(ptmp::data::TPSet& tps);
        };

        struct uniform_sleeps_t {
            int usleeptime, usleepskip;
            int64_t count;

            uniform_sleeps_t(int t, int s);
            void operator()();
        };

        struct exponential_sleeps_t {
            int usleeptime, usleepskip;
            int64_t count{0};
    
            std::exponential_distribution<double> expo_dist;
            std::default_random_engine generator;

            exponential_sleeps_t(int t, int s);
            void operator()();
        };
        
        
    }
}

#endif

    
