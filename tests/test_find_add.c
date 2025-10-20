#include "test_helper.h"

/* 测试10: rb_find_add */
static int test_find_add(void) {
    printf("测试10: rb_find_add...");
    struct chx_rb_root root = CHX_RB_ROOT;

    /* 插入新节点 */
    struct test_node* node1 = create_node(10);
    struct chx_rb_node* result = chx_rb_find_add(&node1->rb, &root, cmp_func);
    if (result != NULL) {
        printf("失败 (首次插入应返回NULL)\n");
        clear_tree(&root);
        return 1;
    }

    /* 尝试插入重复节点 */
    struct test_node* node2 = create_node(10);
    result = chx_rb_find_add(&node2->rb, &root, cmp_func);
    if (result == NULL) {
        printf("失败 (重复插入应返回已存在节点)\n");
        free(node2);
        clear_tree(&root);
        return 1;
    }
    free(node2);

    int count = verify_order(&root);
    if (count != 1) {
        printf("失败 (应只有1个节点)\n");
        clear_tree(&root);
        return 1;
    }

    clear_tree(&root);
    printf("通过\n");
    return 0;
}

int main(void) { return test_find_add(); }
