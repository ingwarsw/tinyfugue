/*************************************************************************
 *  TinyFugue - programmable mud client
 *  Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2002, 2003, 2004, 2005, 2006-2007 Ken Keys
 *
 *  TinyFugue (aka "tf") is protected under the terms of the GNU
 *  General Public License.  See the file "COPYING" for details.
 ************************************************************************/
/* $Id: port.h,v 35004.38 2007/01/13 23:12:39 kkeys Exp $ */

#ifndef PORT_H
#define PORT_H

#ifdef __hpux__
# ifndef _HPUX_SOURCE
#  define _HPUX_SOURCE    /* Enables some "extensions" on HPUX. */
# endif
#endif

#if _AIX - 0
# ifndef _ALL_SOURCE
#  define _ALL_SOURCE     /* Enables some "extensions" on AIX. */
# endif
/* # define _BSD 44 */    /* Needed for nonblocking connect on AIX. */
#endif

#if 0  /* These cause a few problems, but little benefit, so forget it. */
/* These aren't neccessary, but may improve optimization, etc. */
# ifdef __GNUC__
#  define INLINE __inline__
#  if (__GNUC__ == 2 && __GNUC_MINOR__ >= 5) || (__GNUC__ > 2)
#   define GNUC_2_5_OR_LATER
#   define PURE   __attribute__ ((const))
#   define NORET  __attribute__ ((noreturn))
#  endif
# endif
#endif

#ifndef INLINE
# define INLINE
#endif
#ifndef PURE
# define PURE
#endif
#ifndef NORET
# define NORET
#endif

#if 0
# ifdef __GNUC__
#  define format_printf(fmt, var)     __attribute__((format(printf, fmt, var)))
# else
#  define format_printf(fmt, var)     /* empty */
# endif
#else
#  define format_printf(fmt, var)     /* empty */
#endif


/* standard stuff */

#include <errno.h>
#ifndef errno
extern int errno;  /* Some systems don't declare errno in errno.h. Duh. */
#endif

#include <stdio.h>

#ifndef SEEK_SET
# define SEEK_SET 0
#endif


#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifndef STDIN_FILENO
# define STDIN_FILENO        0
# define STDOUT_FILENO       1
# define STDERR_FILENO       2
#endif

#if STDC_HEADERS
# include <stdlib.h>
# include <string.h>
#else
extern void free();
# if HAVE_MEMORY_H
#  include <memory.h>
# endif
# if !HAVE_STRCHR
#  if HAVE_INDEX
#   define strchr index
#   define strrchr rindex    /* assumed */
#  endif
# endif
# if !HAVE_MEMCPY
#  if HAVE_BCOPY
#   define memcpy(dst, src, len) bcopy((src), (dst), (len))
#  endif
# endif
#endif


#if !HAVE_BZERO
  /* We don't use the nonstandard bzero(), but some stupid sys/select.h do */
# define bzero(ptr, size)    memset((ptr), '\0', (size))
#endif

/* Try the common case insensitive strcmp's before falling back to our own */
#if HAVE_STRCASECMP
# define cstrcmp   strcasecmp
#else
# if HAVE_STRICMP
#  define cstrcmp   stricmp
# else
#  if HAVE_STRCMPI
#   define cstrcmp   strcmpi
#  endif
# endif
#endif
#ifndef cstrcmp
extern int    cstrcmp(const char *s, const char *t);
#endif

#if !HAVE_STRERROR
extern int sys_nerr;
extern char *sys_errlist[];
# define strerror(n) (((n) > 0 && (n) < sys_nerr) ? sys_errlist[(n)] : \
    "unknown error")
#endif

#if !HAVE_FILENO  /* occurs on at least one pre-POSIX SVr3-like platform */
# ifdef PLATFORM_UNIX
#  define fileno(p)  ((p)->_file)
# else
   /* Who knows what it should be elsewhere; it should already exist. */
# endif
#endif

#ifdef PLATFORM_OS2
# define HAVE_GETCWD
# define getcwd _getcwd2   /* handles drive names */
# define chdir _chdir2     /* handles drive names */
#endif


#if !HAVE_LOCALE_H
# undef HAVE_SETLOCALE
#endif
#if !HAVE_SETLOCALE
# undef HAVE_LOCALE_H
#endif


#include <ctype.h>
#if STDC_HEADERS /* are tolower and toupper safe on non-letters, per ANSI? */
  /* more efficient */
# define lcase(x)  tolower((unsigned char)(x))
# define ucase(x)  toupper((unsigned char)(x))
#else
  /* will work even in nonstandard C */
  extern int lcase(int x);
  extern int ucase(int x);
  /* This expression evaluates its argument more than once:
   *  (is_upper(x) ? tolower(x) : (x))
   * This expression has no sequence points:
   *  (dummy=(x), (is_upper(dummy) ? tolower(dummy) : (dummy)))
   */
#endif

/* The standard ctype functions expect an int argument, containing either EOF
 * or an unsigned char representation of a character.  The functions below do
 * not allow EOF, but do work on plain char values.  (The signedness becomes
 * important when using character sets other than 7-bit ASCII.)
 */
#define is_alnum(c)	isalnum((unsigned char)(c))
#define is_alpha(c)	isalpha((unsigned char)(c))
#define is_cntrl(c)	iscntrl((unsigned char)(c))
#define is_digit(c)	isdigit((unsigned char)(c))
#define is_graph(c)	isgraph((unsigned char)(c))
#define is_lower(c)	islower((unsigned char)(c))
#define is_print(c)	isprint((unsigned char)(c))
#define is_punct(c)	ispunct((unsigned char)(c))
#define is_space(c)	isspace((unsigned char)(c))
#define is_upper(c)	isupper((unsigned char)(c))
#define is_xdigit(c)	isxdigit((unsigned char)(c))

/* RRAND(lo,hi) returns a random integer in the range [lo,hi].
 * RAND() returns a random integer in the range [0,TF_RAND_MAX].
 * SRAND() seeds the generator.
 * If random() exists, use it, because it is better than rand().
 * If not, we'll have to use rand(); if RAND_MAX isn't defined,
 * we'll have to use the modulus method instead of the division method.
 * Warning: RRAND() is undefined if hi < lo.
 * Warning: on Solaris 2.x, libucb contains a nonstandard rand() that does
 * not agree with RAND_MAX.  We must not link with -lucb.
 */

#if HAVE_SRANDOM
# include <math.h>
# define RAND()         (int)random()
# define SRAND(seed)    srandom(seed)
# define RRAND(lo,hi)   (RAND() % ((hi)-(lo)+1) + (lo))
#else
# if HAVE_SRAND
#  define RAND()         rand()
#  define SRAND(seed)    srand(seed)
#  if RAND_MAX
#   define RRAND(lo,hi)  ((hi)==(lo)) ? (hi) : \
                             ((RAND() / (RAND_MAX / ((hi)-(lo)+1) + 1)) + (lo))
#  else
#   define RRAND(lo,hi)  (RAND() % ((hi)-(lo)+1) + (lo))
#  endif
# else
   error "Don't have srand() or srandom()."
# endif
#endif


#if !HAVE_STRTOD
# define NO_FLOAT
#endif


#ifndef PATH_MAX
# ifdef MAXPATHLEN
#  define PATH_MAX MAXPATHLEN
# else
#  define PATH_MAX 1024
# endif
#endif


/* These just prevent warnings during development.  They should not be
 * used in production, since they might conflict with system headers.
 */
#ifdef TF_IRIX_DECLS
extern int  kill(pid_t, int);
extern int  ioctl(int, int, ...);
extern long random(void);
extern int  srandom(unsigned);
#endif
#ifdef TF_AIX_DECLS
extern int  strcasecmp(const char *, const char *);
extern time_t time(time_t *);
/* extern pid_t wait(int *); */
extern int socket(int, int, int);
extern int getsockopt(int, int, int, char *, int *);
extern int send(int, const char *, int, int);
extern int recv(int, char *, int, int);
extern int  ioctl(int, int, ...);
extern long random(void);
extern int  srandom(unsigned);
#endif

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#endif /* PORT_H */
