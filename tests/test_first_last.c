#include "test_helper.h"

/* 测试4: rb_first和rb_last */
static int test_first_last(void) {
    printf("测试4: rb_first和rb_last...");
    struct chx_rb_root root = CHX_RB_ROOT;

    for (int i = 10; i >= 1; i--) {
        struct test_node* node = create_node(i);
        chx_rb_add(&node->rb, &root, less_func);
    }

    struct chx_rb_node* first = chx_rb_first(&root);
    struct chx_rb_node* last = chx_rb_last(&root);

    if (!first || !last) {
        printf("失败 (first或last为NULL)\n");
        clear_tree(&root);
        return 1;
    }

    struct test_node* fn = chx_rb_entry(first, struct test_node, rb);
    struct test_node* ln = chx_rb_entry(last, struct test_node, rb);

    if (fn->key != 1 || ln->key != 10) {
        printf("失败 (first=%d, last=%d)\n", fn->key, ln->key);
        clear_tree(&root);
        return 1;
    }

    clear_tree(&root);
    printf("通过\n");
    return 0;
}

int main(void) { return test_first_last(); }
