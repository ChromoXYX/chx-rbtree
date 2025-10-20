#include "rbtree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

/* 测试节点结构 */
struct test_node {
    int key;
    struct chx_rb_node rb;
};

/* 比较函数 */
static bool less_func(struct chx_rb_node* a, const struct chx_rb_node* b) {
    struct test_node* node_a = chx_rb_entry(a, struct test_node, rb);
    struct test_node* node_b = chx_rb_entry(b, struct test_node, rb);
    return node_a->key < node_b->key;
}

static int cmp_func(struct chx_rb_node* a, const struct chx_rb_node* b) {
    struct test_node* node_a = chx_rb_entry(a, struct test_node, rb);
    struct test_node* node_b = chx_rb_entry(b, struct test_node, rb);
    if (node_a->key < node_b->key)
        return -1;
    else if (node_a->key > node_b->key)
        return 1;
    return 0;
}

static int key_cmp_func(const void* key, const struct chx_rb_node* node) {
    int k = *(const int*)key;
    struct test_node* n = chx_rb_entry(node, struct test_node, rb);
    if (k < n->key)
        return -1;
    else if (k > n->key)
        return 1;
    return 0;
}

/* 辅助函数：创建测试节点 */
static struct test_node* create_node(int key) {
    struct test_node* node = malloc(sizeof(*node));
    assert(node != NULL);
    node->key = key;
    CHX_RB_CLEAR_NODE(&node->rb);
    return node;
}

/* 辅助函数：验证树的顺序 */
static int verify_order(struct chx_rb_root* root) {
    struct chx_rb_node* node;
    int prev = -1;
    int count = 0;

    for (node = chx_rb_first(root); node; node = chx_rb_next(node)) {
        struct test_node* tn = chx_rb_entry(node, struct test_node, rb);
        if (prev >= 0 && tn->key < prev) {
            fprintf(stderr, "顺序错误: prev=%d, current=%d\n", prev, tn->key);
            return -1;
        }
        prev = tn->key;
        count++;
    }
    return count;
}

/* 辅助函数：清空树 */
static void clear_tree(struct chx_rb_root* root) {
    struct chx_rb_node *node, *next;
    for (node = chx_rb_first(root); node; node = next) {
        next = chx_rb_next(node);
        struct test_node* tn = chx_rb_entry(node, struct test_node, rb);
        chx_rb_erase(&tn->rb, root);
        free(tn);
    }
}

/* 测试1: 基本插入和遍历 */
static int test_basic_insert(void) {
    printf("测试1: 基本插入和遍历...");
    struct chx_rb_root root = CHX_RB_ROOT;
    int test_data[] = {5, 2, 8, 1, 3, 7, 9, 4, 6};
    int n = sizeof(test_data) / sizeof(test_data[0]);

    for (int i = 0; i < n; i++) {
        struct test_node* node = create_node(test_data[i]);
        chx_rb_add(&node->rb, &root, less_func);
    }

    int count = verify_order(&root);
    if (count != n) {
        printf("失败 (期望%d个节点，实际%d个)\n", n, count);
        clear_tree(&root);
        return 1;
    }

    clear_tree(&root);
    printf("通过\n");
    return 0;
}

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

/* 测试3: rb_find查找 */
static int test_find(void) {
    printf("测试3: rb_find查找...");
    struct chx_rb_root root = CHX_RB_ROOT;

    for (int i = 0; i < 100; i += 10) {
        struct test_node* node = create_node(i);
        chx_rb_add(&node->rb, &root, less_func);
    }

    /* 查找存在的键 */
    int key = 50;
    struct chx_rb_node* found = chx_rb_find(&key, &root, key_cmp_func);
    if (!found) {
        printf("失败 (未找到键50)\n");
        clear_tree(&root);
        return 1;
    }

    struct test_node* tn = chx_rb_entry(found, struct test_node, rb);
    if (tn->key != 50) {
        printf("失败 (找到错误的节点)\n");
        clear_tree(&root);
        return 1;
    }

    /* 查找不存在的键 */
    key = 55;
    found = chx_rb_find(&key, &root, key_cmp_func);
    if (found) {
        printf("失败 (不应该找到键55)\n");
        clear_tree(&root);
        return 1;
    }

    clear_tree(&root);
    printf("通过\n");
    return 0;
}

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

int main(void) {
    int failed = 0;

    printf("======== rbtree 单元测试 ========\n\n");

    failed += test_basic_insert();
    failed += test_erase();
    failed += test_find();
    failed += test_first_last();
    failed += test_prev();
    failed += test_empty_tree();
    failed += test_cached();
    failed += test_replace_node();
    failed += test_postorder();
    failed += test_find_add();
    failed += test_stress();
    failed += test_empty_node();

    printf("\n================================\n");
    if (failed == 0) {
        printf("所有测试通过！\n");
        return 0;
    } else {
        printf("失败: %d 个测试\n", failed);
        return 1;
    }
}
