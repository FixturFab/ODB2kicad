/**
 * Minimal WASM test - just enough to test static initialization
 * without the full KiCad codebase.
 */
#include <cstdio>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

extern "C" {
EMSCRIPTEN_KEEPALIVE
int minimal_ping() {
    printf("minimal_ping called\n");
    return 42;
}
}
#endif

int main() {
    printf("main() called\n");
    return 0;
}
