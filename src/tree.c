#include <string.h>
#include "tree.h"

Tree *alloc_tree(const char *item)
{
    Tree *tmp=malloc(sizeof(Tree));
    if(NULL==tmp)
        return NULL;
    tmp->item=(void *)item;
    tmp->subnodes=alloc_list(NULL);
    return tmp;
}

void free_tree(Tree *t)
{
	if(NULL==t)
		return;
    List *l=alloc_list(NULL);
    if(NULL==l)
        return;
    List_item *i=add_to_list(l,t);
    if(NULL==i)
    {
        free_list(l);
        return;
    }
    do
    {
        t=i->item;
        if(NULL!=t->item)
            free(t->item);
        List_item *tmp=t->subnodes->first;
        if(NULL!=tmp)
            do
            {
                add_to_list(l,tmp->item);
            } while(NULL!=(tmp=tmp->next));
        delete_list(t->subnodes);
    } while(NULL!=(i=i->next));
    free_list(l);
}

Match find_in_tree(Tree *t,const char *item, Tree **f)
{    
    List *l=alloc_list(NULL);
    if(NULL==l)
        return 0;
    List_item *i=add_to_list(l,t);
    if(NULL==i)
    {
        delete_list(l);
        return 0;
    }
    int j=1;
    do
    {
        t=i->item;
        if(0==strcmp(t->item,item))
        {
            delete_list(l);
            if(NULL!=f)
                *f=t;
            return (j==1?DIRECT_HIT:SUBCLASS);
        }
        List_item *tmp=t->subnodes->first;
        if(NULL!=tmp)
            do
            {
                if(NULL==add_to_list(l,tmp->item))
                {
                    delete_list(l);
                    return 0;
                }
            } while(NULL!=(tmp=tmp->next));
        j++;
    } while(NULL!=(i=i->next));
    delete_list(l);
    return 0;
}


Tree *add_child_to_tree(Tree *t, Tree *child)
{
    if(NULL==add_to_list(t->subnodes,child))
        return NULL;
    return child;
}

Tree *add_to_tree(Tree *t, const char *item)
{
    Tree *tmp=alloc_tree(item);
    if(NULL==tmp)
        return NULL;
    if(NULL==add_child_to_tree(t,tmp))
    {
        free_tree(tmp);
        return NULL;
    }
    return tmp;
}

// void delete_from_tree(Tree *t, const char *item);

void write_tree_to_file(Tree *t, FILE* file)
{
    List *l=alloc_list(NULL);
    if(NULL==l)
        return;
    List_item *i=add_to_list(l,t);
    if(NULL==i)
    {
        delete_list(l);
        return;
    }
    do
    {
        t=i->item;
        fprintf(file,"%s\n",t->item);
        List_item *tmp=t->subnodes->first;
        if(NULL!=tmp)
            do
            {
                if(NULL==add_to_list(l,tmp->item))
                {
                    delete_list(l);
                    return;
                }
            } while(NULL!=(tmp=tmp->next));
    } while(NULL!=(i=i->next));
    delete_list(l);
}

