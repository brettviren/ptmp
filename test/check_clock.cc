#include <chrono>
#include <czmq.h>
#include <ctime>
#include <iostream>
#include <unistd.h>
using namespace std::chrono;
int main()
{
    zsys_init();
    zsys_info("time:                  %ld", time(0));
    zsys_info("zclock_time:           %ld", zclock_time());
    zsys_info("zclock_mono:           %ld", zclock_mono());
    zsys_info("zclock_usecs:          %ld", zclock_usecs());

    
    {
        system_clock::time_point tp = system_clock::now();
        system_clock::duration dtn = tp.time_since_epoch();
        std::cout <<"chrono system periods: " << dtn.count() << "\n";
    }
    {
        steady_clock::time_point tp = steady_clock::now();
        steady_clock::duration dtn = tp.time_since_epoch();
        std::cout <<"chrono steady periods: " << dtn.count() << "\n";
    }
    {
        high_resolution_clock::time_point tp = high_resolution_clock::now();
        system_clock::duration dtn = tp.time_since_epoch();    
        std::cout <<"chrono hirez periods:  " << dtn.count() << "\n";
    }
    auto tp1 = system_clock::now();
    usleep(1100000);
    auto tp2 = system_clock::now();
    auto dt = tp2-tp1;
    assert (dt > seconds{1});
    std::cout << "dt: " << dt.count() << "\n";

    int64_t us = duration_cast<microseconds>(tp1.time_since_epoch()).count();
    zsys_info("%ld us", us);
    return 0;
}
