
#include <czmq.h>
#include "ptmp/upif.h"

int main () {
    zsys_init();
    upif::cache c;
    auto plugin = c.add("ptmp");
    assert(plugin);
    zsys_info("loaded ptmp plugin");
    auto raw = plugin->rawsym("funcs_test");
    assert(raw);
    zsys_info("got funcs_test symbol");

    // pointer to function
    typedef int (*funcptr)(int);
    funcptr fnp;

    bool ok = plugin->symbol("funcs_test", fnp);
    assert(ok);

    const int x = 42;
    const int y = (*fnp)(x);
    assert(x == y);
    
    return 0;
}
