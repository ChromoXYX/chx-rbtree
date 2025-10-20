// SPDX-License-Identifier: GPL-2.0-or-later
/*
  Red Black Trees
  (C) 1999  Andrea Arcangeli <andrea@suse.de>
  (C) 2002  David Woodhouse <dwmw2@infradead.org>
  (C) 2012  Michel Lespinasse <walken@google.com>


  linux/lib/rbtree.c
*/

#include "rbtree_augmented.h"
#include "rbtree.h"

/* Internal macros - not exposed to users */
#ifndef likely
#define likely(x) __builtin_expect(!!(x), 1)
#endif

#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

#ifndef READ_ONCE
#define READ_ONCE(x) (*(volatile typeof(x)*)&(x))
#endif

#ifndef WRITE_ONCE
#define WRITE_ONCE(x, val) (*(volatile typeof(x)*)&(x) = (val))
#endif

/* RCU dummy macros for userland */
#define rcu_assign_pointer(p, v) WRITE_ONCE(p, v)
#define rcu_dereference_raw(p) READ_ONCE(p)

#define chx_rb_parent(r) ((struct chx_rb_node*)((r)->__rb_parent_color & ~3))

#define CHX_RB_RED 0
#define CHX_RB_BLACK 1

#define __chx_rb_parent(pc) ((struct chx_rb_node*)(pc & ~3))

#define __chx_rb_color(pc) ((pc) & 1)
#define __chx_rb_is_black(pc) __chx_rb_color(pc)
#define __chx_rb_is_red(pc) (!__chx_rb_color(pc))
#define chx_rb_color(rb) __chx_rb_color((rb)->__rb_parent_color)
#define chx_rb_is_red(rb) __chx_rb_is_red((rb)->__rb_parent_color)
#define chx_rb_is_black(rb) __chx_rb_is_black((rb)->__rb_parent_color)

static inline void chx_rb_set_parent(struct chx_rb_node* rb,
                                     struct chx_rb_node* p) {
    rb->__rb_parent_color = chx_rb_color(rb) + (unsigned long)p;
}

static inline void chx_rb_set_parent_color(struct chx_rb_node* rb,
                                           struct chx_rb_node* p, int color) {
    rb->__rb_parent_color = (unsigned long)p + color;
}

static inline void __chx_rb_change_child(struct chx_rb_node* old,
                                         struct chx_rb_node* new_node,
                                         struct chx_rb_node* parent,
                                         struct chx_rb_root* root) {
    if (parent) {
        if (parent->rb_left == old)
            WRITE_ONCE(parent->rb_left, new_node);
        else
            WRITE_ONCE(parent->rb_right, new_node);
    } else
        WRITE_ONCE(root->rb_node, new_node);
}

static inline void __chx_rb_change_child_rcu(struct chx_rb_node* old,
                                             struct chx_rb_node* new_node,
                                             struct chx_rb_node* parent,
                                             struct chx_rb_root* root) {
    if (parent) {
        if (parent->rb_left == old)
            rcu_assign_pointer(parent->rb_left, new_node);
        else
            rcu_assign_pointer(parent->rb_right, new_node);
    } else
        rcu_assign_pointer(root->rb_node, new_node);
}

/*
 * red-black trees properties:  https://en.wikipedia.org/wiki/Rbtree
 *
 *  1) A node is either red or black
 *  2) The root is black
 *  3) All leaves (NULL) are black
 *  4) Both children of every red node are black
 *  5) Every simple path from root to leaves contains the same number
 *     of black nodes.
 *
 *  4 and 5 give the O(log n) guarantee, since 4 implies you cannot have two
 *  consecutive red nodes in a path and every red node is therefore followed by
 *  a black. So if B is the number of black nodes on every simple path (as per
 *  5), then the longest possible path due to 4 is 2B.
 *
 *  We shall indicate color with case, where black nodes are uppercase and red
 *  nodes will be lowercase. Unknown color nodes shall be drawn as red within
 *  parentheses and have some accompanying text comment.
 */

/*
 * Notes on lockless lookups:
 *
 * All stores to the tree structure (rb_left and rb_right) must be done using
 * WRITE_ONCE(). And we must not inadvertently cause (temporary) loops in the
 * tree structure as seen in program order.
 *
 * These two requirements will allow lockless iteration of the tree -- not
 * correct iteration mind you, tree rotations are not atomic so a lookup might
 * miss entire subtrees.
 *
 * But they do guarantee that any such traversal will only see valid elements
 * and that it will indeed complete -- does not get stuck in a loop.
 *
 * It also guarantees that if the lookup returns an element it is the 'correct'
 * one. But not returning an element does _NOT_ mean it's not present.
 *
 * NOTE:
 *
 * Stores to __rb_parent_color are not important for simple lookups so those
 * are left undone as of now. Nor did I check for loops involving parent
 * pointers.
 */

static inline void chx_rb_set_black(struct chx_rb_node* rb) {
    rb->__rb_parent_color += CHX_RB_BLACK;
}

static inline struct chx_rb_node* chx_rb_red_parent(struct chx_rb_node* red) {
    return (struct chx_rb_node*)red->__rb_parent_color;
}

/*
 * Helper function for rotations:
 * - old's parent and color get assigned to new
 * - old gets assigned new as a parent and 'color' as a color.
 */
static inline void __chx_rb_rotate_set_parents(struct chx_rb_node* old,
                                               struct chx_rb_node* new,
                                               struct chx_rb_root* root,
                                               int color) {
    struct chx_rb_node* parent = chx_rb_parent(old);
    new->__rb_parent_color = old->__rb_parent_color;
    chx_rb_set_parent_color(old, new, color);
    __chx_rb_change_child(old, new, parent, root);
}

static inline void
__chx_rb_insert(struct chx_rb_node* node, struct chx_rb_root* root,
                void (*augment_rotate)(struct chx_rb_node* old,
                                       struct chx_rb_node* new_node)) {
    struct chx_rb_node *parent = chx_rb_red_parent(node), *gparent, *tmp;

    while (true) {
        /*
         * Loop invariant: node is red.
         */
        if (unlikely(!parent)) {
            /*
             * The inserted node is root. Either this is the
             * first node, or we recursed at Case 1 below and
             * are no longer violating 4).
             */
            chx_rb_set_parent_color(node, NULL, CHX_RB_BLACK);
            break;
        }

        /*
         * If there is a black parent, we are done.
         * Otherwise, take some corrective action as,
         * per 4), we don't want a red root or two
         * consecutive red nodes.
         */
        if (chx_rb_is_black(parent))
            break;

        gparent = chx_rb_red_parent(parent);

        tmp = gparent->rb_right;
        if (parent != tmp) { /* parent == gparent->rb_left */
            if (tmp && chx_rb_is_red(tmp)) {
                /*
                 * Case 1 - node's uncle is red (color flips).
                 *
                 *       G            g
                 *      / \          / \
                 *     p   u  -->   P   U
                 *    /            /
                 *   n            n
                 *
                 * However, since g's parent might be red, and
                 * 4) does not allow this, we need to recurse
                 * at g.
                 */
                chx_rb_set_parent_color(tmp, gparent, CHX_RB_BLACK);
                chx_rb_set_parent_color(parent, gparent, CHX_RB_BLACK);
                node = gparent;
                parent = chx_rb_parent(node);
                chx_rb_set_parent_color(node, parent, CHX_RB_RED);
                continue;
            }

            tmp = parent->rb_right;
            if (node == tmp) {
                /*
                 * Case 2 - node's uncle is black and node is
                 * the parent's right child (left rotate at parent).
                 *
                 *      G             G
                 *     / \           / \
                 *    p   U  -->    n   U
                 *     \           /
                 *      n         p
                 *
                 * This still leaves us in violation of 4), the
                 * continuation into Case 3 will fix that.
                 */
                tmp = node->rb_left;
                WRITE_ONCE(parent->rb_right, tmp);
                WRITE_ONCE(node->rb_left, parent);
                if (tmp)
                    chx_rb_set_parent_color(tmp, parent, CHX_RB_BLACK);
                chx_rb_set_parent_color(parent, node, CHX_RB_RED);
                augment_rotate(parent, node);
                parent = node;
                tmp = node->rb_right;
            }

            /*
             * Case 3 - node's uncle is black and node is
             * the parent's left child (right rotate at gparent).
             *
             *        G           P
             *       / \         / \
             *      p   U  -->  n   g
             *     /                 \
             *    n                   U
             */
            WRITE_ONCE(gparent->rb_left, tmp); /* == parent->rb_right */
            WRITE_ONCE(parent->rb_right, gparent);
            if (tmp)
                chx_rb_set_parent_color(tmp, gparent, CHX_RB_BLACK);
            __chx_rb_rotate_set_parents(gparent, parent, root, CHX_RB_RED);
            augment_rotate(gparent, parent);
            break;
        } else {
            tmp = gparent->rb_left;
            if (tmp && chx_rb_is_red(tmp)) {
                /* Case 1 - color flips */
                chx_rb_set_parent_color(tmp, gparent, CHX_RB_BLACK);
                chx_rb_set_parent_color(parent, gparent, CHX_RB_BLACK);
                node = gparent;
                parent = chx_rb_parent(node);
                chx_rb_set_parent_color(node, parent, CHX_RB_RED);
                continue;
            }

            tmp = parent->rb_left;
            if (node == tmp) {
                /* Case 2 - right rotate at parent */
                tmp = node->rb_right;
                WRITE_ONCE(parent->rb_left, tmp);
                WRITE_ONCE(node->rb_right, parent);
                if (tmp)
                    chx_rb_set_parent_color(tmp, parent, CHX_RB_BLACK);
                chx_rb_set_parent_color(parent, node, CHX_RB_RED);
                augment_rotate(parent, node);
                parent = node;
                tmp = node->rb_left;
            }

            /* Case 3 - left rotate at gparent */
            WRITE_ONCE(gparent->rb_right, tmp); /* == parent->rb_left */
            WRITE_ONCE(parent->rb_left, gparent);
            if (tmp)
                chx_rb_set_parent_color(tmp, gparent, CHX_RB_BLACK);
            __chx_rb_rotate_set_parents(gparent, parent, root, CHX_RB_RED);
            augment_rotate(gparent, parent);
            break;
        }
    }
}

/*
 * Inline version for chx_rb_erase() use - we want to be able to inline
 * and eliminate the dummy_rotate callback there
 */
static inline void
____chx_rb_erase_color(struct chx_rb_node* parent, struct chx_rb_root* root,
                       void (*augment_rotate)(struct chx_rb_node* old,
                                              struct chx_rb_node* new_node)) {
    struct chx_rb_node *node = NULL, *sibling, *tmp1, *tmp2;

    while (true) {
        /*
         * Loop invariants:
         * - node is black (or NULL on first iteration)
         * - node is not the root (parent is not NULL)
         * - All leaf paths going through parent and node have a
         *   black node count that is 1 lower than other leaf paths.
         */
        sibling = parent->rb_right;
        if (node != sibling) { /* node == parent->rb_left */
            if (chx_rb_is_red(sibling)) {
                /*
                 * Case 1 - left rotate at parent
                 *
                 *     P               S
                 *    / \             / \
                 *   N   s    -->    p   Sr
                 *      / \         / \
                 *     Sl  Sr      N   Sl
                 */
                tmp1 = sibling->rb_left;
                WRITE_ONCE(parent->rb_right, tmp1);
                WRITE_ONCE(sibling->rb_left, parent);
                chx_rb_set_parent_color(tmp1, parent, CHX_RB_BLACK);
                __chx_rb_rotate_set_parents(parent, sibling, root, CHX_RB_RED);
                augment_rotate(parent, sibling);
                sibling = tmp1;
            }
            tmp1 = sibling->rb_right;
            if (!tmp1 || chx_rb_is_black(tmp1)) {
                tmp2 = sibling->rb_left;
                if (!tmp2 || chx_rb_is_black(tmp2)) {
                    /*
                     * Case 2 - sibling color flip
                     * (p could be either color here)
                     *
                     *    (p)           (p)
                     *    / \           / \
                     *   N   S    -->  N   s
                     *      / \           / \
                     *     Sl  Sr        Sl  Sr
                     *
                     * This leaves us violating 5) which
                     * can be fixed by flipping p to black
                     * if it was red, or by recursing at p.
                     * p is red when coming from Case 1.
                     */
                    chx_rb_set_parent_color(sibling, parent, CHX_RB_RED);
                    if (chx_rb_is_red(parent))
                        chx_rb_set_black(parent);
                    else {
                        node = parent;
                        parent = chx_rb_parent(node);
                        if (parent)
                            continue;
                    }
                    break;
                }
                /*
                 * Case 3 - right rotate at sibling
                 * (p could be either color here)
                 *
                 *   (p)           (p)
                 *   / \           / \
                 *  N   S    -->  N   sl
                 *     / \             \
                 *    sl  Sr            S
                 *                       \
                 *                        Sr
                 *
                 * Note: p might be red, and then both
                 * p and sl are red after rotation(which
                 * breaks property 4). This is fixed in
                 * Case 4 (in __chx_rb_rotate_set_parents()
                 *         which set sl the color of p
                 *         and set p CHX_RB_BLACK)
                 *
                 *   (p)            (sl)
                 *   / \            /  \
                 *  N   sl   -->   P    S
                 *       \        /      \
                 *        S      N        Sr
                 *         \
                 *          Sr
                 */
                tmp1 = tmp2->rb_right;
                WRITE_ONCE(sibling->rb_left, tmp1);
                WRITE_ONCE(tmp2->rb_right, sibling);
                WRITE_ONCE(parent->rb_right, tmp2);
                if (tmp1)
                    chx_rb_set_parent_color(tmp1, sibling, CHX_RB_BLACK);
                augment_rotate(sibling, tmp2);
                tmp1 = sibling;
                sibling = tmp2;
            }
            /*
             * Case 4 - left rotate at parent + color flips
             * (p and sl could be either color here.
             *  After rotation, p becomes black, s acquires
             *  p's color, and sl keeps its color)
             *
             *      (p)             (s)
             *      / \             / \
             *     N   S     -->   P   Sr
             *        / \         / \
             *      (sl) sr      N  (sl)
             */
            tmp2 = sibling->rb_left;
            WRITE_ONCE(parent->rb_right, tmp2);
            WRITE_ONCE(sibling->rb_left, parent);
            chx_rb_set_parent_color(tmp1, sibling, CHX_RB_BLACK);
            if (tmp2)
                chx_rb_set_parent(tmp2, parent);
            __chx_rb_rotate_set_parents(parent, sibling, root, CHX_RB_BLACK);
            augment_rotate(parent, sibling);
            break;
        } else {
            sibling = parent->rb_left;
            if (chx_rb_is_red(sibling)) {
                /* Case 1 - right rotate at parent */
                tmp1 = sibling->rb_right;
                WRITE_ONCE(parent->rb_left, tmp1);
                WRITE_ONCE(sibling->rb_right, parent);
                chx_rb_set_parent_color(tmp1, parent, CHX_RB_BLACK);
                __chx_rb_rotate_set_parents(parent, sibling, root, CHX_RB_RED);
                augment_rotate(parent, sibling);
                sibling = tmp1;
            }
            tmp1 = sibling->rb_left;
            if (!tmp1 || chx_rb_is_black(tmp1)) {
                tmp2 = sibling->rb_right;
                if (!tmp2 || chx_rb_is_black(tmp2)) {
                    /* Case 2 - sibling color flip */
                    chx_rb_set_parent_color(sibling, parent, CHX_RB_RED);
                    if (chx_rb_is_red(parent))
                        chx_rb_set_black(parent);
                    else {
                        node = parent;
                        parent = chx_rb_parent(node);
                        if (parent)
                            continue;
                    }
                    break;
                }
                /* Case 3 - left rotate at sibling */
                tmp1 = tmp2->rb_left;
                WRITE_ONCE(sibling->rb_right, tmp1);
                WRITE_ONCE(tmp2->rb_left, sibling);
                WRITE_ONCE(parent->rb_left, tmp2);
                if (tmp1)
                    chx_rb_set_parent_color(tmp1, sibling, CHX_RB_BLACK);
                augment_rotate(sibling, tmp2);
                tmp1 = sibling;
                sibling = tmp2;
            }
            /* Case 4 - right rotate at parent + color flips */
            tmp2 = sibling->rb_right;
            WRITE_ONCE(parent->rb_left, tmp2);
            WRITE_ONCE(sibling->rb_right, parent);
            chx_rb_set_parent_color(tmp1, sibling, CHX_RB_BLACK);
            if (tmp2)
                chx_rb_set_parent(tmp2, parent);
            __chx_rb_rotate_set_parents(parent, sibling, root, CHX_RB_BLACK);
            augment_rotate(parent, sibling);
            break;
        }
    }
}

/* Non-inline version for rb_erase_augmented() use */
void __chx_rb_erase_color(
    struct chx_rb_node* parent, struct chx_rb_root* root,
    void (*augment_rotate)(struct chx_rb_node* old,
                           struct chx_rb_node* new_node)) {
    ____chx_rb_erase_color(parent, root, augment_rotate);
}

/*
 * Non-augmented rbtree manipulation functions.
 *
 * We use dummy augmented callbacks here, and have the compiler optimize them
 * out of the chx_rb_insert_color() and chx_rb_erase() function definitions.
 */

static inline void dummy_propagate(struct chx_rb_node* node
                                   __attribute__((unused)),
                                   struct chx_rb_node* stop
                                   __attribute__((unused))) {}
static inline void dummy_copy(struct chx_rb_node* old __attribute__((unused)),
                              struct chx_rb_node* new_node
                              __attribute__((unused))) {}
static inline void dummy_rotate(struct chx_rb_node* old __attribute__((unused)),
                                struct chx_rb_node* new_node
                                __attribute__((unused))) {}

static const struct chx_rb_augment_callbacks dummy_callbacks = {
    .propagate = dummy_propagate, .copy = dummy_copy, .rotate = dummy_rotate};

void chx_rb_insert_color(struct chx_rb_node* node, struct chx_rb_root* root) {
    __chx_rb_insert(node, root, dummy_rotate);
}

void chx_rb_erase(struct chx_rb_node* node, struct chx_rb_root* root) {
    struct chx_rb_node* rebalance;
    rebalance = __chx_rb_erase_augmented(node, root, &dummy_callbacks);
    if (rebalance)
        ____chx_rb_erase_color(rebalance, root, dummy_rotate);
}

/*
 * Augmented rbtree manipulation functions.
 *
 * This instantiates the same __always_inline functions as in the non-augmented
 * case, but this time with user-defined callbacks.
 */

void __chx_rb_insert_augmented(
    struct chx_rb_node* node, struct chx_rb_root* root,
    void (*augment_rotate)(struct chx_rb_node* old,
                           struct chx_rb_node* new_node)) {
    __chx_rb_insert(node, root, augment_rotate);
}

/*
 * This function returns the first node (in sort order) of the tree.
 */
struct chx_rb_node* chx_rb_first(const struct chx_rb_root* root) {
    struct chx_rb_node* n;

    n = root->rb_node;
    if (!n)
        return NULL;
    while (n->rb_left)
        n = n->rb_left;
    return n;
}

struct chx_rb_node* chx_rb_last(const struct chx_rb_root* root) {
    struct chx_rb_node* n;

    n = root->rb_node;
    if (!n)
        return NULL;
    while (n->rb_right)
        n = n->rb_right;
    return n;
}

struct chx_rb_node* chx_rb_next(const struct chx_rb_node* node) {
    struct chx_rb_node* parent;

    if (CHX_RB_EMPTY_NODE(node))
        return NULL;

    /*
     * If we have a right-hand child, go down and then left as far
     * as we can.
     */
    if (node->rb_right) {
        node = node->rb_right;
        while (node->rb_left)
            node = node->rb_left;
        return (struct chx_rb_node*)node;
    }

    /*
     * No right-hand children. Everything down and left is smaller than us,
     * so any 'next' node must be in the general direction of our parent.
     * Go up the tree; any time the ancestor is a right-hand child of its
     * parent, keep going up. First time it's a left-hand child of its
     * parent, said parent is our 'next' node.
     */
    while ((parent = chx_rb_parent(node)) && node == parent->rb_right)
        node = parent;

    return parent;
}

struct chx_rb_node* chx_rb_prev(const struct chx_rb_node* node) {
    struct chx_rb_node* parent;

    if (CHX_RB_EMPTY_NODE(node))
        return NULL;

    /*
     * If we have a left-hand child, go down and then right as far
     * as we can.
     */
    if (node->rb_left) {
        node = node->rb_left;
        while (node->rb_right)
            node = node->rb_right;
        return (struct chx_rb_node*)node;
    }

    /*
     * No left-hand children. Go up till we find an ancestor which
     * is a right-hand child of its parent.
     */
    while ((parent = chx_rb_parent(node)) && node == parent->rb_left)
        node = parent;

    return parent;
}

void chx_rb_replace_node(struct chx_rb_node* victim,
                         struct chx_rb_node* new_node,
                         struct chx_rb_root* root) {
    struct chx_rb_node* parent = chx_rb_parent(victim);

    /* Copy the pointers/colour from the victim to the replacement */
    *new_node = *victim;

    /* Set the surrounding nodes to point to the replacement */
    if (victim->rb_left)
        chx_rb_set_parent(victim->rb_left, new_node);
    if (victim->rb_right)
        chx_rb_set_parent(victim->rb_right, new_node);
    __chx_rb_change_child(victim, new_node, parent, root);
}

void chx_rb_replace_node_rcu(struct chx_rb_node* victim,
                             struct chx_rb_node* new_node,
                             struct chx_rb_root* root) {
    struct chx_rb_node* parent = chx_rb_parent(victim);

    /* Copy the pointers/colour from the victim to the replacement */
    *new_node = *victim;

    /* Set the surrounding nodes to point to the replacement */
    if (victim->rb_left)
        chx_rb_set_parent(victim->rb_left, new_node);
    if (victim->rb_right)
        chx_rb_set_parent(victim->rb_right, new_node);

    /* Set the parent's pointer to the new node last after an RCU barrier
     * so that the pointers onwards are seen to be set correctly when doing
     * an RCU walk over the tree.
     */
    __chx_rb_change_child_rcu(victim, new_node, parent, root);
}

static struct chx_rb_node*
chx_rb_left_deepest_node(const struct chx_rb_node* node) {
    for (;;) {
        if (node->rb_left)
            node = node->rb_left;
        else if (node->rb_right)
            node = node->rb_right;
        else
            return (struct chx_rb_node*)node;
    }
}

struct chx_rb_node* chx_rb_next_postorder(const struct chx_rb_node* node) {
    const struct chx_rb_node* parent;
    if (!node)
        return NULL;
    parent = chx_rb_parent(node);

    /* If we're sitting on node, we've already seen our children */
    if (parent && node == parent->rb_left && parent->rb_right) {
        /* If we are the parent's left node, go to the parent's right
         * node then all the way down to the left */
        return chx_rb_left_deepest_node(parent->rb_right);
    } else
        /* Otherwise we are the parent's right node, and the parent
         * should be next */
        return (struct chx_rb_node*)parent;
}

struct chx_rb_node* chx_rb_first_postorder(const struct chx_rb_root* root) {
    if (!root->rb_node)
        return NULL;

    return chx_rb_left_deepest_node(root->rb_node);
}

struct chx_rb_node*
__chx_rb_erase_augmented(struct chx_rb_node* node, struct chx_rb_root* root,
                         const struct chx_rb_augment_callbacks* augment) {
    struct chx_rb_node* child = node->rb_right;
    struct chx_rb_node* tmp = node->rb_left;
    struct chx_rb_node *parent, *rebalance;
    unsigned long pc;

    if (!tmp) {
        /*
         * Case 1: node to erase has no more than 1 child (easy!)
         *
         * Note that if there is one child it must be red due to 5)
         * and node must be black due to 4). We adjust colors locally
         * so as to bypass __chx_rb_erase_color() later on.
         */
        pc = node->__rb_parent_color;
        parent = __chx_rb_parent(pc);
        __chx_rb_change_child(node, child, parent, root);
        if (child) {
            child->__rb_parent_color = pc;
            rebalance = NULL;
        } else
            rebalance = __chx_rb_is_black(pc) ? parent : NULL;
        tmp = parent;
    } else if (!child) {
        /* Still case 1, but this time the child is node->rb_left */
        tmp->__rb_parent_color = pc = node->__rb_parent_color;
        parent = __chx_rb_parent(pc);
        __chx_rb_change_child(node, tmp, parent, root);
        rebalance = NULL;
        tmp = parent;
    } else {
        struct chx_rb_node *successor = child, *child2;

        tmp = child->rb_left;
        if (!tmp) {
            /*
             * Case 2: node's successor is its right child
             *
             *    (n)          (s)
             *    / \          / \
             *  (x) (s)  ->  (x) (c)
             *        \
             *        (c)
             */
            parent = successor;
            child2 = successor->rb_right;

            augment->copy(node, successor);
        } else {
            /*
             * Case 3: node's successor is leftmost under
             * node's right child subtree
             *
             *    (n)          (s)
             *    / \          / \
             *  (x) (y)  ->  (x) (y)
             *      /            /
             *    (p)          (p)
             *    /            /
             *  (s)          (c)
             *    \
             *    (c)
             */
            do {
                parent = successor;
                successor = tmp;
                tmp = tmp->rb_left;
            } while (tmp);
            child2 = successor->rb_right;
            WRITE_ONCE(parent->rb_left, child2);
            WRITE_ONCE(successor->rb_right, child);
            chx_rb_set_parent(child, successor);

            augment->copy(node, successor);
            augment->propagate(parent, successor);
        }

        tmp = node->rb_left;
        WRITE_ONCE(successor->rb_left, tmp);
        chx_rb_set_parent(tmp, successor);

        pc = node->__rb_parent_color;
        tmp = __chx_rb_parent(pc);
        __chx_rb_change_child(node, successor, tmp, root);

        if (child2) {
            chx_rb_set_parent_color(child2, parent, CHX_RB_BLACK);
            rebalance = NULL;
        } else {
            rebalance = chx_rb_is_black(successor) ? parent : NULL;
        }
        successor->__rb_parent_color = pc;
        tmp = successor;
    }

    augment->propagate(tmp, NULL);
    return rebalance;
}
