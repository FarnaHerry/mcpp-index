#include <cstddef>
#include <cstdio>
#include <cstring>
#include "message_txt.h"

int main() {
    const char* text = reinterpret_cast<const char*>(fixture::message_txt);
    if (fixture::message_txt_size != std::strlen("hello from mcpp.plugins")) return 1;
    if (std::strcmp(text, "hello from mcpp.plugins") != 0) return 1;
    std::puts("mcpp-plugins-tests: the embedded file reached the program");
    return 0;
}
