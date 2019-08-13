/** 
    PTMP proxy classes are implemented as CZMQ actors.  These are
    their functions.
 */

#ifndef PTMP_ACTORS
#define PTMP_ACTORS

namespace ptmp {
    namespace actor {

        void monitor(zsock_t* pipe, void* vargs);
        void tap(zsock_t* pipe, void* vargs);
        void replay(zsock_t* pipe, void* vargs);
        void sorted(zsock_t* pipe, void* vargs);
        void zipper(zsock_t* pipe, void* vargs);
        void window(zsock_t* pipe, void* vargs);
        void cat(zsock_t* pipe, void* vargs);
        void composer(zsock_t* pipe, void* vargs);
        void filter(zsock_t* pipe, void* vargs);
    }
}

// An implementation of an agent needs to call this CPP macro if it is
// to be used in factory methods.

#define PTMP_AGENT(TYPE, ALIAS) \
extern "C" { void* ptmp_factory_make_##ALIAS##_instance(const char* config) { return new TYPE(config); } }


#endif
