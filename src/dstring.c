/*************************************************************************
 *  TinyFugue - programmable mud client
 *  Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2002, 2003, 2004, 2005, 2006-2007 Ken Keys
 *
 *  TinyFugue (aka "tf") is protected under the terms of the GNU
 *  General Public License.  See the file "COPYING" for details.
 ************************************************************************/
static const char RCSid[] = "$Id: dstring.c,v 35004.49 2007/01/13 23:12:39 kkeys Exp $";


/*********************************************************************
 * Fugue dynamically allocated string handling                       *
 *                                                                   *
 * dSinit() must be used to initialize a dynamically allocated       *
 * string, and dSfree() to free up the contents.  To minimize        *
 * resize()s, initialize the size to be a little more than the       *
 * median expected size.                                             *
 *********************************************************************/

#include "tfconfig.h"
#include "port.h"
#include "malloc.h"
#include "tf.h"
#include "signals.h"	/* core() */

static String *Stringpool = NULL;	/* freelist */
conString blankline[1] = { STRING_LITERAL("") };

#if USE_MMALLOC
# define MD(str)	(str->md)
#else
# define MD(str)	(NULL)
#endif

#define lcheck(str, file, line) \
    do { if ((str)->len >= (str)->size)  resize(str, file, line); } while(0)

#if USE_DMALLOC
# define Smalloc(str, size) \
    (str->static_struct ? \
        mmalloc(MD(str), size) : xmalloc(MD(str), size, file, line))
# define Srealloc(str, ptr, size) \
    (str->static_struct ? \
        mrealloc(MD(str), ptr, size) : xrealloc(MD(str), ptr, size, file, line))
# define Sfree(str, ptr) \
    (str->static_struct ? mfree(MD(str), ptr) : xfree(MD(str), ptr, file, line))
#else
# define Smalloc(str, size)		xmalloc(MD(str), size, file, line)
# define Srealloc(str, ptr, size)	xrealloc(MD(str), ptr, size, file, line)
# define Sfree(str, ptr)		xfree(MD(str), ptr, file, line)
#endif

static void  resize(String *str, const char *file, int line);


/* create charattrs and initialize first n elements */
void check_charattrs(String *str, int n, cattr_t cattrs,
    const char *file, int line)
{
    if (!str->charattrs) {
        cattrs &= F_HWRITE;
        str->charattrs = Smalloc(str, sizeof(cattr_t) * str->size);
        while (--n >= 0)
            str->charattrs[n] = cattrs;
    }
}

/* copy old trailing charattr to new tail */
void extend_charattrs(String *str, int oldlen, cattr_t cattrs)
{
    int i;

    for (i = oldlen+1; i < str->len; i++)
        str->charattrs[i] = str->charattrs[oldlen] | cattrs;
    str->charattrs[str->len] = str->charattrs[oldlen];
    str->charattrs[oldlen] = cattrs;
}

static void resize(String *str, const char *file, int line)
{
    if (!str->resizable) {
        internal_error2(file, line, str->file, str->line, "");
        core("resize: data not resizable", file, line, 0);
    }
    if (str->size < 0) {
        internal_error2(file, line, str->file, str->line, "");
        core("resize freed string", file, line, 0);
    }
    str->size = (str->len / ALLOCSIZE + 1) * ALLOCSIZE;

    str->data = Srealloc(str, str->data, str->size);

    if (str->charattrs) {
        str->charattrs =
            Srealloc(str, str->charattrs, sizeof(cattr_t) * str->size);
    }
}

/* dSinit()
 *  data && len >= 0:  allocate exactly len, copy data
 *  data && len < 0:   allocate exactly strlen(data), copy data
 * !data && len < 0:   don't allocate
 * !data && len >= 0:  allocate rounded len (allocating for 0 sometimes
 *                     wastes an allocation, but not doing it can cause
 *                     problems in code that expects (str->data != NULL))
 * md is used only if (!str && data).
 */
String *dSinit(
    String *str,	/* if NULL, a String will be allocated */
    const char *data,	/* optional initializer data */
    int len,		/* optional length of data */
    attr_t attrs,	/* line display attributes */
    void *md,		/* mmalloc descriptor */
    const char *file,
    int line)
{
    if (data && len < 0)
        len = strlen(data);
    if (!str) {
        if (data) {
            /* allocate String and data in one chunk for better locality */
#if USE_MMALLOC
            if (md) str = dmalloc(md, sizeof(*str) + len + 1, file, line);
            if (!md || !str)
#endif
            {
                md = NULL;
                str = xmalloc(NULL, sizeof(*str) + len + 1, file, line);
            }
            str->data = (char*)str + sizeof(*str);
            str->dynamic_data = 0;
        } else {
            palloc(str, String, Stringpool, data, file, line);
            str->dynamic_data = 1;
        }
        str->dynamic_struct = 1;
        str->static_struct = 0;
    } else {
        str->dynamic_struct = 0;
        str->static_struct = 0;
        if (data) {
            str->data = Smalloc(str, len + 1);
            str->dynamic_data = 1;
        }
    }
    if (data) {
#if USE_MMALLOC
        str->md = md;
#endif
        str->resizable = 0;
        str->len = len;
        str->size = len + 1;
        memcpy(str->data, data, str->len);
        str->data[str->len] = '\0';
    } else if (len >= 0) {
#if USE_MMALLOC
        str->md = NULL;
#endif
        str->resizable = 1;
        str->dynamic_data = 1;
        str->size = ((len + ALLOCSIZE) / ALLOCSIZE) * ALLOCSIZE;
        str->data = Smalloc(str, str->size);
        str->len = 0;
        str->data[str->len] = '\0';
    } else {
#if USE_MMALLOC
        str->md = NULL;
#endif
        str->resizable = 1;
        str->dynamic_data = 1;
        str->data = NULL;
        str->size = str->len = 0;
    }
    str->attrs = attrs;
    str->charattrs = NULL;
    str->links = 0;
    str->time.tv_sec = str->time.tv_usec = -1;  /* caller will set if needed */
    str->file = file;
    str->line = line;
    return str;
}

/* dSfree() assumes links has been decremented and tested by Stringfree() */
void dSfree(String *str, const char *file, int line)
{
    if (str->links < 0) {
	/* While it would be useful to print str->file, it may have been
	 * clobbered the first time str was freed, so is unsafe. */
        internal_error(file, line,
	    "dSfree: links==%d, data=\"%.32b\"", str->links, '\"', str->data);
        core("dSfree: links==%d", file, line, str->links);
    }

    if (str->charattrs) Sfree(str, str->charattrs);
    if (str->dynamic_data && str->data)
        Sfree(str, str->data);

    str->size = -42;  /* break lcheck if str is reused without dSinit */
    str->len = 0;
    if (str->dynamic_struct) {
        if (!str->dynamic_data)	/* str and data were alloced together */
            Sfree(str, str);
#if USE_MMALLOC
        else if (str->md)
            Sfree(str, str);
#endif
        else
            pfree_fl(str, Stringpool, data, file, line);
    }
}

String *dSadd(String *str, int c, const char *file, int line)
{
    str->len++;
    lcheck(str, file, line);
    str->data[str->len - 1] = c;
    str->data[str->len] = '\0';
    if (str->charattrs) {
        str->charattrs[str->len] = str->charattrs[str->len-1];
    }
    return str;
}

String *dSnadd(String *str, int c, int n, const char *file, int line)
{
    int oldlen = str->len;
    if (n < 0) core("dSnadd: n==%ld", file, line, (long)n);
    str->len += n;
    lcheck(str, file, line);
    for (n = oldlen; n < str->len; n++)
        str->data[n] = c;
    str->data[str->len] = '\0';
    if (str->charattrs) extend_charattrs(str, oldlen, 0);
    return str;
}

String *dStrunc(String *str, int len, const char *file, int line)
{
    /* if (str->size && str->len < len) return str; */
    unsigned int oldlen = str->len;
    str->len = len;
    lcheck(str, file, line);
    if (len <= oldlen) {
        str->data[len] = '\0';
    } else {
        str->len = oldlen;
    }
    return str;
}

String *dScpy(String *dest, const char *src, const char *file, int line)
{
    dest->len = strlen(src);
    if (dest->charattrs) {
        Sfree(dest, dest->charattrs);
        dest->charattrs = NULL;
    }
    lcheck(dest, file, line);
    memcpy(dest->data, src, dest->len + 1);
    return dest;
}

String *dSncpy(String *dest, const char *src, int n, const char *file, int line)
{
    int len = strlen(src);

    if (n < 0) core("dSncpy: n==%ld", file, line, (long)n);
    if (n > len) n = len;
    dest->len = n;
    if (dest->charattrs) {
        Sfree(dest, dest->charattrs);
        dest->charattrs = NULL;
    }
    lcheck(dest, file, line);
    memcpy(dest->data, src, n);
    dest->data[n] = '\0';
    return dest;
}

String *dSScpy(String *dest, const conString *src, const char *file, int line)
{
    if (dest->charattrs && !src->charattrs) {
        Sfree(dest, dest->charattrs);
        dest->charattrs = NULL;
    }
    dest->len = src->len;
    lcheck(dest, file, line);
    memcpy(dest->data, src->data ? src->data : "", src->len+1);
    if (src->charattrs) {
        check_charattrs(dest, 0, 0, file, line);
        memcpy(dest->charattrs, src->charattrs, sizeof(cattr_t) * (src->len+1));
    }
    dest->attrs = src->attrs;
    return dest;
}

String *dScat(String *dest, const char *src, const char *file, int line)
{
    int oldlen = dest->len;

    dest->len += strlen(src);
    lcheck(dest, file, line);
    memcpy(dest->data + oldlen, src, dest->len - oldlen + 1);
    if (dest->charattrs) extend_charattrs(dest, oldlen, 0);
    return dest;
}

String *dSSoncat(String *dest, const conString *src, int start, int len,
    const char *file, int line)
{
    int oldlen = dest->len;
    int i, j;
    cattr_t cattrs;

    if (len < 0)
        len = src->len - start;
    dest->len += len;
    lcheck(dest, file, line);
    memcpy(dest->data + oldlen, src->data ? src->data + start: "", len);
    dest->data[dest->len] = '\0';

    if (src->charattrs || dest->charattrs || src->attrs != dest->attrs) {
        if (dest->charattrs && dest->attrs) {
	    cattrs = attr2cattr(dest->attrs);
            for (i = 0; i < oldlen; i++)
                dest->charattrs[i] = adj_attr(cattrs, dest->charattrs[i]);
        } else {
            check_charattrs(dest, oldlen, dest->attrs, file, line);
        }
        dest->attrs = 0;

        if (src->charattrs && src->attrs) {
	    cattrs = attr2cattr(src->attrs);
	    for (i = oldlen, j = start; i < dest->len; i++, j++)
                dest->charattrs[i] = adj_attr(cattrs, src->charattrs[j]);
        } else if (src->charattrs) {
            memcpy(dest->charattrs + oldlen, src->charattrs + start,
                sizeof(cattr_t) * len);
        } else {
	    for (i = oldlen; i < dest->len; i++)
                dest->charattrs[i] = src->attrs;
        }
        dest->charattrs[dest->len] = 0;
    }

    return dest;
}

/* slow version of dSncat, verifies that length of input >= n */
String *dSncat(String *dest, const char *src, int n, const char *file, int line)
{
    int oldlen = dest->len;
    int len = strlen(src);

    if (n < 0) core("dSncat: n==%ld", file, line, (long)n);
    if (n > len) n = len;
    dest->len += n;
    lcheck(dest, file, line);
    memcpy(dest->data + oldlen, src, n);
    dest->data[dest->len] = '\0';
    if (dest->charattrs) extend_charattrs(dest, oldlen, 0);
    return dest;
}

/* fast version of dSncat, assumes length of input >= n */
String *dSfncat(String *dest, const char *src, int n, const char *file, int line)
{
    unsigned int oldlen = dest->len;

    if ((int)n < 0) core("dSfncat: n==%ld", file, line, (long)n);
    dest->len += n;
    lcheck(dest, file, line);
    memcpy(dest->data + oldlen, src, n);
    dest->data[dest->len] = '\0';
    if (dest->charattrs) extend_charattrs(dest, oldlen, 0);
    return dest;
}

String *Stringstriptrail(String *str)
{
    while (is_space(str->data[str->len - 1]))
	--str->len;
    str->data[str->len] = '\0';
    return str;
}

/* like strcmp for Strings, extended to cover their attrs too. */
int Stringcmp(const conString *s, const conString *t)
{
    int retval, i;
    attr_t s_attrs, t_attrs;
    if (!s && !t) return 0;
    if (!s && t) return -257;
    if (s && !t) return 257;
    if ((retval = strcmp(s->data, t->data)) != 0) return retval;
    if (!s->charattrs && !t->charattrs) return s->attrs - t->attrs;
    for (i = 0; i < s->len; i++) {
	s_attrs = s->charattrs ? adj_attr(s->attrs, s->charattrs[i]) : s->attrs;
	t_attrs = t->charattrs ? adj_attr(t->attrs, t->charattrs[i]) : t->attrs;
	if (s_attrs != t_attrs) return s_attrs - t_attrs;
    }
    return 0;
}

#if USE_DMALLOC
void free_dstring(void)
{
    pfreepool(String, Stringpool, data);
}
#endif

