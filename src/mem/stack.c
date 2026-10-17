#include "stack.h"
#include "arch/virt.h"
#include "lib/object.h"
#include "lib/string.h"
#include "mem/mapping.h"
#include "mem/vmar.h"
#include "util/defs.h"
#include "util/except.h"
#include <stdint.h>

err_t stack_alloc(stack_allocation_t* allocation, size_t size, bool user) {
    err_t err = SUCCESS;

    vmar_t* root = user ? &g_user_region : &g_kernel_region;
    size_t page_count = DIV_ROUND_UP(size, PAGE_SIZE);

    // allocate the region on top of the stack, we have a page from either
    // side to act as a guard page for both ends (protects against overflow
    // and against underflow)
    vmar_t* stack_region = nullptr;
    RETHROW(vmar_allocate(root, &stack_region, VMAR_ANY_OFFSET, page_count + 2));
    errdefer {
        vmar_unmap(stack_region);
        OBJECT_PUT(stack_region);
    }

    stack_region->role = VMAR_ROLE_STACK_GUARD;
    strcpy(stack_region->name, "stack-guard");

    // now allocate the stack itself, we don't need its ref once we are done with it
    // on error the unmap on the region will clean it up
    vmar_t* stack = nullptr;
    RETHROW(vmar_map(stack_region, &stack, PAGE_SIZE, page_count));
    defer OBJECT_PUT(stack);

    void* stack_bottom = (vmar_end(stack) + 1) - sizeof(uintptr_t) * 2;
    stack->role = VMAR_ROLE_STACK;
    strcpy(stack->name, "stack");

    // for the kernel stacks pre-populate the stack to make sure
    // faults won't do anything funny
    if (!user) {
        RETHROW(vmar_populate(stack));
    }

    // we are good
    allocation->stack_bottom = stack_bottom;
    allocation->stack_region = stack_region;

    return SUCCESS;
}
