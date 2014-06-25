/* config.h.
 * Manually generated for OS/2 from information provided by A.Sahlbach.
 * Lines marked with '?' indicate I don't have information about that feature
 * for OS/2; for most of those, I assume a safe value.
 */
#define PLATFORM_OS2 1
#define HAVE_DRIVES 1

#define MAILDIR    ""

#define RETSIGTYPE void
/* #undef pid_t */ /* ? */
/* #undef size_t */ /* ? */
/* #undef time_t */

#define HAVE_UNISTD_H 1
/* #undef HAVE_SYS_WAIT_H */
#define STDC_HEADERS 1
#define HAVE_MEMORY_H 1
/* #undef HAVE_SYS_SELECT_H */
#define HAVE_LOCALE_H 1
#define NETINET_IN_H <netinet/in.h>
#define ARPA_INET_H <arpa/inet.h>
#define NETDB_H <netdb.h>
/* #undef HAVE_PWD_H */
/* #undef NEED_PTEM_H */
#define HAVE_STDARG_H 1
/* #undef HAVE_ZLIB_H */ /* ? */

#ifdef HAVE_ZLIB_H
/* #undef HAVE_LIBZ */ /* ? */
#endif

/* #undef HAVE_BCOPY */
/* #undef HAVE_BZERO */
#define HAVE_CONNECT 1 /* ? */
#define HAVE_FILENO 1
#define HAVE_GETCWD 1
#define HAVE_GETHOSTBYNAME 1
/* #undef HAVE_GETHOSTBYNAME2 */ /* ? */
/* #undef HAVE_GETIPNODEBYNAME */ /* ? */
/* #undef HAVE_GETPWNAM */
/* #undef HAVE_GETTIMEOFDAY */ /* ? */
/* #undef HAVE_GETWD */
#define HAVE_H_ERRNO 1
/* #undef HAVE_HSTRERROR */ /* Not all versions of OS/2 have this. -Ken Keys */
/* #undef HAVE_INDEX */
/* #undef HAVE_INET_ATON */ /* ? */
/* #undef HAVE_INET_PTON */ /* ? */
#define HAVE_KILL 1
#define HAVE_MEMCPY 1
#define HAVE_MEMSET 1
#define HAVE_RAISE 1
#define HAVE_SETLOCALE 1
#define HAVE_SIGACTION 1
#define HAVE_SRAND 1
/* #undef HAVE_SRANDOM */
/* #undef HAVE_STRCASECMP */
#define HAVE_STRCHR 1
/* #undef HAVE_STRCMPI */
#define HAVE_STRCSPN 1
#define HAVE_STRERROR 1
#define HAVE_STRFTIME 1
#define HAVE_STRICMP 1
#define HAVE_STRSTR 1
#define HAVE_STRTOD 1
#define HAVE_STRTOL 1
#define HAVE_TZSET 1
/* #undef HAVE_WAITPID */ /* ? */

#define CASE_OK 1

#define TERMCAP 1
/* #undef HARDCODE */
#define USE_TERMIOS 1
/* #undef USE_TERMIO */
/* #undef USE_SGTTY */

/* #undef ENABLE_INET6 */ /* ? */
/* #undef IN6_ADDR */
/* #undef NO_HISTORY */
/* #undef NO_PROCESS */
/* #undef NO_FLOAT */

/* #undef inline */

#define UNAME "OS/2"
/* LIBDIR is defined by os2make.cmd */

#define NDEBUG 1
#include <assert.h>
