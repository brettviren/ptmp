#include "ptmp/api.h"


ptmp::TPAgent::~TPAgent() {}


ptmp::TPAgent* ptmp::agent_factory(const std::string& type, const std::string& config)
{
    if (type == "TPSorted" or type == "sorted") {
        return new ptmp::TPSorted(config);
    }
    if (type == "TPReplay" or type == "replay") {
        return new ptmp::TPSorted(config);
    }
    if (type == "TPWindow" or type == "window") {
        return new ptmp::TPSorted(config);
    }
    if (type == "TPMonitor" or type == "monitor") {
        return new ptmp::TPSorted(config);
    }
    if (type == "TPCat" or type == "cat" or type == "czmqat") {
        return new ptmp::TPCat(config);
    }
    return nullptr;
}
