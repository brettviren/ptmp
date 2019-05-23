#include <czmq.h>
#include "ptmp/api.h"
#include "ptmp/testing.h"

int main()
{
    zsys_init();
    for (int ind=0; ind<10000; ++ind) {
        usleep(10);
        ptmp::data::TPSet tps;
        ptmp::testing::init(tps);
        int64_t now = zclock_usecs();
        int64_t cre = tps.created();
        assert(now != 0);
        zsys_debug("now: %ld, created: %ld, diff: %ld",
                   now, cre, now-cre);
    }
    return 0;
}
