/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

struct chx_rb_node {
    unsigned long __rb_parent_color;
    struct chx_rb_node* rb_right;
    struct chx_rb_node* rb_left;
} __attribute__((aligned(sizeof(long))));
/* The alignment might seem pointless, but allegedly CRIS needs it */

struct chx_rb_root {
    struct chx_rb_node* rb_node;
};

/*
 * Leftmost-cached rbtrees.
 *
 * We do not cache the rightmost node based on footprint
 * size vs number of potential users that could benefit
 * from O(1) rb_last(). Just not worth it, users that want
 * this feature can always implement the logic explicitly.
 * Furthermore, users that want to cache both pointers may
 * find it a bit asymmetric, but that's ok.
 */
struct chx_rb_root_cached {
    struct chx_rb_root rb_root;
    struct chx_rb_node* rb_leftmost;
};

#define CHX_RB_ROOT                                                            \
    (struct chx_rb_root) { NULL, }
#define CHX_RB_ROOT_CACHED                                                     \
    (struct chx_rb_root_cached) {                                              \
        {                                                                      \
            NULL,                                                              \
        },                                                                     \
            NULL                                                               \
    }
