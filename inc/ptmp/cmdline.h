/*
  This namespace holds support for building command line programs with common CLI.
 */

#ifndef PTMP_CMDLINE
#define PTMP_CMDLINE

#include "CLI11.hpp"
#include "json.hpp"

namespace ptmp {
    namespace cmdline {


        struct sock_options_t {
            int hwm;
            std::string socktype;
            std::string attach;
            std::vector<std::string> endpoints;
        };

        
        void add_socket_options(CLI::App* app, sock_options_t& opt);

        nlohmann::json to_json(sock_options_t& opt);

    }
}

#endif
