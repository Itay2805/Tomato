#pragma once

#include <stdint.h>

/**
 * The saved state when switching between threads, the callee saved registers
 */
typedef struct thread_entry_frame {
    // NOTE: must be in-sync with the assembly
    // NOTE: x19 and x20 must be in sync with thread_entry_thunk
    uint64_t x19;
    uint64_t x20;
    uint64_t x21;
    uint64_t x22;
    uint64_t x23;
    uint64_t x24;
    uint64_t x25;
    uint64_t x26;
    uint64_t x27;
    uint64_t x28;
    uint64_t x29;
    uint64_t x30;
} thread_entry_frame_t;
static_assert(sizeof(thread_entry_frame_t) % 16 == 0, "the stack must stay 16 byte aligned");

/**
 * The thunk that we should jump to
 * when starting a thread
 */
void thread_entry_thunk(void);

/**
 * Setup the frame of a new thread, switching to it calls thread_entry_point(entry, arg)
 */
static inline void thread_entry_frame_init(thread_entry_frame_t* frame, uintptr_t entry,
                                           uintptr_t arg) {
    *frame = (thread_entry_frame_t){
        .x19 = entry,
        .x20 = arg,
        .x30 = (uintptr_t)thread_entry_thunk,
    };
}
