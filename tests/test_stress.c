#include "test_helper.h"

/* 测试11: 大量数据压力测试 */
static int test_stress(void) {
    printf("测试11: 大量数据压力测试...");
    struct chx_rb_root root = CHX_RB_ROOT;
    const int N = 1000;

    /* 插入大量随机数据 */
    srand(time(NULL));
    for (int i = 0; i < N; i++) {
        struct test_node* node = create_node(rand() % 10000);
        chx_rb_add(&node->rb, &root, less_func);
    }

    int count = verify_order(&root);
    if (count < 0) {
        printf("失败 (顺序验证失败)\n");
        clear_tree(&root);
        return 1;
    }

    clear_tree(&root);
    printf("通过 (%d个节点)\n", count);
    return 0;
}

int main(void) { return test_stress(); }
