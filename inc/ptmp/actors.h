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
        
    }
}

#endif
