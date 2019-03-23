#include "ptmp/testing.h"

#include <functional>
#include <czmq.h>

int main()
{
    zsys_init();
    ptmp::testing::exponential_sleeps_t foo(1234,0);
    foo();
    ptmp::testing::exponential_sleeps_t foo2 = foo;
    foo2();

    assert(foo2.usleeptime == 1234);
    std::function<void()> sleepy_time = foo;
    sleepy_time();
    return 0;
}
