/** The high level PTMP API
 */

#ifndef PTMP_API_H
#define PTMP_API_H

#include <string>

// brings in class in ptmp::data:: namespace
#include "ptmp/ptmp.pb.h"


namespace ptmp {

    namespace internals {
        class Socket;
    }

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

}

#endif
