#include "json.hpp"

#include <iostream>

using json = nlohmann::json;

int main()
{
    auto jcfg = R"(
{
   "name": "testcontrol",
   "ports": [ ],
   "type": "control"
}
)"_json;
    std::cout << jcfg.dump() << std::endl;

    std::string type = jcfg["type"];
    std::cout << "type: " << type << std::endl;


    return 0;
}
