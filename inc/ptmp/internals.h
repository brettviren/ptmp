#ifndef PTMP_INTERNALS_H
#define PTMP_INTERNALS_H
#include "ptmp/data.h"
#include <czmq.h>
#include <cstdio>
#include <string>
#include <vector>

namespace ptmp {
    namespace internals {


        int socket_type(std::string name);

        zsock_t* endpoint(const std::string& config);
        std::vector<zsock_t*> perendpoint(const std::string& config);

        void recv(zmsg_t* &msg, ptmp::data::TPSet& tps);
        void send(zsock_t* sock, const ptmp::data::TPSet& tps);

        void microsleep(ptmp::data::real_time_t microseconds);


        // Very primitive file I/O.
        // Caller takes ownership of returned msg.
        zmsg_t* read(FILE* fp);
        int write(FILE* fp, zmsg_t* msg);


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
