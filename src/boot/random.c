#include "random.h"

#include "lib/assert.h"
#include "limine.h"
#include <stdint.h>

[[gnu::section(".limine_requests")]]
static volatile struct limine_entropy_request m_entropy_request = {
    .id = LIMINE_ENTROPY_REQUEST_ID,
    .revision = 0,
    .response = nullptr,
    .value_count = RANDOM_INDEX_MAX,
};

static bool m_entropy_given[RANDOM_INDEX_MAX] = {};

uint64_t boot_random_get(random_index_t index) {
    ASSERT(m_entropy_request.response != nullptr);
    ASSERT(index < m_entropy_request.response->value_count);
    ASSERT(!m_entropy_given[index]);
    m_entropy_given[index] = true;
    uint64_t value = m_entropy_request.response->values[index];
    m_entropy_request.response->values[index] = 0;
    return value;
}
