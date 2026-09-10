#include "vmar.h"
#include "arch/virt.h"
#include "lib/assert.h"
#include "lib/container_of.h"
#include "lib/list.h"
#include "lib/rbtree/rbtree.h"
#include "lib/rbtree/rbtree_types.h"
#include "lib/string.h"
#include "lib/trace.h"
#include "tomato/err.h"
#include "util/except.h"
#include <stddef.h>
#include <stdint.h>

err_t vmar_link(vmar_t* parent, vmar_t* child) {
    err_t err = SUCCESS;

    CHECK(parent->kind == VMAR_KIND_REGION, INVALID_ARGUMENT);
    CHECK(child->parent == nullptr, INVALID_ARGUMENT);
    CHECK(child->base != nullptr, INVALID_ARGUMENT);
    CHECK(child->page_count != 0, INVALID_ARGUMENT);

    // get the start and end
    uintptr_t start = (uintptr_t)child->base;
    uintptr_t end;
    CHECK(!__builtin_add_overflow(start, (child->page_count << PAGE_SHIFT) - 1, &end),
          OUT_OF_RANGE);

    // must be fully contained within the parent
    CHECK(start >= (uintptr_t)parent->base, OUT_OF_RANGE);
    CHECK(end <= (uintptr_t)vmar_end(parent), OUT_OF_RANGE);

    // find where to link the child
    rb_node_t** link = &parent->region.children_tree.rb_node;
    rb_node_t* rb_parent = nullptr;
    vmar_t* prev = nullptr;
    while (*link != nullptr) {
        rb_parent = *link;
        vmar_t* sibling = rb_entry(rb_parent, vmar_t, node);

        if (end < (uintptr_t)sibling->base) {
            link = &rb_parent->rb_left;

        } else if (start > (uintptr_t)vmar_end(sibling)) {
            prev = sibling;
            link = &rb_parent->rb_right;

        } else {
            // overlaps with an existing child
            CHECK_FAIL(OVERLAPS);
        }
    }

    // insert into the tree
    rb_link_node(&child->node, rb_parent, link);
    rb_insert_color(&child->node, &parent->region.children_tree);

    // insert into the list right after the predecessor, or at the
    // head if there is none, keeping the list ordered by address
    list_add(prev != nullptr ? &prev->entry : &parent->region.children_list, &child->entry);

    child->parent = parent;

cleanup:
    return err;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Dumping
////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * The glyphs used to draw the tree, a child is drawn after a branch
 * and its descendants are indented by the matching indent so the guide
 * line continues down only while there are more siblings to come.
 */
#define DUMP_BRANCH      "├── "
#define DUMP_BRANCH_LAST "└── "
#define DUMP_INDENT      "│   "
#define DUMP_INDENT_LAST "    "

/**
 * The deepest nesting we draw guide lines for, anything past that
 * is printed at the same indentation as its parent instead of
 * overflowing the prefix buffer.
 */
#define DUMP_MAX_DEPTH   32
#define DUMP_PREFIX_SIZE (DUMP_MAX_DEPTH * (sizeof(DUMP_INDENT) - 1) + 1)

typedef struct dump_glyphs {
    const char* branch;
    const char* indent;
    size_t indent_len;
} dump_glyphs_t;

static const dump_glyphs_t m_dump_glyphs = {
    .branch = DUMP_BRANCH,
    .indent = DUMP_INDENT,
    .indent_len = sizeof(DUMP_INDENT) - 1,
};

static const dump_glyphs_t m_dump_glyphs_last = {
    .branch = DUMP_BRANCH_LAST,
    .indent = DUMP_INDENT_LAST,
    .indent_len = sizeof(DUMP_INDENT_LAST) - 1,
};

static const char* vmar_kind_str(vmar_kind_t kind) {
    switch (kind) {
        case VMAR_KIND_REGION: return "region";
        case VMAR_KIND_SPECIAL: return "special";
        default: return "unknown";
    }
}

static void vmar_dump_line(vmar_t* vmar, const char* prefix, const char* branch) {
    TRACE("%s%s%s: %016lx-%016lx [%s]", prefix, branch, vmar->name, (uintptr_t)vmar->base,
          (uintptr_t)vmar_end(vmar), vmar_kind_str(vmar->kind));
}

/**
 * Print all the children of the given vmar, the prefix is the already
 * drawn guide lines of the ancestors and is extended in place while
 * descending, so it must have room for DUMP_PREFIX_SIZE bytes.
 */
static void vmar_dump_children(vmar_t* vmar, char* prefix, size_t prefix_len) {
    // only regions have children
    if (vmar->kind != VMAR_KIND_REGION) {
        return;
    }

    // the list is kept ordered by address so this
    // prints the children in address order
    list_t* head = &vmar->region.children_list;
    for (list_entry_t* it = head->next; it != head; it = it->next) {
        vmar_t* child = container_of(it, vmar_t, entry);
        const dump_glyphs_t* glyphs = it->next == head ? &m_dump_glyphs_last : &m_dump_glyphs;

        vmar_dump_line(child, prefix, glyphs->branch);

        // extend the prefix for the grandchildren, if there is
        // no more room just print them flat under the child
        size_t child_len = prefix_len + glyphs->indent_len;
        if (child_len < DUMP_PREFIX_SIZE) {
            memcpy(prefix + prefix_len, glyphs->indent, glyphs->indent_len + 1);
            vmar_dump_children(child, prefix, child_len);
            prefix[prefix_len] = '\0';
        } else {
            vmar_dump_children(child, prefix, prefix_len);
        }
    }
}

void vmar_dump(vmar_t* root) {
    char prefix[DUMP_PREFIX_SIZE] = "";
    vmar_dump_line(root, prefix, "");
    vmar_dump_children(root, prefix, 0);
}
