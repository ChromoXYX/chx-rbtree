/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
  Red Black Trees
  (C) 1999  Andrea Arcangeli <andrea@suse.de>
  (C) 2002  David Woodhouse <dwmw2@infradead.org>
  (C) 2012  Michel Lespinasse <walken@google.com>


  linux/include/linux/rbtree_augmented.h
*/

#pragma once

#include "rbtree.h"

/*
 * Please note - only struct chx_rb_augment_callbacks and the prototypes for
 * chx_rb_insert_augmented() and chx_rb_erase_augmented() are intended to be
 * public. The rest are implementation details you are not expected to depend
 * on.
 *
 * See Documentation/core-api/rbtree.rst for documentation and samples.
 */

struct chx_rb_augment_callbacks {
    void (*propagate)(struct chx_rb_node* node, struct chx_rb_node* stop);
    void (*copy)(struct chx_rb_node* old, struct chx_rb_node* new_node);
    void (*rotate)(struct chx_rb_node* old, struct chx_rb_node* new_node);
};

extern void
__chx_rb_insert_augmented(struct chx_rb_node* node, struct chx_rb_root* root,
                          void (*augment_rotate)(struct chx_rb_node* old,
                                                 struct chx_rb_node* new_node));

/*
 * Fixup the rbtree and update the augmented information when rebalancing.
 *
 * On insertion, the user must update the augmented information on the path
 * leading to the inserted node, then call chx_rb_link_node() as usual and
 * chx_rb_insert_augmented() instead of the usual chx_rb_insert_color() call.
 * If chx_rb_insert_augmented() rebalances the rbtree, it will callback into
 * a user provided function to update the augmented information on the
 * affected subtrees.
 */
static inline void
chx_rb_insert_augmented(struct chx_rb_node* node, struct chx_rb_root* root,
                        const struct chx_rb_augment_callbacks* augment) {
    __chx_rb_insert_augmented(node, root, augment->rotate);
}

static inline void
chx_rb_insert_augmented_cached(struct chx_rb_node* node,
                               struct chx_rb_root_cached* root, bool newleft,
                               const struct chx_rb_augment_callbacks* augment) {
    if (newleft)
        root->rb_leftmost = node;
    chx_rb_insert_augmented(node, &root->rb_root, augment);
}

static inline struct chx_rb_node* chx_rb_add_augmented_cached(
    struct chx_rb_node* node, struct chx_rb_root_cached* tree,
    bool (*less)(struct chx_rb_node*, const struct chx_rb_node*),
    const struct chx_rb_augment_callbacks* augment) {
    struct chx_rb_node** link = &tree->rb_root.rb_node;
    struct chx_rb_node* parent = NULL;
    bool leftmost = true;

    while (*link) {
        parent = *link;
        if (less(node, parent)) {
            link = &parent->rb_left;
        } else {
            link = &parent->rb_right;
            leftmost = false;
        }
    }

    chx_rb_link_node(node, parent, link);
    augment->propagate(parent, NULL); /* suboptimal */
    chx_rb_insert_augmented_cached(node, tree, leftmost, augment);

    return leftmost ? node : NULL;
}

/*
 * Template for declaring augmented rbtree callbacks (generic case)
 *
 * RBSTATIC:    'static' or empty
 * RBNAME:      name of the chx_rb_augment_callbacks structure
 * RBSTRUCT:    struct type of the tree nodes
 * RBFIELD:     name of struct chx_rb_node field within RBSTRUCT
 * RBAUGMENTED: name of field within RBSTRUCT holding data for subtree
 * RBCOMPUTE:   name of function that recomputes the RBAUGMENTED data
 */

#define CHX_RB_DECLARE_CALLBACKS(RBSTATIC, RBNAME, RBSTRUCT, RBFIELD,          \
                                 RBAUGMENTED, RBCOMPUTE)                       \
    static inline void RBNAME##_propagate(struct chx_rb_node* rb,              \
                                          struct chx_rb_node* stop) {          \
        while (rb != stop) {                                                   \
            RBSTRUCT* node = chx_rb_entry(rb, RBSTRUCT, RBFIELD);              \
            if (RBCOMPUTE(node, true))                                         \
                break;                                                         \
            rb = chx_rb_parent(&node->RBFIELD);                                \
        }                                                                      \
    }                                                                          \
    static inline void RBNAME##_copy(struct chx_rb_node* rb_old,               \
                                     struct chx_rb_node* rb_new) {             \
        RBSTRUCT* old = chx_rb_entry(rb_old, RBSTRUCT, RBFIELD);               \
        RBSTRUCT* new = chx_rb_entry(rb_new, RBSTRUCT, RBFIELD);               \
        new->RBAUGMENTED = old->RBAUGMENTED;                                   \
    }                                                                          \
    static void RBNAME##_rotate(struct chx_rb_node* rb_old,                    \
                                struct chx_rb_node* rb_new) {                  \
        RBSTRUCT* old = chx_rb_entry(rb_old, RBSTRUCT, RBFIELD);               \
        RBSTRUCT* new = chx_rb_entry(rb_new, RBSTRUCT, RBFIELD);               \
        new->RBAUGMENTED = old->RBAUGMENTED;                                   \
        RBCOMPUTE(old, false);                                                 \
    }                                                                          \
    RBSTATIC const struct chx_rb_augment_callbacks RBNAME = {                  \
        .propagate = RBNAME##_propagate,                                       \
        .copy = RBNAME##_copy,                                                 \
        .rotate = RBNAME##_rotate};

/*
 * Template for declaring augmented rbtree callbacks,
 * computing RBAUGMENTED scalar as max(RBCOMPUTE(node)) for all subtree nodes.
 *
 * RBSTATIC:    'static' or empty
 * RBNAME:      name of the chx_rb_augment_callbacks structure
 * RBSTRUCT:    struct type of the tree nodes
 * RBFIELD:     name of struct chx_rb_node field within RBSTRUCT
 * RBTYPE:      type of the RBAUGMENTED field
 * RBAUGMENTED: name of RBTYPE field within RBSTRUCT holding data for subtree
 * RBCOMPUTE:   name of function that returns the per-node RBTYPE scalar
 */

#define CHX_RB_DECLARE_CALLBACKS_MAX(RBSTATIC, RBNAME, RBSTRUCT, RBFIELD,      \
                                     RBTYPE, RBAUGMENTED, RBCOMPUTE)           \
    static inline bool RBNAME##_compute_max(RBSTRUCT* node, bool exit) {       \
        RBSTRUCT* child;                                                       \
        RBTYPE max = RBCOMPUTE(node);                                          \
        if (node->RBFIELD.rb_left) {                                           \
            child = chx_rb_entry(node->RBFIELD.rb_left, RBSTRUCT, RBFIELD);    \
            if (child->RBAUGMENTED > max)                                      \
                max = child->RBAUGMENTED;                                      \
        }                                                                      \
        if (node->RBFIELD.rb_right) {                                          \
            child = chx_rb_entry(node->RBFIELD.rb_right, RBSTRUCT, RBFIELD);   \
            if (child->RBAUGMENTED > max)                                      \
                max = child->RBAUGMENTED;                                      \
        }                                                                      \
        if (exit && node->RBAUGMENTED == max)                                  \
            return true;                                                       \
        node->RBAUGMENTED = max;                                               \
        return false;                                                          \
    }                                                                          \
    CHX_RB_DECLARE_CALLBACKS(RBSTATIC, RBNAME, RBSTRUCT, RBFIELD, RBAUGMENTED, \
                             RBNAME##_compute_max)

extern void
__chx_rb_erase_color(struct chx_rb_node* parent, struct chx_rb_root* root,
                     void (*augment_rotate)(struct chx_rb_node* old,
                                            struct chx_rb_node* new_node));

extern struct chx_rb_node*
__chx_rb_erase_augmented(struct chx_rb_node* node, struct chx_rb_root* root,
                         const struct chx_rb_augment_callbacks* augment);

static inline void
chx_rb_erase_augmented(struct chx_rb_node* node, struct chx_rb_root* root,
                       const struct chx_rb_augment_callbacks* augment) {
    struct chx_rb_node* rebalance =
        __chx_rb_erase_augmented(node, root, augment);
    if (rebalance)
        __chx_rb_erase_color(rebalance, root, augment->rotate);
}

static inline void
chx_rb_erase_augmented_cached(struct chx_rb_node* node,
                              struct chx_rb_root_cached* root,
                              const struct chx_rb_augment_callbacks* augment) {
    if (root->rb_leftmost == node)
        root->rb_leftmost = chx_rb_next(node);
    chx_rb_erase_augmented(node, &root->rb_root, augment);
}
