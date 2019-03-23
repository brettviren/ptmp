#ifndef PTMP_INTERNALS_H
#define PTMP_INTERNALS_H
#include "ptmp/data.h"
#include <czmq.h>
#include <string>

namespace ptmp {
    namespace internals {


        int socket_type(std::string name);

        zsock_t* endpoint(const std::string& config);

        void recv(zmsg_t* &msg, ptmp::data::TPSet& tps);
        void send(zsock_t* sock, const ptmp::data::TPSet& tps);

        //void send_cfg(zsock_t* sock, const char* cfgstr);

        class Socket {
            zsock_t* m_sock;
            zpoller_t* m_poller;
        public:
            Socket(const std::string& config);
            ~Socket();
            zsock_t* get() { return m_sock; }

            // Return a message.  Negative timeout waits inefinitely.
            zmsg_t* msg(int timeout_msec=-1);

        };
    }
}

#endif
