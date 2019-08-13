#include "ptmp/factory.h"

#include "json.hpp"
#include <cstdlib>

using json = nlohmann::json;

ptmp::TPAgent::~TPAgent()
{
}


/// this is used for testing in test/test_upif
extern "C" {
    int funcs_test(int x);
}
int funcs_test(int x)
{
    return x;
}
