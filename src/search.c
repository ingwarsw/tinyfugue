/*************************************************************************
 *  TinyFugue - programmable mud client
 *  Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2002, 2003, 2004, 2005, 2006-2007 Ken Keys
 *
 *  TinyFugue (aka "tf") is protected under the terms of the GNU
 *  General Public License.  See the file "COPYING" for details.
 ************************************************************************/
static const char RCSid[] = "$Id: search.c,v 35004.32 2007/01/13 23:12:39 kkeys Exp $";


/**********************************************
 * trie, hash table, and linked list routines *
 **********************************************/

#include "tfconfig.h"
#include "port.h"
#include "malloc.h"
#include "search.h"


static ListEntry *nodepool = NULL;		/* freelist */


/********/
/* trie */
/********/

/* Find the datum in trie assosiated with the key. */
void *trie_find(TrieNode *root, const unsigned char *key)
{
    TrieNode *n;

    for (n = root; n && n->children && *key; n = n->u.child[*key++]);
    return (n && !n->children && !*key) ? n->u.datum : NULL;
}

/* Insert a datum into the trie pointed to by root.
 * If key is substring, superstring, or duplicate of an existing key, intrie()
 * returns TRIE_SUB, TRIE_SUPER, or TRIE_DUP and does not insert.
 * Otherwise, returns 1 for success.
 */
int intrie(TrieNode **root, void *datum, const unsigned char *key)
{
    int i;

    if (!*root) {
        *root = (TrieNode *) XMALLOC(sizeof(TrieNode));
        if (*key) {
            (*root)->children = 1;
            (*root)->u.child = (TrieNode**)XMALLOC(0x100 * sizeof(TrieNode *));
            for (i = 0; i < 0x100; i++) (*root)->u.child[i] = NULL;
            return intrie(&(*root)->u.child[*key], datum, key + 1);
        } else {
            (*root)->children = 0;
            (*root)->u.datum = datum;
            return 1;
        }
    } else {
        if (*key) {
            if ((*root)->children) {
                if (!(*root)->u.child[*key]) (*root)->children++;
                return intrie(&(*root)->u.child[*key], datum, key+1);
            } else {
                return TRIE_SUPER;
            }
        } else {
            return ((*root)->children) ? TRIE_SUB : TRIE_DUP;
        }
    }
}

TrieNode *untrie(TrieNode **root, const unsigned char *s)
{
    if (*s) {
        if (untrie(&((*root)->u.child[*s]), s + 1)) return *root;
        if (--(*root)->children) return *root;
        FREE((*root)->u.child);
    }
    FREE(*root);
    return *root = NULL;
}


/***************/
/* linked list */
/***************/

void init_list(List *list)
{
    list->head = list->tail = NULL;
}

/* delete Node from linked list */
void *unlist(ListEntry *node, List *list)
{
    void *result;

    *(node->next ? &node->next->prev : &list->tail) = node->prev;
    *(node->prev ? &node->prev->next : &list->head) = node->next;
    result = node->datum;
    pfree(node, nodepool, next);
    return result;
}

/* Create new node for datum and insert into list in sorted order.
 * <cmp> is a function that compares two data.
 */
ListEntry *sinsert(void *datum, List *list, Cmp *cmp)
{
    ListEntry *node;

    node = list->head;
    while (node && (*cmp)(datum, node->datum) > 0) {
        node = node->next;
    }
    return inlist(datum, list, node ? node->prev : list->tail);
}

/* Create new node for datum and insert into list.
 * If where is non-null, insert after it; else, insert at beginning
 */
ListEntry *inlist_fl(void *datum, List *list, ListEntry *where,
    const char *file, int line)
{
    ListEntry *node;

    palloc(node, ListEntry, nodepool, next, file, line);
    node->datum = datum;
    if (where) {
        node->next = where->next;
        where->next = node;
    } else {
        node->next = list->head;
        list->head = node;
    }
    node->prev = where;
    *(node->next ? &node->next->prev : &list->tail) = node;
    return node;
}


/**************/
/* hash table */
/**************/

void init_hashtable(HashTable *table, int size, Cmp *cmp)
{
    table->size = size;
    table->cmp = cmp;
    table->bucket = (List **)XMALLOC(sizeof(List *) * size);
    while (size)
        table->bucket[--size] = NULL;
}

/* find entry by name */
void *hashed_find(const char *name, unsigned int hash, HashTable *table)
{
    List *bucket;
    ListEntry *node;

    bucket = table->bucket[hash % table->size];
    if (bucket) {
        for (node = bucket->head; node; node = node->next)
            if ((*table->cmp)((void *)name, node->datum) == 0)
                return node->datum;
    }
    return NULL;
}

unsigned int hash_string(const char *str)
{
    unsigned int h;

    for (h = 0; *str; str++)
        h = (h << 5) + h + lcase(*str);
    return h;
}

ListEntry *hashed_insert(void *datum, unsigned int hash, HashTable *table)
{
    int indx = hash % table->size;
    if (!table->bucket[indx]) {
        table->bucket[indx] = (List *)XMALLOC(sizeof(List));
        init_list(table->bucket[indx]);
    }
    return inlist(datum, table->bucket[indx], NULL);
}


void hash_remove(ListEntry *node, HashTable *tab)
{
    unlist(node, tab->bucket[hash_string(*(char**)node->datum) % tab->size]);
}


/***************/
/* comparisons */
/***************/

/* strstructcmp - compares a string to the first field of a structure */
int strstructcmp(const void *key, const void *datum)
{
    return strcmp((char *)key, *(char **)datum);
}

/* cstrstructcmp - compares string to first field of a struct, ignoring case */
int cstrstructcmp(const void *key, const void *datum)
{
    return cstrcmp((char *)key, *(char **)datum);
}

/* strpppcmp - for qsort array of ptrs to structs whose 1st member is char* */
int strpppcmp(const void *a, const void *b)
{
    return strcmp(**(char ***)a, **(char ***)b);
}

/* cstrpppcmp - for qsort array of ptrs to structs whose 1st member is char* */
int cstrpppcmp(const void *a, const void *b)
{
    return cstrcmp(**(char ***)a, **(char ***)b);
}


/*******************/
/* circular queues */
/*******************/
static int alloc_cqueue(CQueue *cq, int maxsize)
{
    cq->maxsize = maxsize;
    if (maxsize) {
        cq->data =
            (void**)MALLOC(maxsize * sizeof(void*));
        if (!cq->data) {
            /*eprintf("not enough memory for %d lines.", maxsize); */ /* XXX */
            cq->maxsize = 1;
            cq->data = (void**)XMALLOC(1 * sizeof(void*));
	    return 0;
        }
    } else {
        cq->data = NULL;
    }
    return 1;
}

struct CQueue *init_cqueue(CQueue *cq, int maxsize,
    void (*free_f)(void*, const char *, int))
{
    if (!cq) cq = (CQueue*)XMALLOC(sizeof(CQueue));
    cq->free = free_f;
    cq->last = cq->index = -1;
    cq->first = cq->size = cq->total = 0;
    alloc_cqueue(cq, maxsize);
    return cq;
}

void free_cqueue(CQueue *cq)
{
    if (cq->data) {
        for ( ; cq->size; cq->size--) {
            cq->free(cq->data[cq->first], __FILE__, __LINE__);
            cq->first = nmod(cq->first + 1, cq->maxsize);
        }
        cq->first = 0;
        cq->last = -1;
        FREE(cq->data);
    }
}

void encqueue(CQueue *cq, void *datum)
{
    if (!cq->data)
        alloc_cqueue(cq, cq->maxsize);
    if (cq->size == cq->maxsize) {
        cq->free(cq->data[cq->first], __FILE__, __LINE__);
        cq->first = nmod(cq->first + 1, cq->maxsize);
    } else {
        cq->size++;
    }
    cq->last = nmod(cq->last + 1, cq->maxsize);
    cq->data[cq->last] = datum;
    cq->total++;
}

void cqueue_replace(CQueue *cq, void *datum, int idx)
{
    cq->free(cq->data[idx], __FILE__, __LINE__);
    cq->data[idx] = datum;
}

int resize_cqueue(CQueue *cq, int maxsize)
{
    int first, last, size;
    void **newdata;

    /* XXX should use version of malloc without reserve */
    if (!(newdata = (void**)MALLOC(maxsize * sizeof(void*))))
	return 0;
    first = nmod(cq->total, maxsize);
    last = nmod(cq->total - 1, maxsize);
    for (size = 0; cq->size; cq->size--) {
	if (size < maxsize) {
	    first = nmod(first - 1, maxsize);
	    newdata[first] = cq->data[cq->last];
	    size++;
	} else {
	    cq->free(cq->data[cq->last], __FILE__, __LINE__);
	}
	cq->last = nmod(cq->last - 1, cq->maxsize);
    }
    if (cq->data) FREE(cq->data);
    cq->data = newdata;
    cq->first = first;
    cq->last = last;
    cq->size = size;
    cq->maxsize = maxsize;

    cq->index = cq->last;
    return cq->maxsize;
}


#if USE_DMALLOC
void free_search(void)
{
    pfreepool(ListEntry, nodepool, next);
}

void free_hash(HashTable *table)
{
    int i;
    for (i = 0; i < table->size; i++) {
        if (table->bucket[i]) FREE(table->bucket[i]);
    }
    FREE(table->bucket);
}
#endif
