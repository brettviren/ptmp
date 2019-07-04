#include "ptmp/api.h"


ptmp::TPAgent::~TPAgent() {}


ptmp::TPAgent* ptmp::agent_factory(const std::string& type, const std::string& config)
{
    // A simple factory to return an instance based on type name.  A
    // type name matching the C++ class name returns an instance of
    // that class.  A lower case "common" type name returns the
    // prefered instance when there are multiple implementations of
    // the same concept.

    zsys_debug("agent %s with %s", type.c_str(), config.c_str());
    if (type == "TPSorted" or type == "sorted") {
        return new ptmp::TPSorted(config.c_str());
    }
    if (type == "TPZipper" or type == "zipper") {
        return new ptmp::TPZipper(config.c_str());
    }
    if (type == "TPReplay" or type == "replay") {
        return new ptmp::TPReplay(config.c_str());
    }
    if (type == "TPWindow" or type == "window") {
        return new ptmp::TPWindow(config.c_str());
    }
    if (type == "TPMonitorz" or type == "monitor") {
        return new ptmp::TPMonitorz(config.c_str());
    }
    if (type == "TPMonitor") {
        return new ptmp::TPMonitor(config.c_str());
    }
    if (type == "TPMonitorcz" or type == "monitorcz") {
        return new ptmp::TPMonitorcz(config.c_str());
    }
    if (type == "TPCat" or type == "cat" or type == "czmqat") {
        return new ptmp::TPCat(config.c_str());
    }
    return nullptr;
}
