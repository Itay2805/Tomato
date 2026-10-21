#pragma once

#include "lib/object.h"
#include "mem/vmar.h"
#include "thread/runq.h"
#include "util/except.h"
#include <stdnoreturn.h>

/**
 * The various states in which a thread can be:
 *
 * - IDLE: newly-created, not running
 * - DEAD: no longer running, will be freed when when all refs are released
 * - READY: currently scheduled out, but not parked (blocked) on anything
 * - RUNNING: currently running on a CPU
 * - PARKING: preparing to park until woken, but currently still running
 * - PARKED: waiting until an explicit unpark is issued
 *
 * The following transitions are allowed:
 *
 *         IDLE
 *          |
 *          V
 *    +-> READY
 *    |     |
 *    ^     V
 *    +- RUNNING -> DEAD
 *    |    ^|
 *    ^    |V
 *    +- PARKING
 *    |     |
 *    ^     V
 *    +- PARKED
 *
 * - IDLE -> READY: performed non-atomically by `thread_start` (should happen at most once).
 *
 * - READY -> RUNNING: performed non-atomically by the run queue in which the thread is
 *   currently waiting to run; may happen as a result of a race while doing PARKING -> RUNNING
 *   by a thread aborting a park.
 *
 * - RUNNING -> DEAD: performed non-atomically by the scheduler when the thead calls `thread_exit`
 *   (can happen at most once).
 *
 * - RUNNING -> PARKING: performed non-atomically only by the thread itself when preparing to park;
 *   exists to avoid races in condition-based parking (see `atomic_wait`) when checking the wait
 *   condition itself.
 *
 * - PARKING -> PARKED: performed as an atomic CAS by the scheduler once a PARKING thread has been
 *   switched out; may race against PARKING -> READY transitions performed by concurrent
 *   `scheduler_try_unpark` calls.
 *
 * - PARKING -> READY: performed as an atomic CAS by `scheduler_try_unpark` to abort an in-progress
 *   park operation; may race against the scheduler's PARKING -> PARKED transition and concurrent
 *   `scheduler_try_unpark` calls.
 *
 * - PARKING -> RUNNING: performed non-atomically only by the thread itself when parking as been
 * aborted; may race against PARKING -> READY transitions performed by concurrent
 * `scheduler_try_unpark` calls.
 *
 * - PARKED -> READY: performed as an atomic CAS by `scheduler_try_unpark` to unpark a thread; may
 *   race against concurrent `scheduler_try_unpark` calls.
 *
 */
typedef enum thread_state {
    THREAD_STATE_IDLE,
    THREAD_STATE_DEAD,
    THREAD_STATE_READY,
    THREAD_STATE_RUNNING,
    THREAD_STATE_PARKING,
    THREAD_STATE_PARKED,
} thread_state_t;

typedef struct thread {
    /**
     * The stack of the thread
     * MUST NOT MOVE THE FIELD
     * offset 0
     */
    void* kernel_rsp;

    /**
     * The object header of the thread
     */
    object_t object;

    /**
     * The kernel stack vmar
     */
    vmar_t* kernel_stack;

    /**
     * The runqueue entry
     */
    run_queue_entry_t rqe;

    /**
     * The current state of the thread
     */
    _Atomic(thread_state_t) state;
} thread_t;

typedef void (*thread_entry_t)(void* arg);

/**
 * Create a new thread
 */
err_t thread_create(thread_t** thread, thread_entry_t entry, void* arg, const char* name_fmt, ...);

/**
 * Start a thread that was not running yet
 */
void thread_start(thread_t* thread);

/**
 * Exit from the current thread
 */
noreturn void thread_exit(void);

/**
 * Yield the current thread
 */
void thread_yield(void);

/**
 * free a thread object
 */
void thread_free(thread_t* thread);

/**
 * The thunk that we should jump to
 * when starting a thread
 */
void thread_entry_thunk(void);

/**
 * Switching to another thread, saving
 * the current context first
 */
void thread_switch(thread_t* to, thread_t* from);

/**
 * Bootstrap a thread, starting up the entire scheduling process
 */
INIT_CODE noreturn void thread_bootstrap(thread_t* thread);
