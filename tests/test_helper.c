#include "test_helper.h"

/* 比较函数 */
bool less_func(struct chx_rb_node* a, const struct chx_rb_node* b) {
    struct test_node* node_a = chx_rb_entry(a, struct test_node, rb);
    struct test_node* node_b = chx_rb_entry(b, struct test_node, rb);
    return node_a->key < node_b->key;
}

int cmp_func(struct chx_rb_node* a, const struct chx_rb_node* b) {
    struct test_node* node_a = chx_rb_entry(a, struct test_node, rb);
    struct test_node* node_b = chx_rb_entry(b, struct test_node, rb);
    if (node_a->key < node_b->key)
        return -1;
    else if (node_a->key > node_b->key)
        return 1;
    return 0;
}

int key_cmp_func(const void* key, const struct chx_rb_node* node) {
    int k = *(const int*)key;
    struct test_node* n = chx_rb_entry(node, struct test_node, rb);
    if (k < n->key)
        return -1;
    else if (k > n->key)
        return 1;
    return 0;
}

/* 辅助函数：创建测试节点 */
struct test_node* create_node(int key) {
    struct test_node* node = malloc(sizeof(*node));
    assert(node != NULL);
    node->key = key;
    CHX_RB_CLEAR_NODE(&node->rb);
    return node;
}

/* 辅助函数：验证树的顺序 */
int verify_order(struct chx_rb_root* root) {
    struct chx_rb_node* node;
    int prev = -1;
    int count = 0;

    for (node = chx_rb_first(root); node; node = chx_rb_next(node)) {
        struct test_node* tn = chx_rb_entry(node, struct test_node, rb);
        if (prev >= 0 && tn->key < prev) {
            fprintf(stderr, "顺序错误: prev=%d, current=%d\n", prev, tn->key);
            return -1;
        }
        prev = tn->key;
        count++;
    }
    return count;
}

/* 辅助函数：清空树 */
void clear_tree(struct chx_rb_root* root) {
    struct chx_rb_node *node, *next;
    for (node = chx_rb_first(root); node; node = next) {
        next = chx_rb_next(node);
        struct test_node* tn = chx_rb_entry(node, struct test_node, rb);
        chx_rb_erase(&tn->rb, root);
        free(tn);
    }
}
