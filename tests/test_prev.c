#include "test_helper.h"

/* 测试5: rb_prev遍历 */
static int test_prev(void) {
    printf("测试5: rb_prev反向遍历...");
    struct chx_rb_root root = CHX_RB_ROOT;

    for (int i = 0; i < 10; i++) {
        struct test_node* node = create_node(i);
        chx_rb_add(&node->rb, &root, less_func);
    }

    int expected = 9;
    for (struct chx_rb_node* node = chx_rb_last(&root); node;
         node = chx_rb_prev(node)) {
        struct test_node* tn = chx_rb_entry(node, struct test_node, rb);
        if (tn->key != expected) {
            printf("失败 (期望%d，实际%d)\n", expected, tn->key);
            clear_tree(&root);
            return 1;
        }
        expected--;
    }

    if (expected != -1) {
        printf("失败 (遍历不完整)\n");
        clear_tree(&root);
        return 1;
    }

    clear_tree(&root);
    printf("通过\n");
    return 0;
}

int main(void) { return test_prev(); }
