#ifndef PTMP_DATA_H
#define PTMP_DATA_H

#include <string>

// For now we supply data classes via protobuf.
// The generated header:
#include "ptmp/ptmp.pb.h"

namespace ptmp {
    namespace data {

        // dump TPSet info to zsys_debug
        void dump(const TPSet& tpset, const std::string& msg="");

        // The type for wall clock aka "real time", used to set
        // TPSet.created.  It is in units of microseconds counted from
        // the start of the Unix epoch.  Applications should use the
        // now() function below.
        typedef int64_t real_time_t;

        // The type for hardware clock aka "data time", used for
        // TPSet.tstart and TrigPrim.tstart.
        typedef uint64_t data_time_t; 
        
        // Return the current "real time".  This is derived from
        // std::chrono::system_clock().
        real_time_t now();

    }
}

#endif
