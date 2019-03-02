#ifndef PTMP_TESTING_H
#define PTMP_TESTING_H

#include "ptmp/data.h"

#include <czmq.h>
#include <random>

namespace ptmp {
    namespace testing {

        // Initialize a TPSet to some default values
        void init(ptmp::data::TPSet& tps);

        //
        // note, if a bare time as an int64_t is used, it is in units
        // of nanoseconds unless otherwise stated.  If absolute, then
        // it counds the nanoseconds from the start of the Unix epoch.
        //

        // Update just the count and time values.  Needs count initialized
        // to something.
        void set_count_clock(ptmp::data::TPSet& tps);

        // Set a timestamp to the given *absolute* nanoseconds value
        // and with some additional, optional latency also in
        // nanoseconds.  If t is zero then system wall clock is used.
        // Latency may be negative.
        void set_timestamp(ptmp::data::Timestamp& ts, int64_t t=0, int64_t latency=0);

        // Set ts to t0 with an additional relative delay in
        // nanoseconds.
        void set_relative_timestamp(ptmp::data::Timestamp& ts,
                                    const ptmp::data::Timestamp& t0,
                                    int64_t delay=0);

        // Return the time span in nanoseconds between two time
        // stamps.  Ie, return (ta-tb).
        int64_t timestamp_span(const ptmp::data::Timestamp& ta,
                               const ptmp::data::Timestamp& tb);

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

    
