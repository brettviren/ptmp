#include "ptmp/api.h"

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

    auto tbeg = zclock_usecs();
    const int count = 1000000;
    for (int ind=0; ind<count; ++ind) {
        send(tps);
        recv(tps);
    }
    auto tend = zclock_usecs();

    const double dt = (tend-tbeg)*1e-6;
    const double khz = 0.001*count/dt;
    zsys_info("%d in %.3f s or %.1f kHz", count, dt, khz);

    return 0;
}
