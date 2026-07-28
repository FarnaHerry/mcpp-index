// Smoke test — verify compat.tray header compiles (header-only lib)
#define TRAY_IMPLEMENTATION
#include "tray.h"
import std;

int main() {
    std::println("compat.tray smoke test: ok");
    return 0;
}
