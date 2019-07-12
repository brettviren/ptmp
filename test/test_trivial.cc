#include "ptmp/api.h"
#include "ptmp/testing.h"

#include <czmq.h>

int main()
{
    ptmp::TPSender send(R"({
"socket": { 
  "type": "PAIR", 
  "bind": ["inproc://trivial"]
}
})");
    ptmp::TPReceiver recv(R"({
"socket": { 
  "type": "PAIR", 
  "connect": ["inproc://trivial"]
}
})");

    ptmp::data::TPSet tps;
    ptmp::testing::init(tps);

    auto tbeg = ptmp::data::now();
    const int count = 1000000;
    for (int ind=0; ind<count; ++ind) {
        ptmp::testing::set_count_clock(tps);
        send(tps);
        recv(tps);
    }
    auto tend = ptmp::data::now();

    const double dt = (tend-tbeg)*1e-6;
    const double khz = 0.001*count/dt;
    zsys_info("%d in %.3f s or %.1f kHz", count, dt, khz);

    return 0;
}
