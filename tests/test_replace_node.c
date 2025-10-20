#include "test_helper.h"

/* 测试8: rb_replace_node */
static int test_replace_node(void) {
    printf("测试8: rb_replace_node...");
    struct chx_rb_root root = CHX_RB_ROOT;
    struct test_node* nodes[5];

    for (int i = 0; i < 5; i++) {
        nodes[i] = create_node(i * 10);
        chx_rb_add(&nodes[i]->rb, &root, less_func);
    }

    /* 替换中间节点 */
    struct test_node* new_node = create_node(20);
    chx_rb_replace_node(&nodes[2]->rb, &new_node->rb, &root);
    free(nodes[2]);

    int count = verify_order(&root);
    if (count != 5) {
        printf("失败 (节点数错误)\n");
        clear_tree(&root);
        return 1;
    }

    clear_tree(&root);
    printf("通过\n");
    return 0;
}

int main(void) { return test_replace_node(); }
