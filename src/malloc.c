/*************************************************************************
 *  TinyFugue - programmable mud client
 *  Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2002, 2003, 2004, 2005, 2006-2007 Ken Keys
 *
 *  TinyFugue (aka "tf") is protected under the terms of the GNU
 *  General Public License.  See the file "COPYING" for details.
 ************************************************************************/
static const char RCSid[] = "$Id: malloc.c,v 35004.22 2007/01/13 23:12:39 kkeys Exp $";

#include "tfconfig.h"
#include "port.h"
#include "signals.h"
#include "malloc.h"

caddr_t mmalloc_base = NULL;
int low_memory_warning = 0;
static char *reserve = NULL;

void init_malloc(void)
{
    reserve = MALLOC(1024*16);
}

void *xmalloc(void *md, long unsigned size, const char *file, const int line)
{
    void *memory;

    if ((long)size <= 0)
        core("xmalloc(%ld).", file, line, (long)size);

    memory = (void*)dmalloc(md, size, file, line);
    if (!memory) {
        if (reserve) {
            low_memory_warning = 1;
            FREE(reserve);
            reserve = NULL;
            memory = (void*)dmalloc(md, size, file, line);
        }
        if (!memory)
            error_exit("xmalloc(%ld): out of memory.", file, line, (long)size);
    }

    return memory;
}

void *xrealloc(void *md, void *ptr, long unsigned size,
    const char *file, const int line)
{
    void *memory;

    if ((long)size <= 0)
        core("xrealloc(%ld).", file, line, (long)size);

    memory = (void*)drealloc(md, ptr, size, file, line);
    if (!memory) {
        if (reserve) {
            low_memory_warning = 1;
            FREE(reserve);
            reserve = NULL;
            memory = (void*)drealloc(md, ptr, size, file, line);
        }
        if (!memory)
            error_exit("xrealloc(%ld): out of memory.", file, line, (long)size);
    }

    return memory;
}

void *xcalloc(void *md, long unsigned size, const char *file, const int line)
{
    void *memory;

    if ((memory = xmalloc(md, size, file, line)))
        memset(memory, '\0', size);
    return memory;
}

void xfree(void *md, void *ptr, const char *file, const int line)
{
    dfree(md, ptr, file, line);
    if (!reserve)
        init_malloc();
}

#if USE_DMALLOC
void free_reserve(void)
{
    FREE(reserve);
}
#endif
