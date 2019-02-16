#ifndef PTMP_INTERNALS_H
#define PTMP_INTERNALS_H

#include <czmq.h>
#include <string>

namespace ptmp {
    namespace internals {


        int socket_type(std::string name);

        zsock_t* endpoint(const std::string& config);

        class Socket {
            zsock_t* m_sock;
        public:
            Socket(const std::string& config);
            ~Socket();
            zsock_t* get() { return m_sock; }
        };
    }
}

#endif
