#include "lib/assert.h"
#include <stdint.h>

uintptr_t __stack_chk_guard = 0;

[[gnu::noreturn]]
void __stack_chk_fail(void) {
    ASSERT(0, "stack smashing detected");
}
