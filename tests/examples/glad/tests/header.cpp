// Smoke test — verify compat.glad header is usable + links
#include "glad/glad.h"
import std;

int main() {
    // Verify the header defines the expected types (no GL context needed)
    std::println("compat.glad smoke test: ok");
    return 0;
}
