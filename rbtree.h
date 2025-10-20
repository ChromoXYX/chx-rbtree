/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
  Red Black Trees
  (C) 1999  Andrea Arcangeli <andrea@suse.de>


  linux/include/linux/rbtree.h

  To use rbtrees you'll have to implement your own insert and search cores.
  This will avoid us to use callbacks and to drop drammatically performances.
  I know it's not the cleaner way,  but in C (not in C++) to get
  performances and genericity...

  See Documentation/core-api/rbtree.rst for documentation and samples.
*/

#pragma once

#include "rbtree_types.h"
#include <stddef.h>
#include <stdbool.h>

/* container_of macro */
#ifndef container_of
#define container_of(ptr, type, member)                                        \
    ({                                                                         \
        const typeof(((type*)0)->member)* __mptr = (ptr);                      \
        (type*)((char*)__mptr - offsetof(type, member));                       \
    })
#endif

#define chx_rb_entry(ptr, type, member) container_of(ptr, type, member)

#define CHX_RB_EMPTY_ROOT(root) ((root)->rb_node == NULL)

/* 'empty' nodes are nodes that are known not to be inserted in an rbtree */
#define CHX_RB_EMPTY_NODE(node)                                                \
    ((node)->__rb_parent_color == (unsigned long)(node))
#define CHX_RB_CLEAR_NODE(node)                                                \
    ((node)->__rb_parent_color = (unsigned long)(node))

extern void chx_rb_insert_color(struct chx_rb_node*, struct chx_rb_root*);
extern void chx_rb_erase(struct chx_rb_node*, struct chx_rb_root*);

/* Find logical next and previous nodes in a tree */
extern struct chx_rb_node* chx_rb_next(const struct chx_rb_node*);
extern struct chx_rb_node* chx_rb_prev(const struct chx_rb_node*);
extern struct chx_rb_node* chx_rb_first(const struct chx_rb_root*);
extern struct chx_rb_node* chx_rb_last(const struct chx_rb_root*);

/* Postorder iteration - always visit the parent after its children */
extern struct chx_rb_node* chx_rb_first_postorder(const struct chx_rb_root*);
extern struct chx_rb_node* chx_rb_next_postorder(const struct chx_rb_node*);

/* Fast replacement of a single node without remove/rebalance/add/rebalance */
extern void chx_rb_replace_node(struct chx_rb_node* victim,
                                struct chx_rb_node* new_node,
                                struct chx_rb_root* root);
extern void chx_rb_replace_node_rcu(struct chx_rb_node* victim,
                                    struct chx_rb_node* new_node,
                                    struct chx_rb_root* root);

static inline void chx_rb_link_node(struct chx_rb_node* node,
                                    struct chx_rb_node* parent,
                                    struct chx_rb_node** rb_link) {
    node->__rb_parent_color = (unsigned long)parent;
    node->rb_left = node->rb_right = NULL;

    *rb_link = node;
}

static inline void chx_rb_link_node_rcu(struct chx_rb_node* node,
                                        struct chx_rb_node* parent,
                                        struct chx_rb_node** rb_link) {
    node->__rb_parent_color = (unsigned long)parent;
    node->rb_left = node->rb_right = NULL;

    *rb_link = node;
}

#define chx_rb_entry_safe(ptr, type, member)                                   \
    ({                                                                         \
        typeof(ptr) ____ptr = (ptr);                                           \
        ____ptr ? chx_rb_entry(____ptr, type, member) : NULL;                  \
    })

/**
 * chx_rbtree_postorder_for_each_entry_safe - iterate in post-order over rb_root
 * of given type allowing the backing memory of @pos to be invalidated
 *
 * @pos:	the 'type *' to use as a loop cursor.
 * @n:		another 'type *' to use as temporary storage
 * @root:	'chx_rb_root *' of the rbtree.
 * @field:	the name of the chx_rb_node field within 'type'.
 *
 * chx_rbtree_postorder_for_each_entry_safe() provides a similar guarantee as
 * list_for_each_entry_safe() and allows the iteration to continue independent
 * of changes to @pos by the body of the loop.
 *
 * Note, however, that it cannot handle other modifications that re-order the
 * rbtree it is iterating over. This includes calling chx_rb_erase() on @pos, as
 * chx_rb_erase() may rebalance the tree, causing us to miss some nodes.
 */
#define chx_rbtree_postorder_for_each_entry_safe(pos, n, root, field)          \
    for (pos = chx_rb_entry_safe(chx_rb_first_postorder(root), typeof(*pos),   \
                                 field);                                       \
         pos && ({                                                             \
             n = chx_rb_entry_safe(chx_rb_next_postorder(&pos->field),         \
                                   typeof(*pos), field);                       \
             1;                                                                \
         });                                                                   \
         pos = n)

/* Same as chx_rb_first(), but O(1) */
#define chx_rb_first_cached(root) (root)->rb_leftmost

static inline void chx_rb_insert_color_cached(struct chx_rb_node* node,
                                              struct chx_rb_root_cached* root,
                                              bool leftmost) {
    if (leftmost)
        root->rb_leftmost = node;
    chx_rb_insert_color(node, &root->rb_root);
}

static inline struct chx_rb_node*
chx_rb_erase_cached(struct chx_rb_node* node, struct chx_rb_root_cached* root) {
    struct chx_rb_node* leftmost = NULL;

    if (root->rb_leftmost == node)
        leftmost = root->rb_leftmost = chx_rb_next(node);

    chx_rb_erase(node, &root->rb_root);

    return leftmost;
}

static inline void chx_rb_replace_node_cached(struct chx_rb_node* victim,
                                              struct chx_rb_node* new_node,
                                              struct chx_rb_root_cached* root) {
    if (root->rb_leftmost == victim)
        root->rb_leftmost = new_node;
    chx_rb_replace_node(victim, new_node, &root->rb_root);
}

/*
 * The below helper functions use 2 operators with 3 different
 * calling conventions. The operators are related like:
 *
 *	comp(a->key,b) < 0  := less(a,b)
 *	comp(a->key,b) > 0  := less(b,a)
 *	comp(a->key,b) == 0 := !less(a,b) && !less(b,a)
 *
 * If these operators define a partial order on the elements we make no
 * guarantee on which of the elements matching the key is found. See
 * chx_rb_find().
 *
 * The reason for this is to allow the find() interface without requiring an
 * on-stack dummy object, which might not be feasible due to object size.
 */

/**
 * chx_rb_add_cached() - insert @node into the leftmost cached tree @tree
 * @node: node to insert
 * @tree: leftmost cached tree to insert @node into
 * @less: operator defining the (partial) node order
 *
 * Returns @node when it is the new leftmost, or NULL.
 */
static inline struct chx_rb_node* chx_rb_add_cached(
    struct chx_rb_node* node, struct chx_rb_root_cached* tree,
    bool (*less)(struct chx_rb_node*, const struct chx_rb_node*)) {
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
    chx_rb_insert_color_cached(node, tree, leftmost);

    return leftmost ? node : NULL;
}

/**
 * chx_rb_add() - insert @node into @tree
 * @node: node to insert
 * @tree: tree to insert @node into
 * @less: operator defining the (partial) node order
 */
static inline void
chx_rb_add(struct chx_rb_node* node, struct chx_rb_root* tree,
           bool (*less)(struct chx_rb_node*, const struct chx_rb_node*)) {
    struct chx_rb_node** link = &tree->rb_node;
    struct chx_rb_node* parent = NULL;

    while (*link) {
        parent = *link;
        if (less(node, parent))
            link = &parent->rb_left;
        else
            link = &parent->rb_right;
    }

    chx_rb_link_node(node, parent, link);
    chx_rb_insert_color(node, tree);
}

/**
 * chx_rb_find_add_cached() - find equivalent @node in @tree, or add @node
 * @node: node to look-for / insert
 * @tree: tree to search / modify
 * @cmp: operator defining the node order
 *
 * Returns the chx_rb_node matching @node, or NULL when no match is found and
 * @node is inserted.
 */
static inline struct chx_rb_node* chx_rb_find_add_cached(
    struct chx_rb_node* node, struct chx_rb_root_cached* tree,
    int (*cmp)(const struct chx_rb_node* a, const struct chx_rb_node* b)) {
    bool leftmost = true;
    struct chx_rb_node** link = &tree->rb_root.rb_node;
    struct chx_rb_node* parent = NULL;
    int c;

    while (*link) {
        parent = *link;
        c = cmp(node, parent);

        if (c < 0) {
            link = &parent->rb_left;
        } else if (c > 0) {
            link = &parent->rb_right;
            leftmost = false;
        } else {
            return parent;
        }
    }

    chx_rb_link_node(node, parent, link);
    chx_rb_insert_color_cached(node, tree, leftmost);
    return NULL;
}

/**
 * chx_rb_find_add() - find equivalent @node in @tree, or add @node
 * @node: node to look-for / insert
 * @tree: tree to search / modify
 * @cmp: operator defining the node order
 *
 * Returns the chx_rb_node matching @node, or NULL when no match is found and
 * @node is inserted.
 */
static inline struct chx_rb_node*
chx_rb_find_add(struct chx_rb_node* node, struct chx_rb_root* tree,
                int (*cmp)(struct chx_rb_node*, const struct chx_rb_node*)) {
    struct chx_rb_node** link = &tree->rb_node;
    struct chx_rb_node* parent = NULL;
    int c;

    while (*link) {
        parent = *link;
        c = cmp(node, parent);

        if (c < 0)
            link = &parent->rb_left;
        else if (c > 0)
            link = &parent->rb_right;
        else
            return parent;
    }

    chx_rb_link_node(node, parent, link);
    chx_rb_insert_color(node, tree);
    return NULL;
}

/**
 * chx_rb_find_add_rcu() - find equivalent @node in @tree, or add @node
 * @node: node to look-for / insert
 * @tree: tree to search / modify
 * @cmp: operator defining the node order
 *
 * Adds a Store-Release for link_node.
 *
 * Returns the chx_rb_node matching @node, or NULL when no match is found and
 * @node is inserted.
 */
static inline struct chx_rb_node* chx_rb_find_add_rcu(
    struct chx_rb_node* node, struct chx_rb_root* tree,
    int (*cmp)(struct chx_rb_node*, const struct chx_rb_node*)) {
    struct chx_rb_node** link = &tree->rb_node;
    struct chx_rb_node* parent = NULL;
    int c;

    while (*link) {
        parent = *link;
        c = cmp(node, parent);

        if (c < 0)
            link = &parent->rb_left;
        else if (c > 0)
            link = &parent->rb_right;
        else
            return parent;
    }

    chx_rb_link_node_rcu(node, parent, link);
    chx_rb_insert_color(node, tree);
    return NULL;
}

/**
 * chx_rb_find() - find @key in tree @tree
 * @key: key to match
 * @tree: tree to search
 * @cmp: operator defining the node order
 *
 * Returns the chx_rb_node matching @key or NULL.
 */
static inline struct chx_rb_node*
chx_rb_find(const void* key, const struct chx_rb_root* tree,
            int (*cmp)(const void* key, const struct chx_rb_node*)) {
    struct chx_rb_node* node = tree->rb_node;

    while (node) {
        int c = cmp(key, node);

        if (c < 0)
            node = node->rb_left;
        else if (c > 0)
            node = node->rb_right;
        else
            return node;
    }

    return NULL;
}

/**
 * chx_rb_find_rcu() - find @key in tree @tree
 * @key: key to match
 * @tree: tree to search
 * @cmp: operator defining the node order
 *
 * Notably, tree descent vs concurrent tree rotations is unsound and can result
 * in false-negatives.
 *
 * Returns the chx_rb_node matching @key or NULL.
 */
static inline struct chx_rb_node*
chx_rb_find_rcu(const void* key, const struct chx_rb_root* tree,
                int (*cmp)(const void* key, const struct chx_rb_node*)) {
    struct chx_rb_node* node = tree->rb_node;

    while (node) {
        int c = cmp(key, node);

        if (c < 0)
            node = node->rb_left;
        else if (c > 0)
            node = node->rb_right;
        else
            return node;
    }

    return NULL;
}

/**
 * chx_rb_find_first() - find the first @key in @tree
 * @key: key to match
 * @tree: tree to search
 * @cmp: operator defining node order
 *
 * Returns the leftmost node matching @key, or NULL.
 */
static inline struct chx_rb_node*
chx_rb_find_first(const void* key, const struct chx_rb_root* tree,
                  int (*cmp)(const void* key, const struct chx_rb_node*)) {
    struct chx_rb_node* node = tree->rb_node;
    struct chx_rb_node* match = NULL;

    while (node) {
        int c = cmp(key, node);

        if (c <= 0) {
            if (!c)
                match = node;
            node = node->rb_left;
        } else if (c > 0) {
            node = node->rb_right;
        }
    }

    return match;
}

/**
 * chx_rb_next_match() - find the next @key in @tree
 * @key: key to match
 * @tree: tree to search
 * @cmp: operator defining node order
 *
 * Returns the next node matching @key, or NULL.
 */
static inline struct chx_rb_node*
chx_rb_next_match(const void* key, struct chx_rb_node* node,
                  int (*cmp)(const void* key, const struct chx_rb_node*)) {
    node = chx_rb_next(node);
    if (node && cmp(key, node))
        node = NULL;
    return node;
}

/**
 * chx_rb_for_each() - iterates a subtree matching @key
 * @node: iterator
 * @key: key to match
 * @tree: tree to search
 * @cmp: operator defining node order
 */
#define chx_rb_for_each(node, key, tree, cmp)                                  \
    for ((node) = chx_rb_find_first((key), (tree), (cmp)); (node);             \
         (node) = chx_rb_next_match((key), (node), (cmp)))
