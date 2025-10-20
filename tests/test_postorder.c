#include "test_helper.h"

/* 测试9: postorder遍历 */
static int test_postorder(void) {
    printf("测试9: postorder遍历...");
    struct chx_rb_root root = CHX_RB_ROOT;

    for (int i = 0; i < 10; i++) {
        struct test_node* node = create_node(i);
        chx_rb_add(&node->rb, &root, less_func);
    }

    int count = 0;
    for (struct chx_rb_node* node = chx_rb_first_postorder(&root); node;
         node = chx_rb_next_postorder(node)) {
        count++;
    }

    if (count != 10) {
        printf("失败 (遍历数量错误: %d)\n", count);
        clear_tree(&root);
        return 1;
    }

    clear_tree(&root);
    printf("通过\n");
    return 0;
}

int main(void) { return test_postorder(); }
