#ifndef PTMP_FILTER_H
#define PTMP_FILTER_H

#include "ptmp/data.h"

namespace ptmp {
    namespace filter {
    
        /** Abstract base class for any "filter engine" component.
         * Note, construction of filter engine is only supported
         * through the factory.  See the TPFilter agent class for an
         * API-level class that can run an engine.  User code inherits
         * from this base, provides the operator(), adds a call to the
         * PTMP_FILTER() CPP macro to their implementation file,
         * compiles their code into a shared object library and
         * assures that library is either in LD_LIBRARY_PATH or
         * PTMP_PLUGINS.
         */
        class engine_t {
        public:
            virtual ~engine_t();
            virtual void operator()(const ptmp::data::TPSet& input_tpset,
                                    std::vector<ptmp::data::TPSet>& output_tpsets) = 0;
        };

    }
}

// Use like:
// PTMP_FILTER(MyFilterClass, my_cool_filter_name)
// name must be alphanumeric plus "_".
#define PTMP_FILTER(TYPE, ALIAS) \
extern "C" { void* ptmp_factory_make_##ALIAS##_instance(const char* config) { return new TYPE(config); } }

#define PTMP_FILTER_NOCONFIG(TYPE, ALIAS) \
extern "C" { void* ptmp_factory_make_##ALIAS##_instance(const char* /*unused*/) { return new TYPE; } }


#endif
