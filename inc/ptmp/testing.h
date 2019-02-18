#ifndef PTMP_TESTING_H
#define PTMP_TESTING_H

#include "ptmp/data.h"

namespace ptmp {
    namespace testing {

        // Initialize a TPSet to some default values
        void init(ptmp::data::TPSet& tps);

        // Update just the count and time values.  Needs count initialized
        // to something.
        void set_count_clock(ptmp::data::TPSet& tps);

    }
}

#endif

    
