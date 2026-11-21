/* List helpers driver_2.c takes from the AROS-style lists.h of the original
 * vbcc build; minimal versions for the GCC build. */
#ifndef PROMLIB_GCC_LISTS_H
#define PROMLIB_GCC_LISTS_H

#include <exec/lists.h>
#include <exec/nodes.h>

#define ForeachNode(list, node) \
    for (node = (void *)(((struct List *)(list))->lh_Head); \
         ((struct Node *)(node))->ln_Succ; \
         node = (void *)(((struct Node *)(node))->ln_Succ))

#define ForeachNodeSafe(list, node, next) \
    for (node = (void *)(((struct List *)(list))->lh_Head); \
         (next = (void *)((struct Node *)(node))->ln_Succ) != NULL; \
         node = (void *)(next))

#define ListLength(list, count) \
    do { struct Node *n_; (count) = 0; \
         for (n_ = ((struct List *)(list))->lh_Head; n_->ln_Succ; n_ = n_->ln_Succ) \
             ++(count); } while (0)


static __inline__ void NewList(struct List *l)
{
    l->lh_Head = (struct Node *)&l->lh_Tail;
    l->lh_Tail = NULL;
    l->lh_TailPred = (struct Node *)&l->lh_Head;
}

#endif
