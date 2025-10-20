# Common compiler flags
AM_CFLAGS = -Wall -Wextra -O2 -std=gnu11 -I$(srcdir)

# Library
lib_LIBRARIES = libchxrbtree.a
libchxrbtree_a_SOURCES = rbtree.c rbtree.h rbtree_types.h rbtree_augmented.h

# Headers to install
include_HEADERS = rbtree.h rbtree_types.h rbtree_augmented.h

# Test programs
# Enable subdir-objects to handle sources in subdirectories
AUTOMAKE_OPTIONS = subdir-objects

# Convenience library for test helpers
noinst_LIBRARIES = libtesthelper.a
libtesthelper_a_SOURCES = tests/test_helper.c tests/test_helper.h

# List of tests
TESTS = \
    tests/test_basic_insert \
    tests/test_erase \
    tests/test_find \
    tests/test_first_last \
    tests/test_prev \
    tests/test_empty_tree \
    tests/test_cached \
    tests/test_replace_node \
    tests/test_postorder \
    tests/test_find_add \
    tests/test_stress \
    tests/test_empty_node

check_PROGRAMS = $(TESTS)

# Define sources and libraries for each test
tests_test_basic_insert_SOURCES = tests/test_basic_insert.c
tests_test_basic_insert_LDADD = libtesthelper.a libchxrbtree.a

tests_test_erase_SOURCES = tests/test_erase.c
tests_test_erase_LDADD = libtesthelper.a libchxrbtree.a

tests_test_find_SOURCES = tests/test_find.c
tests_test_find_LDADD = libtesthelper.a libchxrbtree.a

tests_test_first_last_SOURCES = tests/test_first_last.c
tests_test_first_last_LDADD = libtesthelper.a libchxrbtree.a

tests_test_prev_SOURCES = tests/test_prev.c
tests_test_prev_LDADD = libtesthelper.a libchxrbtree.a

tests_test_empty_tree_SOURCES = tests/test_empty_tree.c
tests_test_empty_tree_LDADD = libtesthelper.a libchxrbtree.a

tests_test_cached_SOURCES = tests/test_cached.c
tests_test_cached_LDADD = libtesthelper.a libchxrbtree.a

tests_test_replace_node_SOURCES = tests/test_replace_node.c
tests_test_replace_node_LDADD = libtesthelper.a libchxrbtree.a

tests_test_postorder_SOURCES = tests/test_postorder.c
tests_test_postorder_LDADD = libtesthelper.a libchxrbtree.a

tests_test_find_add_SOURCES = tests/test_find_add.c
tests_test_find_add_LDADD = libtesthelper.a libchxrbtree.a

tests_test_stress_SOURCES = tests/test_stress.c
tests_test_stress_LDADD = libtesthelper.a libchxrbtree.a

tests_test_empty_node_SOURCES = tests/test_empty_node.c
tests_test_empty_node_LDADD = libtesthelper.a libchxrbtree.a
