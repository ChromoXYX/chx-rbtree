#include "test_helper.h"

/* 测试1: 基本插入和遍历 */
static int test_basic_insert(void) {
    printf("测试1: 基本插入和遍历...");
    struct chx_rb_root root = CHX_RB_ROOT;
    int test_data[] = {5, 2, 8, 1, 3, 7, 9, 4, 6};
    int n = sizeof(test_data) / sizeof(test_data[0]);

    for (int i = 0; i < n; i++) {
        struct test_node* node = create_node(test_data[i]);
        chx_rb_add(&node->rb, &root, less_func);
    }

    int count = verify_order(&root);
    if (count != n) {
        printf("失败 (期望%d个节点，实际%d个)\n", n, count);
        clear_tree(&root);
        return 1;
    }

    clear_tree(&root);
    printf("通过\n");
    return 0;
}

int main(void) { return test_basic_insert(); }
