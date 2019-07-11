#include "ptmp/api.h"
#include "ptmp/upif.h"

int main()
{
    zsys_init();

    auto a = ptmp::agent_factory("zipper", "{}");
    assert(a);
    zsys_debug("got zipper factory func, again");    

    zclock_sleep(1000);
    delete a;

    return 0;
}
