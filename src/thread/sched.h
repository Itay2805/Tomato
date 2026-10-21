#pragma once

#include "sync/smp.h"
#include "thread/thread.h"
#include "util/defs.h"
#include <stdnoreturn.h>

/**
 * The currently running thread
 */
CPU_LOCAL(extern, thread_t*, g_current_thread);

/**
 * Initialize the scheduler for all the cores
 */
INIT_CODE void sched_init(void);

/**
 * Wait for all cores to start the scheduler
 */
INIT_CODE void sched_wait(void);

/**
 * Start the scheduler, called per-core
 */
INIT_CODE noreturn void sched_start_per_core(void);

/**
 * Enter the scheduler and potentially select a new thread to run.
 *
 * Must be called with interrupts disabled and returns with interrupts disabled.
 */
void scheduler_schedule(void);
