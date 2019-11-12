#include <dlfcn.h>
#include <czmq.h>
#include <string>
#include <iostream>

int main(int argc, char* argv[])
{
    zsys_init();

    std::string lname = "libptmp.so";
    if (argc > 1) {
        lname = argv[1];
    }
    std::string sym = "funcs_test";
    if (argc > 2) {
        sym = argv[2];
    }
    void* lib = dlopen(lname.c_str(), RTLD_NOW);
    if (!lib) {
        std::cerr << "failed to find library: \"" << lname << "\"\n";
        std::cerr << " -> Check spelling?  Set LD_LIBRARY_PATH?\n";
    }
    assert(lib);
    std::cout << "found plugin libray " << lname << std::endl;

    void* raw = dlsym(lib, sym.c_str());
    assert(raw);
    std::cout << "got raw sym \"" << sym << "\" from " << lname << " or dependencies\n";

    return 0;
}
