#include "runq.h"

#include "irq/irq.h"
#include "lib/list.h"
#include "lib/trace.h"
#include "sync/smp.h"
#include "thread/thread.h"

/**
 * The most simple run queue implementation, just a queue per core
 *
 * TODO: something better later on
 */

CPU_LOCAL(static, list_t, m_run_queue);

void run_queue_init(void) {
    for (size_t i = 0; i < g_cpu_count; i++) {
        list_t* list = CPU_LOCAL_PTR_OF(m_run_queue, i);
        init_list(list);
    }
}

void run_queue_put(run_queue_entry_t* entry) {
    list_t* run_queue = CPU_LOCAL_PTR(m_run_queue);
    list_add(run_queue, &entry->link);
}

run_queue_entry_t* run_queue_get(void) {
    list_t* run_queue = CPU_LOCAL_PTR(m_run_queue);
    run_queue_entry_t* rqe = list_first_entry_or_null(run_queue, run_queue_entry_t, link);
    if (rqe != nullptr) {
        list_del(&rqe->link);
    }
    return rqe;
}
