#include "ptmp/api.h"


ptmp::TPAgent::~TPAgent() {}


ptmp::TPAgent* ptmp::agent_factory(const std::string& type, const std::string& config)
{
    zsys_debug("agent %s with %s", type.c_str(), config.c_str());
    if (type == "TPSorted" or type == "sorted") {
        return new ptmp::TPSorted(config.c_str());
    }
    if (type == "TPReplay" or type == "replay") {
        return new ptmp::TPReplay(config.c_str());
    }
    if (type == "TPWindow" or type == "window") {
        return new ptmp::TPWindow(config.c_str());
    }
    if (type == "TPMonitor" or type == "monitor") {
        return new ptmp::TPMonitor(config.c_str());
    }
    if (type == "TPCat" or type == "cat" or type == "czmqat") {
        return new ptmp::TPCat(config.c_str());
    }
    return nullptr;
}
