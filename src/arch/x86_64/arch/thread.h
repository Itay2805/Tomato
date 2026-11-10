#pragma once

#include <stdint.h>

/**
 * The saved state when switching between threads
 */
typedef struct [[gnu::packed]] thread_entry_frame {
    // NOTE: must be in-sync with the assembly
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t rbx;
    uint64_t rbp;
    uint64_t rip;

    // NOTE: must be in sync with thread_entry_thunk
    uint64_t rdi;
    uint64_t rsi;
} thread_entry_frame_t;

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
        .rip = (uintptr_t)thread_entry_thunk,
        .rdi = entry,
        .rsi = arg,
    };
}
