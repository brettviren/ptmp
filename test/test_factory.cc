#include "ptmp/api.h"
#include "ptmp/factory.h"

int main()
{
    zsys_init();

    ptmp::TPAgent* a = ptmp::factory::make<ptmp::TPAgent>("zipper", "{}");
    assert(a);
    zsys_debug("test_factory: got zipper from factory");
    zclock_sleep(2000);
    zsys_debug("test_factory: deleting zipper");
    delete a;
    zclock_sleep(2000);
    zsys_debug("test_factory: exiting");    
    return 0;
}
