/*
 *   $Id: list.h,v 1.1 2005/12/20 02:44:36 gvormayr Exp $
 *
 *   This source code is released for the public domain.
 *
 *   This file defines the public interface for managing single linked
 *   lists.
 *
 *   The header stores the first AND the last node - so adding is
 *   faster than in standard lists :)
 */
#ifndef __SLL_H
#define __SLL_H

#include <stdlib.h>

/**
 * \defgroup list Single linked list module
 * \brief With this module u can easily create lists
 * 
 * Don't forget to free the lists!!!
 * 
 * layout of list:
 * 
 * \dotfile list.dot "list layout"
 *@{*/

//! list item
typedef struct List_item
{
    void *item;               ///< the data
    struct List_item *next;    ///< next item
} List_item;

//! list header
typedef struct       
{
    List_item *first;          ///< first item
    List_item *last;           ///< last item
    size_t size;              ///< size of list
} List;

//! Allocates header of list
/*!
 * \return Header of list or NULL
 */
extern List *alloc_list();

//! Frees list and the containing items
/*!
 * \param l Header of the list
 */
extern void free_list(List *l);

//! Frees list - but not the items
/*!
 * \param l Header of list
 */
extern void delete_list(List *l);

//! Adds data to list
/*!
 * \param l Header of the list
 * \param item Item to add
 */
extern List_item *add_to_list(List *l,const void *item);

//! Searches in list
/*!
 * \param l Header of list
 * \param item Item to search for
 * \param cmpfunc compare function
 * \return Pointer to item or NULL
 */
extern List_item *find_in_list(const List *l,const void *item,
        int (*cmpfunc)(const void *a, const void *b));

// void delete_from_list(List *l,const void *item); we need no deletion atm

/*@}*/

#endif //__SLL_H

