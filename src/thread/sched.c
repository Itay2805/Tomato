#include "sched.h"
#include "debug/trace.h"
#include "irq/irq.h"
#include "lib/assert.h"
#include "lib/container_of.h"
#include "lib/object.h"
#include "sync/smp.h"
#include "thread/runq.h"
#include "thread/thread.h"
#include "util/atomic.h"
#include "util/defs.h"

typedef enum last_thread_action {
    LAST_THREAD_ACTION_NONE,
    LAST_THREAD_ACTION_PARK,
    LAST_THREAD_ACTION_PUT,
} last_thread_action_t;

/**
 * The last thread just switched out.
 */
DEFINE_CPU_LOCAL(static, thread_t*, last_thread);

/**
 * The action to perform on `last_thread` in the context of the newly-switched thread.
 */
DEFINE_CPU_LOCAL(static, last_thread_action_t, last_thread_action);

/**
 * The idle thread of the current core
 */
DEFINE_CPU_LOCAL(static, thread_t*, idle_thread);

/**
 * The currently running thread
 */
DEFINE_CPU_LOCAL(, thread_t*, current_thread);

INIT_DATA atomic_size_t m_sched_start_count = 0;

[[clang::noinline]]
noreturn static void scheduler_idle_loop(void) {
    for (;;) {
        // attempt to schedule, this will return once there is no
        // more work to run
        scheduler_schedule();

        // wait for an interrupt to give us more work
        irq_wait();
    }

    __builtin_unreachable();
}

INIT_CODE static void scheduler_idle_thread(void* arg) {
    (void)arg;

    // mark we started scheduling
    m_sched_start_count++;

    // and enter the idle loop without interrupts
    irq_disable();
    scheduler_idle_loop();
}

INIT_CODE void sched_init(void) {
    // setup the run queue for all the cores
    run_queue_init();

    // create all the idle threads
    for (size_t i = 0; i < g_cpu_count; i++) {
        thread_t* thread;
        ASSERT_SUCCESS(
            thread_create(&thread, scheduler_idle_thread, nullptr, "{idle-%d}", CPU_LOCAL(cpu_id)));
        atomic_store_relaxed(&thread->state, THREAD_STATE_READY);
        *CPU_LOCAL_PTR_OF(idle_thread, i) = thread;
    }
}

INIT_CODE void sched_wait(void) {
    // we are going to do a busy loop for the start since its just for the cpu startup,
    // so we don't care toooo much
    while (m_sched_start_count != g_cpu_count)
        ;
}

static thread_t* sched_select_thread(void) {
    run_queue_entry_t* entry = run_queue_get();
    if (entry != nullptr) {
        return container_of(entry, thread_t, rqe);
    } else {
        return CPU_LOCAL(idle_thread);
    }
}

INIT_CODE void sched_start_per_core(void) {
    irq_disable();
    thread_bootstrap(CPU_LOCAL(idle_thread));
}

static void scheduler_finish_park(thread_t* thread) {
    thread_state_t state = THREAD_STATE_PARKING;
    if (atomic_compare_exchange_release(&thread->state, &state, THREAD_STATE_PARKED)) {
        // We're done -- the thread is now safely parked and can no longer race with
        // `scheduler_try_unpark`. The release store above synchronizes-with the acquire fence
        // in `scheduler_try_unpark` to ensure that the thread's state is consistent if it ends
        // up being unparked on a different core.
        return;
    }

    if (state == THREAD_STATE_READY) {
        // A concurrent `scheduler_try_unpark` has caught us during the park operation; we are
        // now responsible for re-enqueuing the thread. The acquire fence here synchronizes-with
        // the release fence in `scheduler_try_unpark` to ensure the unpark happens-before the
        // thread resumes execution.
        atomic_fence_acquire();
        run_queue_put(&thread->rqe);
    }
}

void scheduler_after_schedule(void) {
    // We are now executing in the context of the new thread, but may need to clean up
    // after the old thread.
    switch (CPU_LOCAL(last_thread_action)) {
        case LAST_THREAD_ACTION_NONE:
            break;
        case LAST_THREAD_ACTION_PARK:
            scheduler_finish_park(CPU_LOCAL(last_thread));
            break;
        case LAST_THREAD_ACTION_PUT:
            OBJECT_PUT(CPU_LOCAL(last_thread));
            break;
    }

    CPU_LOCAL(last_thread) = nullptr;
    CPU_LOCAL(last_thread_action) = LAST_THREAD_ACTION_NONE;
}

void scheduler_schedule(void) {
    // Interrupts must be disabled in the scheduler
    ASSERT(!irq_enabled());

    thread_t* current = CPU_LOCAL(current_thread);

    thread_state_t state = atomic_load_relaxed(&current->state);
    switch (state) {
        case THREAD_STATE_DEAD: {
            // The thread has died, drop the ref the scheduler owns.
            CPU_LOCAL(last_thread_action) = LAST_THREAD_ACTION_PUT;
        } break;

        case THREAD_STATE_PARKING: {
            // Finish parking the thread once we've switched away from it.
            CPU_LOCAL(last_thread_action) = LAST_THREAD_ACTION_PARK;
        } break;

        case THREAD_STATE_RUNNING: {
            // place back on the run queue and set the state as ready
            atomic_store_relaxed(&current->state, THREAD_STATE_READY);

            // only put non-idle threads into the run queue
            if (current != CPU_LOCAL(idle_thread)) {
                run_queue_put(&current->rqe);
            }
        } break;

        default:
            ASSERT(!"Invalid thread state");
    }

    // remember the last thread
    CPU_LOCAL(last_thread) = current;

    // select a new thread
    thread_t* new_thread = sched_select_thread();
    ASSERT(new_thread != nullptr);
    ASSERT(atomic_load_relaxed(&new_thread->state) == THREAD_STATE_READY);
    atomic_store_relaxed(&new_thread->state, THREAD_STATE_RUNNING);

    // TODO: set the preemption timer

    if (new_thread != current) {
        // actually switch to the new thread
        CPU_LOCAL(current_thread) = new_thread;
        thread_switch(new_thread, current);
    }

    // cleanup after schedule
    scheduler_after_schedule();
}

void thread_entry_point(thread_entry_t entry, void* arg) {
    // let the scheduler know all is good
    scheduler_after_schedule();

    // the scheduler runs with interrupts disabled, but threads run with them enabled
    irq_enable();

    // we can now call the thread's
    // real entry point
    entry(arg);

    // and we can exit normally now
    thread_exit();
}
