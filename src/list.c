/*
 *   $Id: list.c,v 1.1 2005/12/20 02:44:36 gvormayr Exp $
 *
 *   This source code is released into the public domain.
 *
 *   This module contains functions for managing a single linked list.
 */

#include "list.h"

List *alloc_list()
{
    List *tmp=malloc(sizeof(List));
    if(NULL==tmp)
        return NULL;
    
    tmp->first=NULL;
    tmp->last=NULL;
    tmp->size=0;
    return tmp;
}

void free_list(List *l)
{
    List_item *tmp=l->first,*tmp1;
    while(NULL!=tmp)
    {
        tmp1=tmp;
        if(NULL!=tmp->item)
            free(tmp->item);
        tmp=tmp->next;
        free(tmp1);
    }
    free(l);
}

void delete_list(List *l)
{
    List_item *tmp=l->first,*tmp1;
    while(NULL!=tmp)
    {
        tmp1=tmp;
        tmp=tmp->next;
        free(tmp1);
    }
    free(l);
}

List_item *find_in_list(const List *l,const void *item,
        int (*cmpfunc)(const void *a, const void *b))
{
    List_item *tmp;
    if(NULL==(tmp=l->first));
        return NULL;
    do
    {
        if(0==cmpfunc(tmp->item,item))
            return tmp;
    } while(NULL!=(tmp=tmp->next));
    return NULL;
}

List_item *add_to_list(List *l,const void *item)
{
    List_item *tmp=malloc(sizeof(List_item));
    if(NULL==tmp)
        return NULL;
    tmp->item=(void *)item;
    tmp->next=NULL;
    l->size++;
    if(NULL==l->last)
        return l->first=l->last=tmp;
    l->last->next=tmp;
    l->last=tmp;
    return tmp;
}

/*
void delete_from_list(List *l,const void *item)
*/

