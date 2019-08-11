#include "ptmp/stringutil.h"


std::string ptmp::stringutil::trim(const std::string& str,
                                   const std::string& kill)
{
    const auto strBegin = str.find_first_not_of(kill);
    if (strBegin == std::string::npos) {
        return ""; // no content
    }

    const auto strEnd = str.find_last_not_of(kill);
    const auto strRange = strEnd - strBegin + 1;

    return str.substr(strBegin, strRange);
}

void ptmp::stringutil::find_replace(std::string & data,
                                    std::string have, std::string want)
{
    size_t pos = data.find(have);
 
    while (pos != std::string::npos) {
        data.replace(pos, have.size(), want);
        pos = data.find(have, pos + want.size());
    }
}

