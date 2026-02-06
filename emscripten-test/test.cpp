#include <iostream>
#include <emscripten/emscripten.h>

extern "C" {
    EMSCRIPTEN_KEEPALIVE
    int add(int a, int b) {
        return a + b;
    }

    EMSCRIPTEN_KEEPALIVE
    const char* greet() {
        return "Hello from WebAssembly!";
    }
}

int main() {
    std::cout << "WASM module initialized" << std::endl;
    return 0;
}
