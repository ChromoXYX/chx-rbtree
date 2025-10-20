#include "test_helper.h"

/* 测试12: RB_EMPTY_NODE和RB_CLEAR_NODE */
static int test_empty_node(void) {
    printf("测试12: RB_EMPTY_NODE和RB_CLEAR_NODE...");
    struct test_node node;

    CHX_RB_CLEAR_NODE(&node.rb);
    if (!CHX_RB_EMPTY_NODE(&node.rb)) {
        printf("失败 (RB_CLEAR_NODE后应为空)\n");
        return 1;
    }

    /* 插入到树中后不应为空 */
    struct chx_rb_root root = CHX_RB_ROOT;
    node.key = 10;
    chx_rb_add(&node.rb, &root, less_func);

    if (CHX_RB_EMPTY_NODE(&node.rb)) {
        printf("失败 (插入后不应为空)\n");
        chx_rb_erase(&node.rb, &root);
        return 1;
    }

    chx_rb_erase(&node.rb, &root);
    printf("通过\n");
    return 0;
}

int main(void) { return test_empty_node(); }
