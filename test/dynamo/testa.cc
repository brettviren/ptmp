#include "ptmp/upif.h"
#include <cassert>
int main()
{
    auto& pi = upif::plugins();
    auto plugin = pi.add("a");
    assert(plugin);

    typedef char (*funcptr)(void);
    funcptr fnp;

    bool ok = plugin->symbol("some_function", fnp);
    assert(ok);
    char got = (*fnp)();
    assert(got == 'a');
    return 0;
}
