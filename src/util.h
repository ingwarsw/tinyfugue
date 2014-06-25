/*************************************************************************
 *  TinyFugue - programmable mud client
 *  Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2002, 2003, 2004, 2005, 2006-2007 Ken Keys
 *
 *  TinyFugue (aka "tf") is protected under the terms of the GNU
 *  General Public License.  See the file "COPYING" for details.
 ************************************************************************/
/* $Id: util.h,v 35004.63 2007/01/13 23:12:39 kkeys Exp $ */

#ifndef UTIL_H
#define UTIL_H

struct feature {
    const char *name;
    const int *flag;
};

#undef CTRL
/* convert to or from ctrl character */
#define CTRL(c)  (ucase(c) ^ '@')

/* map char to or from "safe" character set */
#define mapchar(c)    ((c) ? (c) & 0xFF : 0x80)
#define unmapchar(c)  ((char)(((c) == (char)0x80) ? 0x0 : (c)))

/* Map character into set allowed by locale */
#define localize(c)  ((is_print(c) || is_cntrl(c)) ? (c) : (c) & 0x7F)

/* Note STRNDUP works only if src[len] == '\0', ie. len == strlen(src) */
#define STRNDUP(src, len) \
    (strcpy(xmalloc(NULL, (len) + 1, __FILE__, __LINE__), (src)))
#define STRDUP(src)  STRNDUP((src), strlen(src))


#define IS_QUOTE	0x01
#define IS_STATMETA	0x02
#define IS_STATEND	0x04
#define IS_KEYSTART	0x08
#define IS_UNARY	0x10
#define IS_MULT		0x20
#define IS_ADDITIVE	0x40
#define IS_ASSIGNPFX	0x80

extern const struct timeval tvzero;
extern struct timeval mail_update;
extern int mail_count;
extern struct mail_info_s *maillist;
extern char tf_ctype[];
extern Stringp featurestr;
extern struct feature features[];

extern const int feature_256colors;
extern const int feature_core;
extern const int feature_float;
extern const int feature_ftime;
extern const int feature_history;
extern const int feature_IPv6;
extern const int feature_locale;
extern const int feature_MCCPv1;
extern const int feature_MCCPv2;
extern const int feature_process;
extern const int feature_SOCKS;
extern const int feature_ssl;
extern const int feature_subsecond;
extern const int feature_TZ;

#define is_quote(c)	(tf_ctype[(unsigned char)c] & IS_QUOTE)
#define is_statmeta(c)	(tf_ctype[(unsigned char)c] & IS_STATMETA)
#define is_statend(c)	(tf_ctype[(unsigned char)c] & IS_STATEND)
#define is_keystart(c)	(tf_ctype[(unsigned char)c] & IS_KEYSTART)
#define is_unary(c)	(tf_ctype[(unsigned char)c] & IS_UNARY)
#define is_mult(c)	(tf_ctype[(unsigned char)c] & IS_MULT)
#define is_additive(c)	(tf_ctype[(unsigned char)c] & IS_ADDITIVE)
#define is_assignpfx(c)	(tf_ctype[(unsigned char)c] & IS_ASSIGNPFX)

#define tvcmp(a, b) \
   (((a)->tv_sec != (b)->tv_sec) ? \
       ((a)->tv_sec - (b)->tv_sec) : \
       ((a)->tv_usec - (b)->tv_usec))

#if HAVE_GETTIMEOFDAY
# define gettime(p)	(gettimeofday(p, NULL))
#else
# define gettime(p)	((p)->tv_usec = 0, time(&(p)->tv_sec))
#endif

#define strtochr(s, ep)   ((char)(strtol((s), (char**)ep, 0) % 0x100))
#define strtoint(s, ep)   ((int)strtol((s), (char**)ep, 10))
#define strtolong(s, ep)  (strtol((s), (char**)ep, 10))
extern int    enum2int(const char *str, long val, conString *vec, const char *msg);
extern void   init_util1(void);
extern void   init_util2(void);
extern const conString* print_to_ascii(String *buf, const char *str);
extern const conString* ascii_to_print(const char *str);
extern char  *cstrchr(const char *s, int c);
extern char  *estrchr(const char *s, int c, int e);
extern int    numarg(const char **str);
extern int    nullstrcmp(const char *s, const char *t);
extern int    nullcstrcmp(const char *s, const char *t);
extern int    cstrncmp(const char *s, const char *t, size_t n);
extern char  *stringarg(char **str, const char **end);
extern int    stringliteral(struct String *dest, const char **str);
extern char  *stripstr(char *s);
extern void   startopt(const conString *args, const char *opts);
extern char   nextopt(const char **arg, ValueUnion *u, int *type, int *offp);
#if HAVE_TZSET
extern int    ch_timezone(Var *var);
#else
# define ch_timezone NULL
#endif
extern int    ch_locale(Var *var);
extern int    ch_mailfile(Var *var);
extern int    ch_maildelay(Var *var);
extern void   check_mail(void);

extern type_t string_arithmetic_type(const char *str, int typeset);
extern Value *parsenumber(const char *str, const char **caller_endp,
		int typeset, Value *val);
extern long   parsetime(const char *str, char **endp, int *istime);
extern void   abstime(struct timeval *tv);
extern void   append_usec(String *buf, long usec, int truncflag);
extern void   tftime(String *buf, const conString *fmt,
		const struct timeval *tv);
extern void   tvsub(struct timeval *a, const struct timeval *b,
		const struct timeval *c);
extern void   tvadd(struct timeval *a, const struct timeval *b,
		const struct timeval *c);
extern void   die(const char *why, int err) NORET;
#if USE_DMALLOC
extern void   free_util(void);
#endif

#endif /* UTIL_H */
