#ifndef __TREE_OLD_H__
#define __TREE_OLD_H__

#include <stdio.h>

#include "parse.h"


/*
 * Build an inheritance tree for classes
 * Looks like:
 *
 *             --- subclass1
 * rootclass <
 *             --- subclass2 -- subclass2_1 --> subclass3
 *                            \ subclass2_2
 *
 * Caller must free the tree with
 * free_tree() after calling this function
 */
Tree* build_inheritance_tree(const char *class);
/* same as build_inheritance_tree() but for #include files */
Tree* build_include_tree(Tree* start, const char *filename);

/* when we find a tag, the following conditions must be met, to show the tag:
 * - method must be part of a class (key=class)
 * - access permissions (private, protected, public) must match.
 * - FIXME: method must not be pure virtual
 * - no static method
 */
bool is_member_of_scope (const tagEntry *entry, const Scope *scope, const Tree* tree);

/**
 * parses a tags file to show all class members
 * relative to the scope of another class
 */
void find_entries (const Expression *exp, const Scope *scope);

#endif /* __TREE_H__ */

