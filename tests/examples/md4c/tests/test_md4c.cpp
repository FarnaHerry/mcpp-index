// Smoke test — verify compat.md4c header is usable + links successfully
#include "md4c.h"
import std;

int main() {
    // Verify the header is available and the ABI struct is defined
    MD_PARSER parser = {0};
    std::println("compat.md4c smoke test: ok");
    return 0;
}
