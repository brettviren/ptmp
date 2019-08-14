#include <iostream>

extern "C" { 
    char some_function();
}

char some_function()
{
    std::cout << "libb\n";
    return 'b';
}
        
