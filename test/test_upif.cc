
#include <czmq.h>
#include "upif.h"

int main () {
    const char* funcname = "ptmp_upif_test";

    upif::cache c;
    auto plugin = c.add("ptmp");
    assert(plugin);
    auto raw = plugin->rawsym(funcname);
    assert(raw);

    // pointer to function
    typedef int (*funcptr)(int);
    funcptr fnp;

    bool ok = plugin->symbol(funcname, fnp);
    assert(ok);

    const int x = 42;
    const int y = (*fnp)(x);
    assert(x == y);
    
    return 0;
}
