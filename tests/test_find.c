#include "test_helper.h"

/* 测试3: rb_find查找 */
static int test_find(void) {
    printf("测试3: rb_find查找...");
    struct chx_rb_root root = CHX_RB_ROOT;

    for (int i = 0; i < 100; i += 10) {
        struct test_node* node = create_node(i);
        chx_rb_add(&node->rb, &root, less_func);
    }

    /* 查找存在的键 */
    int key = 50;
    struct chx_rb_node* found = chx_rb_find(&key, &root, key_cmp_func);
    if (!found) {
        printf("失败 (未找到键50)\n");
        clear_tree(&root);
        return 1;
    }

    struct test_node* tn = chx_rb_entry(found, struct test_node, rb);
    if (tn->key != 50) {
        printf("失败 (找到错误的节点)\n");
        clear_tree(&root);
        return 1;
    }

    /* 查找不存在的键 */
    key = 55;
    found = chx_rb_find(&key, &root, key_cmp_func);
    if (found) {
        printf("失败 (不应该找到键55)\n");
        clear_tree(&root);
        return 1;
    }

    clear_tree(&root);
    printf("通过\n");
    return 0;
}

int main(void) { return test_find(); }
