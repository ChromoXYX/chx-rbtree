#include "test_helper.h"

/* 测试2: 删除操作 */
static int test_erase(void) {
    printf("测试2: 删除操作...");
    struct chx_rb_root root = CHX_RB_ROOT;
    struct test_node* nodes[10];

    for (int i = 0; i < 10; i++) {
        nodes[i] = create_node(i);
        chx_rb_add(&nodes[i]->rb, &root, less_func);
    }

    /* 删除一些节点 */
    chx_rb_erase(&nodes[5]->rb, &root);
    free(nodes[5]);
    chx_rb_erase(&nodes[0]->rb, &root);
    free(nodes[0]);
    chx_rb_erase(&nodes[9]->rb, &root);
    free(nodes[9]);

    int count = verify_order(&root);
    if (count != 7) {
        printf("失败 (期望7个节点，实际%d个)\n", count);
        clear_tree(&root);
        return 1;
    }

    clear_tree(&root);
    printf("通过\n");
    return 0;
}

int main(void) { return test_erase(); }
