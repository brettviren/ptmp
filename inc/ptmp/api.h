/** The high level PTMP API
 */

#ifndef PTMP_API_H
#define PTMP_API_H

#include <string>
#include <vector>

#include "ptmp/data.h"
#include "ptmp/internals.h"

namespace ptmp {


    class TPSender {
        internals::Socket* m_sock;
    public:
        /// Create a TPSender with a configuration string.
        TPSender(const std::string& config);
        ~TPSender();

        /// Send one TPSet.  
        void operator()(const data::TPSet& tps);

        TPSender() =default;
        TPSender(const TPSender&) =delete;
        TPSender& operator=(const TPSender&) =delete;            
    };

    class TPReceiver {
        internals::Socket* m_sock;
    public:
        /// Create a TPReceiver with a configuration string.
        TPReceiver(const std::string& config);
        ~TPReceiver();

        /// Receive next TPSet, by filling.  If a timeout occurs, or
        /// otherwise a break is received, this will return false.  If
        /// a message is received but is corrupted this will throw
        /// runtime_error.
        bool operator()(data::TPSet& tps, int timeout_msec=-1);

        TPReceiver() =default;
        TPReceiver(const TPReceiver&) =delete;
        TPReceiver& operator=(const TPReceiver&) =delete;            
    };

    // A "device" that accepts TPSets from N TPSenders on input and
    // produces an ordered output stream of their TPSets on output.
    // Synchronizing is done on the "tstart" time of the TPSets.
    class TPSorted {
    public:
        // Create a TPSorted with one config for input sockets and one
        // for output.  Note, each output message is sent to all
        // output sockets.  The config is two objects each like
        // TPSender/TPReceiver held by a key "sender" and "receiver".
        TPSorted(const std::string& config);

        // The TPSorted will run a thread needs to be kept in scope
        // but otherwise doesn't have a serviceable API.  Destroy it
        // when done.
        ~TPSorted();
    private:
        zactor_t* m_actor;
        
    };

}

#endif
