/*************************************************************************
 *  TinyFugue - programmable mud client
 *  Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2002, 2003, 2004, 2005, 2006-2007 Ken Keys
 *
 *  TinyFugue (aka "tf") is protected under the terms of the GNU
 *  General Public License.  See the file "COPYING" for details.
 ************************************************************************/
/* $Id: malloc.h,v 35004.24 2007/01/13 23:12:39 kkeys Exp $ */

/* Function hierarchy:
 *  xmalloc: will kill process if it fails.  Calls dmalloc.
 *  dmalloc: if USE_DMALLOC, do debugging.  Calls mmalloc.
 *  mmalloc: if USE_MMALLOC, use GNU mmalloc; otherwise call native functions.
 *   malloc: if !USE_MMALLOC, use native functions; otherwise, call mmalloc.
 *
 * XMALLOC is like xmalloc, but has file and line built in.
 *  MALLOC is like dmalloc, but has file and line built in.
 *
 * Most code will call XMALLOC() (for automatic error checking) or MALLOC()
 * (if it can handle failure itself).  Code may call xmalloc() or dmalloc() 
 * to use nonlocal file and line info.  Code should almost never call
 * mmalloc() or malloc() directly.
 */

#ifndef MALLOC_H
#define MALLOC_H

extern int low_memory_warning;

#if USE_MMALLOC
# include "mmalloc/mmalloc.h"
# define malloc(size)			mmalloc(NULL, size)
# define calloc(size)			mcalloc(NULL, size)
# define realloc(ptr, size)		mrealloc(NULL, ptr, size)
# define free(ptr)			mfree(NULL, ptr)
#else
# define mmalloc(md, size)		malloc(size)
# define mcalloc(md, size)		calloc(size)
# define mrealloc(md, ptr, size)	realloc(ptr, size)
# define mfree(md, ptr)			free(ptr)
#endif

#define XMALLOC(size) xmalloc(NULL, (size), __FILE__, __LINE__)
#define XCALLOC(size) xcalloc(NULL, (size), __FILE__, __LINE__)
#define XREALLOC(ptr, size) xrealloc(NULL, (ptr), (size), __FILE__, __LINE__)
#define MALLOC(size) dmalloc(NULL, (size), __FILE__, __LINE__)
#define CALLOC(size) dcalloc(NULL, (size), __FILE__, __LINE__)
#define REALLOC(ptr, size) drealloc(NULL, (ptr), (size), __FILE__, __LINE__)
#define FREE(ptr) xfree(NULL, (void*)(ptr), __FILE__, __LINE__)

#if USE_DMALLOC
extern void  *dmalloc(void *md, long unsigned size,
                       const char *file, const int line);
extern void  *dcalloc(void *md, long unsigned size,
                       const char *file, const int line);
extern void  *drealloc(void *md, void *ptr, long unsigned size,
                       const char *file, const int line);
extern void   dfree(void *md, void *ptr,
                       const char *file, const int line);
#else
# define dmalloc(md, size, file, line)	mmalloc(md, size)
# define dcalloc(md, size, file, line)	mcalloc(md, size)
# define drealloc(md, ptr, size, file, line) \
					mrealloc(md, (void*)(ptr), (size))
# define dfree(md, ptr, file, line)	mfree(md, (void*)(ptr))
#endif

extern void  *xmalloc(void *md, long unsigned size,
                       const char *file, const int line);
extern void  *xcalloc(void *md, long unsigned size,
                       const char *file, const int line);
extern void  *xrealloc(void *md, void *ptr, long unsigned size,
                       const char *file, const int line);
extern void      xfree(void *md, void *ptr,
                       const char *file, const int line);
extern void      init_malloc(void);

/* Fast allocation from pool.
 * Should be used only on objects which are freed frequently, with md==NULL.
 */
#define palloc(item, type, pool, next, file, line) \
    ((pool) ? \
      ((item) = (pool), (pool) = (void*)(pool)->next) : \
      ((item) = (type *)xmalloc(NULL, sizeof(type), file, line)))

#if !USE_DMALLOC
# define pfree_fl(item, pool, next, file, line) \
    ((item)->next = (void*)(pool), (pool) = (item))
# define pfreepool(type, pool, next) \
    do { \
	type *tmp; \
	while (pool) { \
	   tmp = pool; \
	   pool = (type*)pool->next; \
	   FREE(tmp); \
	} \
    } while (0)
#else
# define pfree_fl(item, pool, next, file, line) \
    dfree(NULL, item, file, line)
# define pfreepool(type, pool, next)	/* do nothing */
#endif

#define pfree(item, pool, next)  pfree_fl(item, pool, next, __FILE__, __LINE__)

#if USE_DMALLOC
extern void   free_reserve(void);
extern void   debug_mstats(const char *s);
#endif

#endif /* MALLOC_H */
