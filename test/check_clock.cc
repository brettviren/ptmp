
#include <czmq.h>
#include <ctime>

int main()
{
    zsys_init();
    zsys_info("time:            %ld", time(0));
    zsys_info("zclock_time:     %ld", zclock_time());
    zsys_info("zclock_mono:     %ld", zclock_mono());
    zsys_info("zclock_usecs:    %ld", zclock_usecs());
    zsys_info("time-ztime/1000: %ld", time(0)- zclock_time()/1000);
    zsys_info("time-zmono/1000: %ld", time(0)- zclock_mono()/1000);
    return 0;
}
