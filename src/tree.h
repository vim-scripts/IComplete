/*
 *   $Id: tree.h,v 1.4 2005/12/20 02:44:36 gvormayr Exp $
 *
 *   This source code is released for the public domain.
 *
 *   This file defines the public interface for managing trees which
 *   store strings.
 */
#ifndef __TREE_H__
#define __TREE_H__

#include <stdlib.h>
#include <stdio.h>
#include "list.h"
/**
 * \defgroup tree Tree module
 * \brief With this module u can easily create trees which can store strings
 *
 * layout of the tree: 
 * 
 * \dotfile tree.dot "tree layout"
 *@{*/

//! Tree item <b>and</b> header
typedef struct Tree
{
	char *item;        ///< the data
	List *subnodes;     ///< the subnodes
} Tree;

//! Return value of find_in_tree
typedef enum
{
    NOT_FOUND=0,
    DIRECT_HIT,
    SUBCLASS
} Match;

//! Allocates a tree
/*!
 * \param item data for the root item
 * \return Pointer to the tree or NULL
 */
extern Tree *alloc_tree(const char *item);

//! Frees a tree and its items
/*!
 * \param t pointer to tree to be freed
 */
extern void free_tree(Tree *t);

//! Search in tree
/*!
 * \param t Pointer to tree
 * \param item string to search for
 * \param f will point to the found element. Can be NULL
 * \returns 0 if not found.
 */
extern Match find_in_tree(Tree *t, const char *item, Tree **f);

//! Add data to tree
/*!
 * \param t Pointer to tree
 * \param item String to be added
 * \return Pointer to added tree element
 */
extern Tree *add_to_tree(Tree *t, const char *item);
// void delete_from_tree(Tree *t, const char *item);

//! Add child to tree
/*!
 * \param t Pointer to tree
 * \param child Child to be added
 * \return Pointer to added tree element
 */
extern Tree *add_child_to_tree(Tree *t, Tree *child);

//! Writes tree to file
/*!
 * \param t pointer to tree
 * \param file pointer to FILE opened with fopen
 */
extern void write_tree_to_file(Tree *t, FILE* file);

/*@}*/

#endif /* __TREE_H__ */

