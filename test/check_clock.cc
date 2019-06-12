#include <chrono>
#include <czmq.h>
#include <ctime>
using namespace std::chrono;
int main()
{
    zsys_init();
    zsys_info("time:                  %ld", time(0));
    zsys_info("zclock_time:           %ld", zclock_time());
    zsys_info("zclock_mono:           %ld", zclock_mono());
    zsys_info("zclock_usecs:          %ld", zclock_usecs());

    
    system_clock::time_point sys_tp = system_clock::now();
    system_clock::duration sys_dtn = sys_tp.time_since_epoch();
    zsys_info("chrono system periods: %ld", sys_dtn.count());

    high_resolution_clock::time_point hrc_tp = high_resolution_clock::now();    
    system_clock::duration hrc_dtn = hrc_tp.time_since_epoch();    
    zsys_info("chrono hirez periods:  %ld", hrc_dtn.count());

    return 0;
}
