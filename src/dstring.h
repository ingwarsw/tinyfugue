/*************************************************************************
 *  TinyFugue - programmable mud client
 *  Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2002, 2003, 2004, 2005, 2006-2007 Ken Keys
 *
 *  TinyFugue (aka "tf") is protected under the terms of the GNU
 *  General Public License.  See the file "COPYING" for details.
 ************************************************************************/
/* $Id: dstring.h,v 35004.37 2007/01/13 23:12:39 kkeys Exp $ */

#ifndef DSTRING_H
#define DSTRING_H

#define ALLOCSIZE		(32L)

typedef struct String {
    char *data;			/* pointer to space holding text */
    int len;			/* length of actual data (not counting NUL) */
    int size;			/* length allocated */
    short links;		/* number of pointers to this structure */
    unsigned int static_struct: 1;	/* is struct static (not automatic)? */
    unsigned int dynamic_struct: 1;	/* was struct malloc'd? */
    unsigned int dynamic_data: 1;	/* was data malloc'd? */
    /* unsigned dynamic_charattrs:1; */	/* charattrs is always dynamic */
    unsigned int resizable: 1;		/* can data be resized? */
    attr_t attrs;		/* whole-line attributes */
    cattr_t *charattrs;		/* per-character attributes */
    struct timeval time;	/* timestamp */
#if USE_MMALLOC		/* don't waste the space if not using mmalloc */
    void *md;			/* mmalloc descriptor */
#endif
    const char *file;
    int line;
} String, Stringp[1];

/* A conString is a String with const *data and *charattrs. */
typedef struct conString {
    const char *data;		/* pointer to space holding text */
    int len;			/* length of actual data (not counting NUL) */
    int size;			/* length allocated */
    short links;		/* number of pointers to this structure */
    unsigned int static_struct: 1;	/* is struct static (not automatic)? */
    unsigned int dynamic_struct: 1;	/* was struct malloc'd? */
    unsigned int dynamic_data: 1;	/* was data malloc'd? */
    /* unsigned dynamic_charattrs:1; */	/* charattrs is always dynamic */
    unsigned int resizable: 1;		/* can data be resized? */
    attr_t attrs;		/* whole-line attributes */
    const cattr_t *charattrs;	/* per-character attributes */
    struct timeval time;	/* timestamp */
#if USE_MMALLOC		/* don't waste the space if not using mmalloc */
    void *md;			/* mmalloc descriptor */
#endif
    const char *file;
    int line;
} conString;

/* safely cast String* to conString* */
static inline conString *CS(String *s) { return (conString*)s; }

#if USE_MMALLOC
# define MD_INIT	NULL,
#else
# define MD_INIT	/* blank */
#endif

#define STRING_LITERAL_ATTR(data, attrs) \
    { (data), sizeof(data)-1, sizeof(data), 1, 1,0,0,0, \
	attrs, NULL, { -1, -1 }, MD_INIT __FILE__, __LINE__ }
#define STRING_LITERAL(data) STRING_LITERAL_ATTR(data, 0)
#define STRING_NULL \
    { NULL, 0, 0, 1, 1,0,0,0, \
	0, NULL, { -1, -1 }, MD_INIT __FILE__, __LINE__ }

/* AUTO_BUFFER: The structure is allocated automatically in a function's
 * (file's) scope; if it is used, it must be Stringfree()'d before
 * before function (program) exit, and it it safe to Stringfree() even if
 * it wasn't used.  Its data may be modifed and resized.
 */
# define AUTO_BUFFER(name) \
     Stringp (name) = {{ NULL, 0, 0, 1, 0,0,1,1, \
	0, NULL, { -1, -1 }, MD_INIT __FILE__, __LINE__ }}

/* STATIC_BUFFER: The structure has static storage, and its data is allocated
 * the first time it's needed, but not freed, and reused after that to save
 * time.  It never needs to be Stringfree()'d.  Its data may be
 * modified and resized.  Not reentrant-safe.
 */
#define STATIC_BUFFER_INIT \
    {{ NULL, 0, 0, 1, 1,0,1,1, 0, NULL, { -1,-1 }, MD_INIT __FILE__, __LINE__ }}
#define STATIC_BUFFER(name) \
    static Stringp (name) = STATIC_BUFFER_INIT

/* STATIC_STRING: The structure and data have static storage.  It can never
 * be modified or resized.
 */
#define STATIC_STRING(name, sl, attrs) \
    static conString (name)[1] = \
        {{ (sl), sizeof(sl)-1, sizeof(sl), 1, 1,0,0,0, \
	    (attrs), NULL, {-1,-1}, MD_INIT __FILE__, __LINE__ }}


/* String*init() sets links=1 to indicate the implicit ownership by whoever
 * created the structure.
 */
#define Stringdup(src) \
    SStringcpy(Stringnew(NULL, -1, 0), (src))
#define Stringodup(src, start) \
    SStringocat(Stringnew(NULL, -1, 0), (src), (start))
#define StringnewM(data, len, attrs, arena) \
    dSinit(NULL, (data), (len), (attrs), (arena), __FILE__, __LINE__)
#define Stringnew(data, len, attrs) \
    StringnewM(data, len, attrs, NULL)
#define Stringinit(str) \
    ((void)(dSinit((str), NULL, 0, 0, NULL, __FILE__, __LINE__)->links++))
#define Stringninit(str, size) \
    ((void)(dSinit((str), NULL, (size), 0, NULL, __FILE__, __LINE__)->links++))
#define Stringzero(str)		Stringninit((str), 0)

#define Stringfree(str)		Stringfree_fl(str, __FILE__, __LINE__)
#define Stringfree_fl(str, file, line) \
    do { \
	String *temp = (str);  /* must evaluate str exactly once */ \
	if (--temp->links <= 0) dSfree(temp, (file), (line)); \
    } while (0)

#define conStringfree(str)	conStringfree_fl(str, __FILE__, __LINE__)
#define conStringfree_fl(str, file, line) \
    do { \
	conString *temp = (str);  /* must evaluate str exactly once */ \
	if (--temp->links <= 0) dSfree((String*)temp, (file), (line)); \
    } while (0)

#define Stringadd(str, c)	dSadd((str), (c), __FILE__, __LINE__)
#define Stringnadd(str, c, n)	dSnadd((str), (c), (n), __FILE__, __LINE__)
#define Stringtrunc(str, n)	dStrunc((str), (n), __FILE__, __LINE__)
#define Stringcpy(dst, src)	dScpy((dst), (src), __FILE__, __LINE__)
#define SStringcpy(dst, src)	dSScpy((dst), (src), __FILE__, __LINE__)
#define Stringncpy(dst, src, n)	dSncpy((dst), (src), (n), __FILE__, __LINE__)
#define Stringcat(dst, src)	dScat((dst), (src), __FILE__, __LINE__)
#define Stringncat(dst, src, n)	dSncat((dst), (src), (n), __FILE__, __LINE__)
#define Stringfncat(dst, src, n) dSfncat((dst), (src), (n), __FILE__, __LINE__)

/* the following macros use -1 instead of src->len so src isn't evaled twice */
#define SStringcat(dst, src) \
	dSSoncat((dst), (src), 0, -1, __FILE__, __LINE__)
#define SStringncat(dst, src, len) \
	dSSoncat((dst), (src), 0, (len), __FILE__, __LINE__)
#define SStringocat(dst, src, start) \
	dSSoncat((dst), (src), (start), -1, __FILE__,__LINE__)
#define SStringoncat(dst, src, start, len) \
	dSSoncat((dst), (src), (start), (len), __FILE__, __LINE__)

#define FL	const char *file, int line

extern String *dSinit  (String *str, const char *data, int len,
               attr_t attrs, void *arena, FL);
extern void    dSfree  (String *str, FL);  /* call via Stringfree() */
extern String *dSadd   (String *str, int c, FL);
extern String *dSnadd  (String *str, int c, int n, FL);
extern String *dStrunc (String *str, int n, FL);
extern String *dScpy   (String *dest, const char *src, FL);
extern String *dSScpy  (String *dest, const conString *src, FL);
extern String *dSncpy  (String *dest, const char *src, int n, FL);
extern String *dScat   (String *dest, const char *src, FL);
extern String *dSSoncat(String *dest, const conString *src, int start, int len, FL);
extern String *dSncat  (String *dest, const char *src, int n, FL);
extern String *dSfncat (String *dest, const char *src, int n, FL);
extern String *Stringstriptrail(String *str);
extern int Stringcmp(const conString *s, const conString *t);

extern void check_charattrs(String *str, int n, cattr_t attrs,
    const char *file, int line);
extern void extend_charattrs(String *str, int oldlen, cattr_t attrs);

#if USE_DMALLOC
extern void free_dstring(void);
#endif


extern conString blankline[1];

#undef FL

#endif /* DSTRING_H */
