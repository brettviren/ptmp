#include "ptmp/factory.h"


ptmp::factory::FactoryBase::FactoryBase()
{
    const std::string delim = ",";
    std::string pis = "ptmp" + delim;
    const char* cpis = std::getenv("PTMP_PLUGINS"); // comma separated, please
    if (cpis) {
        pis += cpis + delim;
    }
    
    auto& pim = upif::plugins();

    auto beg=0U;
    auto end=pis.find(delim);
    while (end != std::string::npos) {
        std::string pi = pis.substr(beg, end-beg);
        //zsys_debug("adding plugin: \"%s\"", pi.c_str());
        pim.add(pi);
        beg = end + delim.length();
        end = pis.find(delim, beg);
    }
}

void* ptmp::factory::FactoryBase::make_void(const std::string& alias, const std::string& config)
{
    // This must match what PTMP_*() macro builds
    std::string fname = "ptmp_factory_make_" + alias + "_instance";
    auto& pim = upif::plugins();
    auto pi = pim.find(fname);
    if (!pi) {
        zsys_error("failed to find symbol: \"%s\"", fname.c_str());
        return nullptr;
    }

    ptmp::factory::maker_func mf;
    bool ok = pi->symbol(fname.c_str(), mf);
    if (!ok) { return nullptr; }
    return ((*mf)(config.c_str()));
}

