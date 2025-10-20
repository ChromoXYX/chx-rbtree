#include "test_helper.h"

/* 测试6: 空树操作 */
static int test_empty_tree(void) {
    printf("测试6: 空树操作...");
    struct chx_rb_root root = CHX_RB_ROOT;

    if (!CHX_RB_EMPTY_ROOT(&root)) {
        printf("失败 (RB_EMPTY_ROOT检查失败)\n");
        return 1;
    }

    if (chx_rb_first(&root) != NULL || chx_rb_last(&root) != NULL) {
        printf("失败 (空树的first/last应为NULL)\n");
        return 1;
    }

    printf("通过\n");
    return 0;
}

int main(void) { return test_empty_tree(); }
