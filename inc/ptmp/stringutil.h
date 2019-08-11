#ifndef PTMP_STRINGUTIL_H
#define PTMP_STRINGUTIL_H

#include <string>

namespace ptmp {
    namespace stringutil {

        std::string trim(const std::string& str,
                         const std::string& kill = " \t");

        void find_replace(std::string & data,
                          std::string have, std::string want);

    }
}



#endif

