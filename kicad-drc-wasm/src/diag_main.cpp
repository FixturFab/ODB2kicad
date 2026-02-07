/**
 * Diagnostic main for WASM - catches exceptions from static constructors.
 */
#include <cstdio>
#include <cstdlib>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

static volatile int s_abort_count = 0;

// Override abort() to NOT actually abort in WASM.
// libc's abort() compiles to __builtin_trap() → "unreachable" instruction.
// Our override prints a message and returns (UB but diagnostic).
extern "C" {
void abort() {
    s_abort_count++;
    fprintf(stderr, "[ABORT INTERCEPTED #%d] abort() called\n", s_abort_count);
    emscripten_log(EM_LOG_ERROR | EM_LOG_C_STACK | EM_LOG_JS_STACK,
                   "abort() call stack:");
    // Return instead of trapping - UB but lets initialization continue
}
}

// Override __assert_fail to NOT call abort() - just print the message.
extern "C" {
void __assert_fail(const char* assertion, const char* file,
                   unsigned int line, const char* function) {
    fprintf(stderr, "[ASSERT FAIL] %s at %s:%u in %s\n", assertion, file, line, function);
    // Do NOT call abort() - just continue
}
}

// Early constructor to verify static init starts
__attribute__((constructor(101)))
static void diag_early_ctor() {
    fprintf(stderr, "[DIAG] Early constructor executing (priority 101)\n");
}

// Late constructor to verify static init completes
__attribute__((constructor(65535)))
static void diag_late_ctor() {
    fprintf(stderr, "[DIAG] Late constructor executing (priority 65535)\n");
}

extern "C" {
EMSCRIPTEN_KEEPALIVE
int diag_ping() {
    printf("diag_ping called\n");
    return 42;
}
}
#endif
