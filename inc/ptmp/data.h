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

    }
}

#endif
