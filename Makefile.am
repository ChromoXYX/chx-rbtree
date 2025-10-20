# Library
lib_LIBRARIES = librbtree.a
librbtree_a_SOURCES = rbtree.c rbtree.h rbtree_types.h rbtree_augmented.h

# Test program
noinst_PROGRAMS = test_rbtree
test_rbtree_SOURCES = test_rbtree.c
test_rbtree_LDADD = librbtree.a

# Headers to install
include_HEADERS = rbtree.h rbtree_types.h rbtree_augmented.h

# Compiler flags
AM_CFLAGS = -Wall -Wextra -O2 -std=gnu11

# Test target
check_PROGRAMS = test_rbtree
TESTS = test_rbtree
