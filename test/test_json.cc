#include "json.hpp"

#include <iostream>

using json = nlohmann::json;

int main()
{
    // note, this isn't real JSON schema for anything in PTMP
    auto jcfg = R"(
{
   "name": "testcontrol",
   "ports": [ ],
   "type": "control",
   "bind": "tcp://127.0.0.1:12345"
}
)"_json;
    std::cout << jcfg.dump() << std::endl;

    std::string type = jcfg["type"];
    std::cout << "type: " << type << std::endl;

    json cfg = { { "socket",
                   {
                       { "type", jcfg["type"] },
                       { "bind", jcfg["bind"] } }}};
    std::cout << "cfg: " << cfg << std::endl;


    return 0;
}
