#include "test_helper.h"

/* 测试7: cached版本 */
static int test_cached(void) {
    printf("测试7: cached版本...");
    struct chx_rb_root_cached root = CHX_RB_ROOT_CACHED;

    for (int i = 9; i >= 0; i--) {
        struct test_node* node = create_node(i);
        chx_rb_add_cached(&node->rb, &root, less_func);
    }

    struct chx_rb_node* leftmost = chx_rb_first_cached(&root);
    if (!leftmost) {
        printf("失败 (leftmost为NULL)\n");
        clear_tree(&root.rb_root);
        return 1;
    }

    struct test_node* tn = chx_rb_entry(leftmost, struct test_node, rb);
    if (tn->key != 0) {
        printf("失败 (leftmost键值错误: %d)\n", tn->key);
        clear_tree(&root.rb_root);
        return 1;
    }

    /* 测试erase_cached */
    chx_rb_erase_cached(leftmost, &root);
    free(tn);

    leftmost = chx_rb_first_cached(&root);
    tn = chx_rb_entry(leftmost, struct test_node, rb);
    if (tn->key != 1) {
        printf("失败 (删除后leftmost错误: %d)\n", tn->key);
        clear_tree(&root.rb_root);
        return 1;
    }

    clear_tree(&root.rb_root);
    printf("通过\n");
    return 0;
}

int main(void) { return test_cached(); }
