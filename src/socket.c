/*************************************************************************
 *  TinyFugue - programmable mud client
 *  Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2002, 2003, 2004, 2005, 2006-2007 Ken Keys
 *
 *  TinyFugue (aka "tf") is protected under the terms of the GNU
 *  General Public License.  See the file "COPYING" for details.
 ************************************************************************/
static const char RCSid[] = "$Id: socket.c,v 35004.288 2007/01/13 23:12:39 kkeys Exp $";


/***************************************************************
 * Fugue socket handling
 *
 * Written by Ken Keys
 * Reception and transmission through sockets is handled here.
 * This module also contains the main loop.
 * Multiple sockets handled here.
 * Autologin handled here.
 ***************************************************************/

#include "tfconfig.h"
#include <sys/types.h>
#if HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif
/* #include <sys/time.h> */
#include <fcntl.h>
#include <sys/file.h>	/* for FNONBLOCK on SVR4, hpux, ... */
#include <sys/socket.h>
#include <signal.h>	/* for killing resolver child process */

#if HAVE_SSL
# include <openssl/ssl.h>
# include <openssl/err.h>
    SSL_CTX *ssl_ctx;
#endif

#ifdef NETINET_IN_H
# include NETINET_IN_H
#else
/* Last resort - we'll assume the "normal" stuff. */
struct in_addr {
	unsigned long	s_addr;
};
struct sockaddr_in {
	short		sin_family;
	unsigned short	sin_port;
	struct in_addr	sin_addr;
	char		sin_zero[8];
};
#define	htons(x)	(x)	/* assume big-endian machine */
#endif

#ifdef ARPA_INET_H
# include ARPA_INET_H
#endif

#if HAVE_MCCP
# include <zlib.h>
#endif

#include "port.h"
#include "tf.h"
#include "util.h"
#include "pattern.h"
#include "search.h"
#include "tfio.h"
#include "tfselect.h"
#include "history.h"
#include "world.h"
#include "socket.h"
#include "output.h"
#include "attr.h"
#include "process.h"
#include "macro.h"
#include "keyboard.h"
#include "cmdlist.h"
#include "command.h"
#include "signals.h"
#include "variable.h"	/* set_var_by_*() */

#ifdef _POSIX_VERSION
# include <sys/wait.h>
#endif

#if !defined(AF_INET6) || !defined(IN6_ADDR) || !HAVE_GETADDRINFO
# undef ENABLE_INET6
# define ENABLE_INET6 0
#endif

#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN 16
#endif
#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 46
#endif

#ifdef PLATFORM_OS2
# define NONBLOCKING_GETHOST
#endif

#ifdef PLATFORM_UNIX
# ifndef __CYGWIN32__
#  if HAVE_WAITPID
#   define NONBLOCKING_GETHOST
#  endif
# endif
#endif

#include NETDB_H

#if !HAVE_GAI_STRERROR || !defined(AI_NUMERICHOST) || !defined(EAI_SERVICE)
  /* System's implementation is incomplete.  Avoid it. */
# undef HAVE_GETADDRINFO
#endif

#define TF_EAI_ADDRFAMILY  -1 /* address family for hostname not supported */
#define TF_EAI_AGAIN       -2 /* temporary failure in name resolution */
#define TF_EAI_BADFLAGS    -3 /* invalid value for ai_flags */
#define TF_EAI_FAIL        -4 /* non-recoverable failure in name resolution */
#define TF_EAI_FAMILY      -5 /* ai_family not supported */
#define TF_EAI_MEMORY      -6 /* memory allocation failure */
#define TF_EAI_NODATA      -7 /* no address associated with hostname */
#define TF_EAI_NONAME      -8 /* hostname nor servname provided, or not known */
#define TF_EAI_SERVICE     -9 /* servname not supported for ai_socktype */
#define TF_EAI_SOCKTYPE   -10 /* ai_socktype not supported */
#define TF_EAI_SYSTEM     -11 /* system error returned in errno */
#define TF_EAI_BADHINTS   -12
#define TF_EAI_PROTOCOL   -13
#define TF_EAI_MAX        -14

static const char *tf_gai_errlist[] = {
    "Error 0",
    "address family for hostname not supported",
    "temporary failure in name resolution",
    "invalid value for ai_flags",
    "non-recoverable failure in name resolution",
    "ai_family not supported",
    "memory allocation failure",
    "no address associated with hostname",
    "hostname nor servname provided, or not known",
    "unknown service (port) name",
    "ai_socktype not supported",
    "system error returned in errno",
};

#if HAVE_GETADDRINFO
/* The standard message for EAI_SERVICE is horrible.  We override it. */
# define gai_strerror(err) \
    ((err) == EAI_SERVICE ? tf_gai_errlist[-TF_EAI_SERVICE] : (gai_strerror)(err))

#else /* !HAVE_GETADDRINFO */
/* Partial implementation of getaddrinfo() and friends */
# define addrinfo tfaddrinfo
# define getaddrinfo tfgetaddrinfo
# define freeaddrinfo tffreeaddrinfo

struct tfaddrinfo {
    int     ai_flags;     /* AI_PASSIVE, AI_CANONNAME, AI_NUMERICHOST */
    int     ai_family;    /* PF_xxx */
    int     ai_socktype;  /* SOCK_xxx */
    int     ai_protocol;  /* 0 or IPPROTO_xxx for IPv4 and IPv6 */
    size_t  ai_addrlen;   /* length of ai_addr */
    char   *ai_canonname; /* canonical name for nodename */
    struct sockaddr  *ai_addr; /* binary address */
    struct addrinfo  *ai_next; /* next structure in linked list */
};

#define AI_NUMERICHOST  0x00000004 /* prevent name resolution */
#define AI_ADDRCONFIG   0x00000400 /* only if any address is assigned */

# if !HAVE_HSTRERROR
static const char *h_errlist[] = {
    "Error 0",
    "Unknown host",
    "Host name lookup failure",
    "Unknown server error",
    "No address associated with name"
};
#  define hstrerror(err)  ((err) <= 4 ? h_errlist[(err)] : "unknown error")
# endif /* !HAVE_HSTRERROR */

#if HAVE_H_ERRNO
  /* extern int h_errno; */ /* this could conflict */
#elif !defined(h_errno)
# define h_errno 1
#endif

# define gai_strerror(err) ((err) < 0 ? tf_gai_errlist[-(err)] : hstrerror(err))

#endif /* !HAVE_GETADDRINFO */

#ifdef NONBLOCKING_GETHOST
# ifndef PLATFORM_OS2
#  include <sys/uio.h> /* child uses writev() */
# endif
  static void waitforhostname(int fd, const char *name, const char *port);
  static int nonblocking_gethost(const char *name, const char *port,
      struct addrinfo **addrs, pid_t *pidp, const char **what);
#endif

#ifndef INADDR_NONE
# define INADDR_NONE 0xffffffff     /* should be in <netinet/in.h> */
#endif


/* Nonblocking connect.
 * Legend for comment columns:
 *      W = works, N = not defined, F = defined but fails, ? = unknown
 * Nonblocking connect will work on a system if the column contains a 'W'
 * and there is no 'F' above it; 'N' does not matter.  The order of the
 * tests is arranged to keep the 'F's below the 'W's.
 * 
 *                                                        S
 *                                                        o
 *                                      P           L  S  l     H
 *                                      O     S     i  u  a  I  P
 *                                      S  B  Y  A  n  n  r  R  /
 *                                      I  S  S  I  u  O  i  I  U
 *                                      X  D  V  X  x  S  s  X  X
 */
#ifdef FNONBLOCK                     /* N  ?  W  W  N  ?  W  W  W */
# define TF_NONBLOCK FNONBLOCK
#else
# ifdef O_NONBLOCK                   /* W  ?  ?  W  W  ?  W  W  ? */
#  define TF_NONBLOCK O_NONBLOCK
# else
#  ifdef FNDELAY                     /* ?  W  ?  F  W  ?  W  W  ? */
#   define TF_NONBLOCK FNDELAY
#  else
#   ifdef O_NDELAY                   /* ?  W  ?  F  W  ?  W  W  ? */
#    define TF_NONBLOCK O_NDELAY
#   else
#    ifdef FNBIO                     /* ?  ?  W  N  N  F  N  N  ? */
#     define TF_NONBLOCK FNBIO
#    else
#     ifdef FNONBIO                  /* ?  ?  ?  N  N  F  N  N  ? */
#      define TF_NONBLOCK FNONBIO
#     else
#      ifdef FNONBLK                 /* ?  ?  ?  N  N  ?  N  W  ? */
#       define TF_NONBLOCK FNONBLK
#      else
#       define TF_NONBLOCK 0
#      endif
#     endif
#    endif
#   endif
#  endif
# endif
#endif


#ifdef TF_AIX_DECLS
extern int connect(int, struct sockaddr *, int);
#endif

#if SOCKS
# ifndef SOCKS_NONBLOCK
#  define TF_NONBLOCK 0
# endif
#endif

/* connection states */
typedef enum {
    SS_NEW,		/* brand spanking new */
    SS_RESOLVING,	/* hostname resolution is pending */
    SS_RESOLVED,	/* hostname resolution is complete */
    SS_CONNECTING,	/* connection is pending */
    SS_CONNECTED,	/* connection is established */
    SS_OPEN,		/* open, without a connection */
    SS_ZOMBIE,		/* connection dead, but some text may be unseen */
    SS_DEAD		/* connection dead, and all text has been seen */
} constate_t;

/* flags */
#define SOCKLOGIN	0x01	/* autologin requested by user */
#define SOCKPROMPT	0x02	/* last prompt was definitely a prompt */
#define SOCKPROXY	0x04	/* indirect connection through proxy server */
#define SOCKTELNET	0x08	/* server supports telnet protocol */
#define SOCKMAYTELNET	0x10	/* server might support telnet protocol */
#define SOCKCOMPRESS	0x20	/* server has enabled MCCP v1 or v2 */
#define SOCKALLOCADDRS	0x40	/* addrs allocated by tf, not getaddrinfo */
#define SOCKECHO	0x80	/* receive all sent text (loopback) */

VEC_TYPEDEF(telnet_opts, 256);

typedef struct Sock {		/* an open connection to a server */
    int fd;			/* socket to server, or pipe to name resolver */
    const char *host, *port;	/* server address, human readable */
    struct addrinfo *addrs;	/* possible server addresses */
    struct addrinfo *addr;	/* actual server address */
    const char *myhost;		/* explicit client address, human readable */
    struct addrinfo *myaddr;	/* explicit client address */
    telnet_opts tn_us;		/* our telnet options */
    telnet_opts tn_us_tog;	/* our telnet options we want changed */
    telnet_opts tn_them;	/* server's telnet options */
    telnet_opts tn_them_tog;	/* server's telnet options we want changed */
    constate_t constate;	/* connection state */
    unsigned char flags;	/* SOCK* flags */
    short numquiet;		/* # of lines to gag after connecting */
    struct World *world;	/* world to which socket is connected */
    struct Sock *next, *prev;	/* next/prev sockets in linked list */
    Stringp buffer;		/* buffer for incoming characters */
    Stringp subbuffer;		/* buffer for incoming characters */
    Queue queue;		/* queue of incoming lines */
    conString *prompt;		/* prompt from server */
    struct timeval prompt_timeout; /* when does unterm'd line become a prompt */
    int ttype;			/* index into enum_ttype[] */
    attr_t attrs;		/* current text attributes */
    attr_t prepromptattrs;	/* text attributes before implicit prompt */
    unsigned long alert_id;	/* id of last alert on this socket */
    struct timeval time[2];	/* time of last receive/send */
    char fsastate;		/* parser finite state automaton state */
    char substate;		/* parser fsa state for telnet subnegotiation */
    pid_t pid;			/* OS pid of name resolution process */
#if HAVE_MCCP
    z_stream *zstream;		/* state of compressed stream */
#endif
#if HAVE_SSL
    SSL *ssl;			/* SSL state */
#endif
} Sock;

typedef struct {
    int err;
    size_t size;
} nbgai_hdr_t;

static Sock *find_sock(const char *name);
static void  wload(World *w);
static int   fg_sock(Sock *sock, int quiet);
static int   get_host_address(Sock *sock, const char **what, int *errp);
#if !HAVE_GETADDRINFO
static int   tfgetaddrinfo(const char *nodename, const char *port,
	     const struct addrinfo *hints, struct addrinfo **res);
#endif
static int   opensock(World *world, int flags);
static int   openconn(Sock *new);
static int   establish(Sock *new);
#if 0
static void  fg_live_sock(void);
#endif
static void  nuke_dead_socks(void);
static void  nukesock(Sock *sock);
static void  handle_prompt(String *str, int offset, int confirmed);
static void  handle_implicit_prompt(void);
static void  unprompt(Sock *sock, int update);
static void  test_prompt(void);
static void  schedule_prompt(Sock *sock);
static void  handle_socket_lines(void);
static int   handle_socket_input(const char *simbuffer, int simlen);
static int   transmit(const char *s, unsigned int len);
static void  telnet_send(String *cmd);
static void  telnet_subnegotiation(void);
static void  f_telnet_recv(int cmd, int opt);
static int   is_quiet(const char *str);
static int   is_bamf(const char *str);
static void  do_naws(Sock *sock);
static void  telnet_debug(const char *dir, const char *str, int len);
static void  preferred_telnet_options(void);
static void  killsock(Sock *sock);

#define zombiesock(sock)	killsock(sock)
#define flushxsock() \
    do { if (xsock->queue.list.head) handle_socket_lines(); } while (0)

#define telnet_recv(cmd, opt)	f_telnet_recv((UCHAR)cmd, (UCHAR)opt);
#define no_reply(str) telnet_debug("sent", "[no reply (" str ")", 0)

#ifndef CONN_WAIT
#define CONN_WAIT 400000
#endif

#ifndef PROC_WAIT
#define PROC_WAIT 100000
#endif

#define SPAM (4*1024)		/* break loop if this many chars are received */

static fd_set readers;		/* input file descriptors */
static fd_set active;		/* active file descriptors */
static fd_set writers;		/* pending connections */
static fd_set connected;	/* completed connections */
static unsigned int nfds;	/* max # of readers/writers */
static Sock *hsock = NULL;	/* head of socket list */
static Sock *tsock = NULL;	/* tail of socket list */
static Sock *fsock = NULL;	/* foreground socket */
static int dead_socks = 0;	/* Number of unnuked dead sockets */
static int socks_with_lines = 0;/* Number of socks with queued received lines */
static struct timeval prompt_timeout = {0,0};
static const char *telnet_label[0x100];
STATIC_BUFFER(telbuf);

#define MAXQUIET        25	/* max # of lines to suppress during login */

/* Note: many telnet servers send DO ECHO and DO SGA together to mean
 * character-at-a-time mode.
 */

/* TELNET special characters (RFC 854) */
#define TN_EOR		((char)239)	/* end of record (RFC 885) */
#define TN_SE		((char)240)	/* subnegotiation end */
#define TN_NOP		((char)241)	/* no operation */
#define TN_DATA_MARK	((char)242)	/* (not used) */
#define TN_BRK		((char)243)	/* break (not used) */
#define TN_IP		((char)244)	/* interrupt process (not used) */
#define TN_AO		((char)245)	/* abort output (not used) */
#define TN_AYT		((char)246)	/* are you there? (not used) */
#define TN_EC		((char)247)	/* erase character (not used) */
#define TN_EL		((char)248)	/* erase line (not used) */
#define TN_GA		((char)249)	/* go ahead */
#define TN_SB		((char)250)	/* subnegotiation begin */
#define TN_WILL		((char)251)	/* I offer to ~, or ack for DO */
#define TN_WONT		((char)252)	/* I will stop ~ing, or nack for DO */
#define TN_DO		((char)253)	/* Please do ~?, or ack for WILL */
#define TN_DONT		((char)254)	/* Stop ~ing!, or nack for WILL */
#define TN_IAC		((char)255)	/* telnet Is A Command character */

/* TELNET options (RFC 855) */		/* RFC# - description */
#define TN_BINARY	((char)0)	/*  856 - transmit binary */
#define TN_ECHO		((char)1)	/*  857 - echo */
#define TN_SGA		((char)3)	/*  858 - suppress GA (not used) */
#define TN_STATUS	((char)5)	/*  859 - (not used) */
#define TN_TIMING_MARK	((char)6)	/*  860 - (not used) */
#define TN_TTYPE	((char)24)	/* 1091 - terminal type */
#define TN_SEND_EOR	((char)25)	/*  885 - transmit EOR */
#define TN_NAWS		((char)31)	/* 1073 - negotiate about window size */
#define TN_TSPEED	((char)32)	/* 1079 - terminal speed (not used) */
#define TN_FLOWCTRL	((char)33)	/* 1372 - (not used) */
#define TN_LINEMODE	((char)34)	/* 1184 - (not used) */
#define TN_XDISPLOC	((char)35)	/* 1096 - (not used) */
#define TN_ENVIRON	((char)36)	/* 1408 - (not used) */
#define TN_AUTH		((char)37)	/* 1416 - (not used) */
#define TN_NEW_ENVIRON	((char)39)	/* 1572 - (not used) */
#define TN_CHARSET	((char)42)	/* 2066 - (not used) */
/* 85 & 86 are not standard.  See http://www.randomly.org/projects/MCCP/ */
#define TN_COMPRESS	((char)85)	/* MCCP v1 */
#define TN_COMPRESS2	((char)86)	/* MCCP v2 */

#define UCHAR		unsigned char

#define tn_send_opt(cmd, opt) \
    ( Sprintf(telbuf, "%c%c%c", TN_IAC, (cmd), (opt)), telnet_send(telbuf) )

#define TELOPT(sock, field, opt) \
	VEC_ISSET((UCHAR)(opt), &(sock)->tn_ ## field)
#define SET_TELOPT(sock, field, opt) \
	VEC_SET((UCHAR)(opt), &(sock)->tn_ ## field)
#define CLR_TELOPT(sock, field, opt) \
	VEC_CLR((UCHAR)(opt), &(sock)->tn_ ## field)

#define DO(opt)		( tn_send_opt(TN_DO,   (opt)) )
#define DONT(opt)	( tn_send_opt(TN_DONT, (opt)) )
#define WILL(opt)	( tn_send_opt(TN_WILL, (opt)) )
#define WONT(opt)	( tn_send_opt(TN_WONT, (opt)) )

#define ANSI_CSI	'\233'	/* ANSI terminal Command Sequence Intro */

#if HAVE_MCCP /* hack for broken MCCPv1 subnegotiation */
char mccp1_subneg[] = { TN_IAC, TN_SB, TN_COMPRESS, TN_WILL, TN_SE };
#endif

Sock *xsock = NULL;		/* current (transmission) socket */
int quit_flag = FALSE;		/* Are we all done? */
int active_count = 0;		/* # of (non-current) active sockets */
String *incoming_text = NULL;
const int feature_IPv6 = ENABLE_INET6 - 0;
const int feature_MCCPv1 = HAVE_MCCP - 0;
const int feature_MCCPv2 = HAVE_MCCP - 0;
const int feature_ssl = HAVE_SSL - 0;
const int feature_SOCKS = SOCKS - 0;

static const char *CONFAIL_fmt = "%% Connection to %s failed: %s: %s";
static const char *ICONFAIL_fmt = "%% Intermediate connection to %s failed: %s: %s";

#define ICONFAIL_AI(sock, why) \
    ICONFAIL((sock), printai((sock)->addr, NULL), (why))

#define CONFAILHP(sock, why) \
do { \
    do_hook(H_CONFAIL, "%% Unable to connect to %s: %s %s: %s", "%s %s %s: %s", \
        (sock)->world->name, (sock)->world->host, (sock)->world->port, (why)); \
    oflush(); \
} while (0)

#define CONFAIL(sock, what, why) \
do { \
    do_hook(H_CONFAIL, CONFAIL_fmt, "%s %s %s", \
	(sock)->world->name, (what), (why)); \
    oflush(); \
} while (0)

#define DISCON(where, what, why) \
    do_hook(H_DISCONNECT, "%% Connection to %s closed: %s: %s", "%s %s: %s", \
	where, what, why)

#if HAVE_SSL
static void ssl_err(const char *str)
{
    unsigned long e;
    while ((e = ERR_get_error()))
	eprintf("%s: %s", str, ERR_error_string(e, NULL));
}

static void ssl_io_err(Sock *sock, int ret, int hook)
{
    /* NB: sock->addr may have already been changed by setupnextconn() */
    int err;
    const char *fmt;

    flushxsock();

    switch (hook) {
    case H_CONFAIL: fmt = CONFAIL_fmt; break;
    case H_ICONFAIL: fmt = ICONFAIL_fmt; break;
    case H_DISCONNECT: fmt = "%% Connection to %s closed: %s: %s"; break;
    }

#define ssl_io_err_hook(what, why) \
    do_hook(hook, fmt, "%s %s %s", xsock->world->name, what, why);

    err = SSL_get_error(sock->ssl, ret);
    switch (err) {
    case SSL_ERROR_NONE:
	break;
    case SSL_ERROR_ZERO_RETURN:
	ssl_io_err_hook("SSL", "SSL_ERROR_ZERO_RETURN");
	break;
    case SSL_ERROR_WANT_READ:
	ssl_io_err_hook("SSL", "SSL_ERROR_WANT_READ");
	break;
    case SSL_ERROR_WANT_WRITE:
	ssl_io_err_hook("SSL", "SSL_ERROR_WANT_WRITE");
	break;
    case SSL_ERROR_WANT_CONNECT:
	ssl_io_err_hook("SSL", "SSL_ERROR_WANT_CONNECT");
	break;
    case SSL_ERROR_SYSCALL:
	if (ret == 0) {
	    ssl_io_err_hook("SSL/system", "invalid EOF");
	} else if (ret == -1) {
	    ssl_io_err_hook("SSL/system", strerror(errno));
	} else {
	    while ((err = ERR_get_error())) {
		ssl_io_err_hook("SSL/system", ERR_error_string(err, NULL));
	    }
	}
	break;
    case SSL_ERROR_SSL:
	while ((err = ERR_get_error())) {
	    ssl_io_err_hook("SSL/lib", ERR_error_string(err, NULL));
	}
	break;
    }
}

static void init_ssl(void)
{
    SSL_load_error_strings();
    SSL_library_init();
    /* XXX seed PRNG */
    ssl_ctx = SSL_CTX_new(SSLv23_client_method());
    if (!ssl_ctx) {
	ssl_err("SSL_CTX_new");
	return;
    }
    if (!SSL_CTX_set_cipher_list(ssl_ctx, "ALL")) {
	ssl_err("SSL_CTX_set_cipher_list");
	return;
    }
}
#endif

/* initialize socket.c data */
void init_sock(void)
{
    int i;

    FD_ZERO(&readers);
    FD_ZERO(&active);
    FD_ZERO(&writers);
    FD_ZERO(&connected);
    FD_SET(STDIN_FILENO, &readers);
    nfds = 1;

    set_var_by_id(VAR_async_conn, !!TF_NONBLOCK);
#ifdef NONBLOCKING_GETHOST
    set_var_by_id(VAR_async_name, 1);
#endif

    for (i = 0; i < 0x100; i++) telnet_label[i] = NULL;

    telnet_label[(UCHAR)TN_BINARY]	= "BINARY";
    telnet_label[(UCHAR)TN_ECHO]	= "ECHO";
    telnet_label[(UCHAR)TN_SGA]		= "SUPPRESS-GO-AHEAD";
    telnet_label[(UCHAR)TN_STATUS]	= "STATUS";
    telnet_label[(UCHAR)TN_TIMING_MARK]	= "TIMING-MARK";
    telnet_label[(UCHAR)TN_TTYPE]	= "TERMINAL-TYPE";
    telnet_label[(UCHAR)TN_SEND_EOR]	= "SEND-EOR";
    telnet_label[(UCHAR)TN_NAWS]	= "NAWS";
    telnet_label[(UCHAR)TN_TSPEED]	= "TSPEED";
    telnet_label[(UCHAR)TN_FLOWCTRL]	= "TOGGLE-FLOW-CONTROL";
    telnet_label[(UCHAR)TN_LINEMODE]	= "LINEMODE";
    telnet_label[(UCHAR)TN_XDISPLOC]	= "XDISPLOC";
    telnet_label[(UCHAR)TN_ENVIRON]	= "ENVIRON";
    telnet_label[(UCHAR)TN_AUTH]	= "AUTHENTICATION";
    telnet_label[(UCHAR)TN_NEW_ENVIRON]	= "NEW-ENVIRON";
    telnet_label[(UCHAR)TN_CHARSET]	= "CHARSET";
    telnet_label[(UCHAR)TN_COMPRESS]	= "COMPRESS";
    telnet_label[(UCHAR)TN_COMPRESS2]	= "COMPRESS2";
    telnet_label[(UCHAR)TN_EOR]		= "EOR";
    telnet_label[(UCHAR)TN_SE]		= "SE";
    telnet_label[(UCHAR)TN_NOP]		= "NOP";
    telnet_label[(UCHAR)TN_DATA_MARK]	= "DATA-MARK";
    telnet_label[(UCHAR)TN_BRK]		= "BRK";
    telnet_label[(UCHAR)TN_IP]		= "IP";
    telnet_label[(UCHAR)TN_AO]		= "AO";
    telnet_label[(UCHAR)TN_AYT]		= "AYT";
    telnet_label[(UCHAR)TN_EC]		= "EC";
    telnet_label[(UCHAR)TN_EL]		= "EL";
    telnet_label[(UCHAR)TN_GA]		= "GA";
    telnet_label[(UCHAR)TN_SB]		= "SB";
    telnet_label[(UCHAR)TN_WILL]	= "WILL";
    telnet_label[(UCHAR)TN_WONT]	= "WONT";
    telnet_label[(UCHAR)TN_DO]		= "DO";
    telnet_label[(UCHAR)TN_DONT]	= "DONT";
    telnet_label[(UCHAR)TN_IAC]		= "IAC";

#if HAVE_SSL
    init_ssl();
#endif
}

#define set_min_earliest(tv) \
    do { \
	if (earliest.tv_sec == 0 || (tvcmp(&tv, &earliest) < 0)) { \
	    earliest = tv; \
	} \
    } while (0)

/* main_loop
 * Here we mostly sit in select(), waiting for something to happen.
 * The select timeout is set for the earliest process, mail check,
 * or refresh event.  Signal processing and garbage collection is
 * done at the beginning of each loop, where we're in a "clean" state.
 */
void main_loop(void)
{
    static struct timeval now, earliest;    /* static, for recursion */
    Sock *sock = NULL;		/* loop index */
    static int count;		/* select count; remembered across recursion */
    int received;
    Sock *stopsock;
    static int depth = 0;
    struct timeval tv, *tvp;
    struct timeval refresh_tv;
    STATIC_STRING(low_memory_msg,
	"% WARNING: memory is low.  Try reducing history sizes.", 0);

    depth++;
    while (!quit_flag) {
        if (depth > 1 && interrupted()) break;

        /* deal with pending signals */
        /* at loop beginning in case of signals before main_loop() */ 
        process_signals();

        /* run processes */
        /* at loop beginning in case of processes before main_loop() */ 
        gettime(&now);
        if (proctime.tv_sec && tvcmp(&proctime, &now) <= 0)
	    runall(0, NULL); /* run timed processes */

        if (low_memory_warning) {
            low_memory_warning = 0;
	    tfputline(low_memory_msg, tferr);
        }

        if (quit_flag) break;

        /* figure out when next event is so select() can timeout then */
        gettime(&now);
        earliest = proctime;
#if 1 /* XXX debugging */
	{
	    Sock *s;
	    int n = 0;
	    for (s = hsock; s; s = s->next)
		if (s->queue.list.head) n++;
	    if (n != socks_with_lines) {
		internal_error(__FILE__, __LINE__,
		    "socks_with_lines (%d) is not correct (%d)!",
		    socks_with_lines, n);
		socks_with_lines = n;
	    }
	}
#endif
	if (socks_with_lines)
	    earliest = now;
        if (maillist && tvcmp(&maildelay, &tvzero) > 0) {
            if (tvcmp(&now, &mail_update) >= 0) {
                check_mail();
                tvadd(&mail_update, &now, &maildelay);
            }
	    set_min_earliest(mail_update);
        }
        if (visual && alert_timeout.tv_sec > 0) {
            if (tvcmp(&alert_timeout, &now) < 0) {
		clear_alert();
	    } else {
		set_min_earliest(alert_timeout);
            }
	}
        if (visual && clock_update.tv_sec > 0) {
            if (now.tv_sec >= clock_update.tv_sec)
                update_status_field(NULL, STAT_CLOCK);
	    set_min_earliest(clock_update);
        }
        if (prompt_timeout.tv_sec > 0) {
	    set_min_earliest(prompt_timeout);
	}

        /* flush pending display_screen output */
        /* must be after all possible output and before select() */
        oflush();

        if (pending_input || pending_line) {
            tvp = &tv;
            tv = tvzero;
        } else if (earliest.tv_sec) {
            tvp = &tv;
            if (tvcmp(&earliest, &now) <= 0) {
                tv = tvzero;
            } else {
                tvsub(&tv, &earliest, &now);
#if !HAVE_GETTIMEOFDAY
                /* We can't read microseconds, so we get within the right
                 * second and then start polling. */
                if (proctime.tv_sec) {
                    if ((--tv.tv_sec) == 0)
                        tv.tv_usec = PROC_WAIT;
                }
#endif
            }
        } else tvp = NULL;

        if (need_refresh) {
            if (!visual) {
                refresh_tv.tv_sec = refreshtime / 1000000;
                refresh_tv.tv_usec = refreshtime % 1000000;
            } else {
                refresh_tv = tvzero;
            }

            if (!tvp || refresh_tv.tv_sec < tvp->tv_sec ||
                (refresh_tv.tv_sec == tvp->tv_sec &&
                refresh_tv.tv_usec < tvp->tv_usec))
            {
                tvp = &refresh_tv;
            }
        }

        if (visual && need_more_refresh) {
            if (!tvp || 1 < tvp->tv_sec ||
                (1 == tvp->tv_sec && 0 < tvp->tv_usec))
            {
                refresh_tv.tv_sec = 1;
                refresh_tv.tv_usec = 0;
                tvp = &refresh_tv;
            }
        }

        /* Wait for next event.
         *   descriptor read:	user input, socket input, or /quote !
         *   descriptor write:	nonblocking connect()
         *   timeout:		time for runall() or do_refresh()
         * Note: if the same descriptor appears in more than one fd_set, some
         * systems count it only once, some count it once for each occurance.
         */
        active = readers;
        connected = writers;
        count = tfselect(nfds, &active, &connected, NULL, tvp);

        if (count < 0) {
            /* select() must have exited due to error or interrupt. */
            if (errno != EINTR) core(strerror(errno), __FILE__, __LINE__, 0);
            /* In case we're in a kb tfgetS(), clear things for parent loop. */
            FD_ZERO(&active);
            FD_ZERO(&connected);
	    /* In case the dreaded solaris select bug caused tf to remove stdin
	     * from readers, user will probably panic and hit ^C, so we add
	     * stdin back to readers, and recover. */
	    FD_SET(STDIN_FILENO, &readers);

        } else {
            if (count == 0) {
                /* select() must have exited due to timeout. */
                do_refresh();
            }

            /* check for user input */
            if (pending_input || FD_ISSET(STDIN_FILENO, &active)) {
                if (FD_ISSET(STDIN_FILENO, &active)) count--;
                do_refresh();
                if (!handle_keyboard_input(FD_ISSET(STDIN_FILENO, &active))) {
                    /* input is at EOF, stop reading it */
                    FD_CLR(STDIN_FILENO, &readers);
                }
            }

            /* Check for socket completion/activity.  We pick up where we
             * left off last time, so sockets near the end of the list aren't
             * starved.  We stop when we've gone through the list once, or
             * when we've received a lot of data (so spammy sockets don't
             * degrade interactive response too much).
             */
            if (hsock) {
                received = 0;
                if (!sock) sock = hsock;
                stopsock = sock;
		/* note: count may have been zeroed by nested main_loop */
                while (count > 0 || socks_with_lines > 0 ||
		    prompt_timeout.tv_sec > 0)
		{
                    xsock = sock;
                    if (sock->constate >= SS_OPEN) {
                        /* do nothing */
                    } else if (FD_ISSET(xsock->fd, &connected)) {
                        count--;
                        establish(xsock);
                    } else if (FD_ISSET(xsock->fd, &active)) {
                        count--;
                        if (xsock->constate == SS_RESOLVING) {
                            openconn(xsock);
                        } else if (xsock->constate == SS_CONNECTING) {
                            establish(xsock);
                        } else if (xsock == fsock || background) {
                            received += handle_socket_input(NULL, 0);
                        } else {
                            FD_CLR(xsock->fd, &readers);
                        }
                    }
		    if (xsock->queue.list.head)
			handle_socket_lines();

		    /* If there's a partial line that's past prompt_timeout,
		     * make it a prompt. */
		    if (xsock->prompt_timeout.tv_sec) {
			gettime(&now);
			if (tvcmp(&xsock->prompt_timeout, &now) < 0) {
			    handle_implicit_prompt();
			}
		    }

                    sock = sock->next ? sock->next : hsock;
		    if (sock == stopsock || received >= SPAM) break;
                }

                /* fsock and/or xsock may have changed during loop above. */
                xsock = fsock;

		if (prompt_timeout.tv_sec > 0) {
		    /* reset global prompt timeout */
		    Sock *psock;
		    prompt_timeout = tvzero;
		    for (psock = hsock; psock; psock = psock->next) {
			schedule_prompt(psock);
		    }
		}
            }

#if !NO_PROCESS
            /* other active fds must be from command /quotes. */
            if (count) gettime(&proctime);
#endif
        }

        if (pending_line && read_depth) {    /* end of tf read() */
            pending_line = FALSE;
            break;
        }

#if 0
        if (fsock && fsock->constate >= SS_ZOMBIE && auto_fg &&
	    !(fsock->world->screen->nnew || fsock->queue.list.head))
	{
            fg_live_sock();
        }
#endif

        /* garbage collection */
        if (depth == 1) {
            if (sock && sock->constate == SS_DEAD) sock = NULL;
            if (dead_socks) nuke_dead_socks(); /* at end in case of quitdone */
            nuke_dead_macros();
            nuke_dead_procs();
        }
    }

    /* end of loop */
    if (!--depth) {
	fsock = NULL;
	xsock = NULL;
        while (hsock) nukesock(hsock);
#if HAVE_SSL
	SSL_CTX_free(ssl_ctx);
#endif
    }

    /* If exiting recursive main_loop, count, active, and connected in the
     * parent main_loop are invalid.  Set count to 0 to indicate that. */
    count = 0;
}

int sockecho(void)
{
    return !xsock || (xsock && !TELOPT(xsock, them, TN_ECHO));
}

void close_all(void)
{
    while (nfds > 3)
	close(--nfds);
}

int is_active(int fd)
{
    return FD_ISSET(fd, &active);
}

void readers_clear(int fd)
{
    FD_CLR(fd, &readers);
}

void readers_set(int fd)
{
    FD_SET(fd, &readers);
    if (fd >= nfds) nfds = fd + 1;
}

int tog_bg(Var *var)
{
    Sock *sock;
    if (background)
        for (sock = hsock; sock; sock = sock->next)
            if (sock->constate == SS_CONNECTED)
                FD_SET(sock->fd, &readers);
    return 1;
}

int tog_keepalive(Var *var)
{
    Sock *sock;
    int flags;

    flags = keepalive;
    for (sock = hsock; sock; sock = sock->next) {
	if (sock->constate != SS_CONNECTED) continue;
	if (setsockopt(sock->fd, SOL_SOCKET, SO_KEEPALIVE, (void*)&flags,
	    sizeof(flags)) < 0)
	{
	    wprintf("setsockopt KEEPALIVE: %s", strerror(errno));
	}
    }
    return 1;
}

/* get name of foreground world */
const char *fgname(void)
{
    return fsock ? fsock->world->name : NULL;
}

/* get current operational world */
World *xworld(void)
{
    return xsock ? xsock->world : NULL;
}

int xsock_is_fg(void)
{
    return xsock == fsock;
}

int have_active_socks(void)
{
    Sock *sock;

    for (sock = hsock; sock; sock = sock->next) {
	if (sock->world->screen->nnew || sock->queue.list.head)
	    return 1;
    }
    return 0;
}

/* set alert_id on xsock, so fg_sock can decide whether to clear alert */
void xsock_alert_id(void)
{
    if (xsock)
	xsock->alert_id = alert_id;
}

struct timeval *socktime(const char *name, int dir)
{
    Sock *sock;
    sock = *name ? find_sock(name) : xsock;
    if (!sock) return NULL;
    return &sock->time[dir];
}

static Sock *find_sock(const char *name)
{
    Sock *sock;

    /* It is possible to (briefly) have multiple sockets with the same name,
     * if at most one is alive.  The alive socket is the most interesting,
     * and it will be closer to the tail, so we search backwards.
     */
    for (sock = tsock; sock; sock = sock->prev)
        if (sock->world && cstrcmp(sock->world->name, name) == 0)
            return sock;
    return NULL;
}

/* load macro file for a world */
static void wload(World *w)
{
    const char *mfile;
    if (restriction >= RESTRICT_FILE) return;
    if ((mfile = world_mfile(w)))
        do_file_load(mfile, FALSE);
}

int world_hook(const char *fmt, const char *name)
{
    return do_hook(H_WORLD, virtscreen ? NULL : fmt, "%s",
	name ? name : "");
}

/* bring a socket into the foreground */
static int fg_sock(Sock *sock, int quiet)
{
    static int depth = 0;

    if (depth) {
        eprintf("illegal reentry");
        return 0;
    }

    if (sockecho() != (!sock || !TELOPT(sock, them, TN_ECHO)))
        set_refresh_pending(REF_LOGICAL);

    xsock = sock;
    if (sock == fsock) return 2;  /* already there */

    if (fsock) {                          /* the socket being backgrounded... */
	/* ...has new text */
        if (fsock->world->screen->nnew || fsock->queue.list.head) {
	    fsock->world->screen->active = 1;
            ++active_count;
            update_status_field(NULL, STAT_ACTIVE);
	/* ...is dead, with all text seen */
        } else if (fsock->constate == SS_ZOMBIE) {
            fsock->constate = SS_DEAD;
	    dead_socks++;
	}
    }
    fsock = sock;
    depth++;

    if (!virtscreen) update_status_field(NULL, STAT_WORLD);
    hide_screen(NULL);

    if (sock) {
	if (sock->alert_id == alert_id) {
	    clear_alert();
	    sock->alert_id = 0;
	}
	if (sock->constate == SS_CONNECTED)
	    FD_SET(sock->fd, &readers);
        if (sock->world->screen->active) {
	    sock->world->screen->active = 0;
            --active_count;
            update_status_field(NULL, STAT_ACTIVE);
        }
        fg_screen = sock->world->screen;
	unhide_screen(fg_screen); /* must do before hook which might oflush */
        world_hook((sock->constate >= SS_ZOMBIE) ?
            "---- World %s (dead) ----" : "---- World %s ----",
           sock->world->name);
	switch_screen(quiet || !bg_output);
        tog_lp(NULL);
        update_prompt(sock->prompt, 1);
        if (sockmload) wload(sock->world);
    } else {
        fg_screen = default_screen;
	unhide_screen(fg_screen);
        world_hook("---- No world ----", NULL);
	switch_screen(quiet || !bg_output);
        update_prompt(NULL, 1);
    }
    depth--;
    return 1;
}

struct Value *handle_fg_command(String *args, int offset)
{
    int opt, nosock = FALSE, noerr = FALSE, quiet = FALSE;
    int num = 0;
    ValueUnion uval;
    Sock *sock;

    startopt(CS(args), "nlqs<>c#");
    while ((opt = nextopt(NULL, &uval, NULL, &offset))) {
        switch (opt) {
        case 'n':  nosock = TRUE;  break;
        case 's':  noerr = TRUE;  break;
        case 'l':  break;  /* accepted and ignored */
        case 'q':  quiet = TRUE; break;
        case '<':  num = -1;  break;
        case '>':  num =  1;  break;
        case 'c':  num = uval.ival; break;
        default:   return shareval(val_zero);
        }
    }

    if (nosock) {
        return newint(fg_sock(NULL, quiet));

    } else if (num) {
        Sock *stop;
        if (!hsock) return shareval(val_zero);
        stop = sock = (fsock ? fsock : hsock);
        do {
	    sock = (num > 0) ?
		(sock->next ? sock->next : hsock) :
		(sock->prev ? sock->prev : tsock);
	    num += (num<0) ? 1 : -1;
        } while (num && sock != stop);
        return newint(fg_sock(sock, quiet));
    }

    sock = (!(args->len - offset)) ? hsock : find_sock(args->data + offset);
    if (!sock) {
        if (!noerr) eprintf("no socket named '%s'", args->data + offset);
        return shareval(val_zero);
    }
    return newint(fg_sock(sock, quiet));
}

/* openworld
 * If (name && port), they are used as hostname and port number.
 * If (!port), name is used as the name of a world.  A NULL or empty name
 * corresponds to the first defined world.  A NULL name should be used for
 * the initial automatic connection, an empty name should be used for any
 * other unspecified connection.
 */
int openworld(const char *name, const char *port, int flags)
{
    World *world = NULL;

    if (!(flags & (CONN_FG | CONN_BG)))
	flags |= (xsock == fsock) ? CONN_FG : CONN_BG;
    xsock = NULL;
    if (!port) {
        world = find_world(name);
        if (!world) {
            if (name) {
		/* not H_CONFAIL (it needs a Sock, we haven't made one yet) */
                eprintf("%s: %s",
                    *name ? name : "default world", "no such world");
            } else {
                world_hook("---- No world ----", NULL);
	    }
	}
    } else {
        if (restriction >= RESTRICT_WORLD)
            eprintf("arbitrary connections restricted");
        else {
            world = new_world(NULL, "", name, port, "", "", "",
		WORLD_TEMP | ((flags & CONN_SSL) ? WORLD_SSL : 0), "");
        }
    }

    return world ? opensock(world, flags) : 0;
}

static int opensock(World *world, int flags)
{
    int gai_err;
    const char *what;

    if (world->sock) {
        if (world->sock->constate < SS_ZOMBIE) {
            eprintf("socket to %s already exists", world->name);
            return 0;
        }
        /* recycle existing Sock */
	if (world->sock->constate == SS_DEAD)
	    dead_socks--;
        xsock = world->sock;
#if HAVE_SSL
	if (xsock->ssl) {
	    SSL_free(xsock->ssl); /* killsock() closed it, but didn't free it */
	    xsock->ssl = NULL;
	}
#endif

    } else {
        /* create and initialize new Sock */
        if (!(xsock = world->sock = (Sock *) MALLOC(sizeof(struct Sock)))) {
            eprintf("opensock: not enough memory");
            return 0;
        }
        if (!world->screen)
            world->screen = new_screen(hist_getsize(world->history));
        xsock->world = world;
        xsock->prev = tsock;
        tsock = *(tsock ? &tsock->next : &hsock) = xsock;
        xsock->prompt = NULL;
	xsock->prompt_timeout = tvzero;
        xsock->next = NULL;
	xsock->myhost = NULL;
	xsock->myaddr = NULL;
	xsock->addrs = NULL;
	xsock->addr = NULL;
#if HAVE_SSL
	xsock->ssl = NULL;
#endif
    }
    Stringninit(xsock->buffer, 80);  /* data must be allocated */
    Stringninit(xsock->subbuffer, 1);
    init_queue(&xsock->queue);
    xsock->host = NULL;
    xsock->port = NULL;
    xsock->ttype = -1;
    xsock->fd = -1;
    xsock->pid = -1;
    xsock->fsastate = '\0';
    xsock->substate = '\0';
    xsock->attrs = 0;
    xsock->prepromptattrs = 0;
    xsock->alert_id = 0;
#if HAVE_MCCP
    xsock->zstream = NULL;
#endif
    VEC_ZERO(&xsock->tn_them);
    VEC_ZERO(&xsock->tn_them_tog);
    VEC_ZERO(&xsock->tn_us);
    VEC_ZERO(&xsock->tn_us_tog);
    xsock->constate = SS_NEW;
    xsock->flags = 0;
    gettime(&xsock->time[SOCK_SEND]);
    xsock->time[SOCK_RECV] = xsock->time[SOCK_SEND];

    if ((flags & CONN_QUIETLOGIN) && (flags & CONN_AUTOLOGIN) &&
	world_character(world))
    {
	xsock->numquiet = MAXQUIET;
    } else {
	xsock->numquiet = 0;
    }

    if (flags & CONN_FG)
	fg_sock(xsock, FALSE); /* XXX 2nd arg??? */

    if (flags & CONN_AUTOLOGIN) xsock->flags |= SOCKLOGIN;
    if (world->flags & WORLD_ECHO) xsock->flags |= SOCKECHO;
    if ((flags & CONN_SSL) || (world->flags & WORLD_SSL)) {
#if HAVE_SSL
	xsock->ssl = SSL_new(ssl_ctx);
#else
        CONFAIL(xsock, "ssl", "not supported");
#endif
    }

    if (!world->host) {
	/* opening a connectionless socket */
        xsock->constate = SS_OPEN;
        return establish(xsock);
    } else if (!(world->flags & WORLD_NOPROXY) && proxy_host && *proxy_host) {
	/* open a connection through a proxy */
        xsock->flags |= SOCKPROXY;
        xsock->host = STRDUP(proxy_host);
        xsock->port = (proxy_port && *proxy_port) ? proxy_port : "23";
        xsock->port = STRDUP(xsock->port);
    } else {
	/* open a connection directly */
        xsock->host = STRDUP(world->host);
        xsock->port = STRDUP(world->port);
    }

    xsock->flags |= SOCKMAYTELNET;

    if (world->myhost && *world->myhost)
	xsock->myhost = STRDUP(world->myhost);
    else if (tfhost)
	xsock->myhost = STRDUP(tfhost);
    else
	xsock->myhost = NULL;

    if (xsock->myhost) {
	/* We don't bother with nonblocking:  this feature is rarely used,
	 * and the address should be numeric or resolvable locally (quickly)
	 * anyway, so it's not worth the complications of nonblocking. */
	struct addrinfo hints;
	int err;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	err = getaddrinfo(xsock->myhost, NULL, &hints, &xsock->myaddr);

	if (err) {
	    CONFAIL(xsock, xsock->myhost, gai_strerror(err));
	    killsock(xsock);  /* can't nukesock(), this Sock may be recycled. */
	    return 0;
	}
    }

    xsock->constate = SS_RESOLVING;
    xsock->fd = get_host_address(xsock, &what, &gai_err);
    if (xsock->fd == 0) {
        /* The name lookup succeeded */
        xsock->constate = SS_RESOLVED;
	xsock->addr = xsock->addrs;
        return openconn(xsock);
#ifdef NONBLOCKING_GETHOST
    } else if (xsock->fd > 0) {
        /* The name lookup is pending.  We wait for it for a fraction of a
         * second here so "relatively fast" looks "immediate" to the user.
         */
        fd_set readable;
        struct timeval tv;
        FD_ZERO(&readable);
        FD_SET(xsock->fd, &readable);
        tv.tv_sec = 0;
        tv.tv_usec = CONN_WAIT;
        if (select(xsock->fd + 1, &readable, NULL, NULL, &tv) > 0) {
            /* The lookup completed. */
            return openconn(xsock);
        }
        /* select() returned 0, or -1 and errno==EINTR.  Either way, the
         * lookup needs more time.  So we add the fd to the set being
         * watched by main_loop(), and don't block here any longer.
         */
        readers_set(xsock->fd);
        do_hook(H_PENDING, "%% Hostname lookup for %s in progress.", "%s",
            xsock->world->name);
        return 2;
#endif /* NONBLOCKING_GETHOST */
    } else {
        /* The name lookup failed */
        /* Note, some compilers can't handle (herr ? hsterror() : literal) */
        if (what)
            CONFAIL(xsock, what, strerror(errno));
        else if (gai_err)
            CONFAILHP(xsock, gai_strerror(gai_err));
        else
            CONFAILHP(xsock, "name lookup failed");
        killsock(xsock);  /* can't nukesock(), this may be a recycled Sock. */
        return 0;
    }
}


static const char *printai(struct addrinfo *ai, const char *fmt_hp)
{
    static char buf[1024];
    static char hostbuf[INET6_ADDRSTRLEN+1];
    const void *hostaddr = NULL;
    unsigned short port = 0;
    const char *host = NULL;

    if (!fmt_hp) fmt_hp = "%s %d";

    switch (ai->ai_family) {
	case AF_INET:
	    hostaddr = &((struct sockaddr_in*)ai->ai_addr)->sin_addr;
	    port = ((struct sockaddr_in*)ai->ai_addr)->sin_port;
	    host = inet_ntoa(*(struct in_addr*)hostaddr);
	    break;
#if ENABLE_INET6
	case AF_INET6:
	    hostaddr = &((struct sockaddr_in6*)ai->ai_addr)->sin6_addr;
	    port = ((struct sockaddr_in6*)ai->ai_addr)->sin6_port;
	    host = inet_ntop(ai->ai_family, hostaddr, hostbuf, sizeof(hostbuf));
	    break;
#endif
    }
    sprintf(buf, port ? fmt_hp : "%s", host ? host : "?", ntohs(port));
    return buf;
}

static inline int ai_connect(int s, struct addrinfo *ai)
{
    do_hook(H_PENDING, "%% Trying to connect to %s: %s.", "%s %s",
	xsock->world->name, printai(ai, NULL));
    return connect(s, ai->ai_addr, ai->ai_addrlen);
}

static void setupnextconn(Sock *sock)
{
    struct addrinfo *ai, *next = sock->addr;

    if (sock->fd >= 0)
	close(sock->fd);
retry:
    next = next->ai_next;
    /* if next address is a duplicate of one we've already done, skip it */
    for (ai = sock->addrs; next && ai != next; ai = ai->ai_next) {
	if (memcmp(ai->ai_addr, next->ai_addr, ai->ai_addrlen) == 0) {
	    goto retry;
	}
    }
    sock->addr = next;
}

/* If there are more addresses to try, hook ICONFAIL and try the next;
 * otherwise, hook CONFAIL and give up. */
static int ICONFAIL(Sock *sock, const char *what, const char *why)
{
    setupnextconn(sock);

    if (sock->addr) {
	do_hook(H_ICONFAIL, ICONFAIL_fmt, "%s %s: %s",
	    (sock)->world->name, (what), (why));
	oflush();
	return openconn(sock);
    }
    do_hook(H_CONFAIL, "%% Connection to %s failed: %s: %s", "%s %s: %s",
	(sock)->world->name, (what), (why));
    oflush();
    killsock(sock);
    return 0;
}

static int openconn(Sock *sock)
{
    int flags;

    xsock = sock;
#ifdef NONBLOCKING_GETHOST
    if (xsock->constate == SS_RESOLVING) {
	nbgai_hdr_t info = { 0, 0 };
	struct addrinfo *ai;
        FD_CLR(xsock->fd, &readers);
        if (read(xsock->fd, &info, sizeof(info)) < 0 || info.err != 0) {
            if (!info.err)
                CONFAIL(xsock, "read", strerror(errno));
            else
		CONFAILHP(xsock, gai_strerror(info.err));
            close(xsock->fd);
            xsock->fd = -1;
# ifdef PLATFORM_UNIX
	    if (xsock->pid >= 0)
		if (waitpid(xsock->pid, NULL, 0) < 0)
		    tfprintf(tferr, "waitpid %ld: %s", xsock->pid, strerror(errno));
	    xsock->pid = -1;
# endif /* PLATFORM_UNIX */
            killsock(xsock);
            return 0;
        }
	if (info.size <= 0) { /* shouldn't happen */
	    xsock->addrs = NULL;
	} else {
	    xsock->addrs = XMALLOC(info.size);
	    read(xsock->fd, (char*)xsock->addrs, info.size);
	}
        close(xsock->fd);
# ifdef PLATFORM_UNIX
        if (xsock->pid >= 0)
            if (waitpid(xsock->pid, NULL, 0) < 0)
               tfprintf(tferr, "waitpid: %ld: %s", xsock->pid, strerror(errno));
        xsock->pid = -1;
# endif /* PLATFORM_UNIX */
        xsock->constate = SS_RESOLVED;
	for (ai = xsock->addrs; ai; ai = ai->ai_next) {
	    ai->ai_addr = (struct sockaddr*)((char*)ai + sizeof(*ai));
	    if (ai->ai_next != 0) {
		ai->ai_next =
		    (struct addrinfo*)((char*)ai->ai_addr + ai->ai_addrlen);
	    }
	}
	xsock->addr = xsock->addrs;
    }
#endif /* NONBLOCKING_GETHOST */

#if 0
    /* Jump back here if we start a nonblocking connect and then discover
     * that the platform has a broken read() or select().
     */
    retry:
#endif

    if (!xsock->addr) {
	CONFAILHP(xsock, "no usable addresses");
	killsock(xsock);
	return 0;
    }

    xsock->fd = socket(xsock->addr->ai_family, xsock->addr->ai_socktype,
	xsock->addr->ai_protocol);
    if (xsock->fd < 0) {
	if (errno == EPROTONOSUPPORT || errno == EAFNOSUPPORT)
	    return ICONFAIL(xsock, "socket", strerror(errno));
        CONFAIL(xsock, "socket", strerror(errno));
        killsock(xsock);
        return 0;
    }

    if (xsock->myaddr && bind(xsock->fd, xsock->myaddr->ai_addr,
	xsock->myaddr->ai_addrlen) < 0)
    {
        CONFAIL(xsock, printai(xsock->myaddr, NULL), strerror(errno));
        killsock(xsock);
        return 0;
    }

#if HAVE_SSL
    if (xsock->ssl && SSL_set_fd(xsock->ssl, xsock->fd) <= 0) {
        CONFAIL(xsock, "SSL", ERR_error_string(ERR_get_error(), NULL));
        killsock(xsock);
        return 0;
    }
#endif

    if (xsock->fd >= nfds) nfds = xsock->fd + 1;

    if (!TF_NONBLOCK) {
        set_var_by_id(VAR_async_conn, 0);
    } else if (async_conn) {
        /* note: 3rd arg to fcntl() is optional on Unix, but required by OS/2 */
        if ((flags = fcntl(xsock->fd, F_GETFL, 0)) < 0) {
            operror("Can't make socket nonblocking: F_GETFL fcntl");
            set_var_by_id(VAR_async_conn, 0);
        } else if ((fcntl(xsock->fd, F_SETFL, flags | TF_NONBLOCK)) < 0) {
            operror("Can't make socket nonblocking: F_SETFL fcntl");
            set_var_by_id(VAR_async_conn, 0);
        }
    }

    if (keepalive) {
	flags = 1;
	if (setsockopt(xsock->fd, SOL_SOCKET, SO_KEEPALIVE, (void*)&flags,
	    sizeof(flags)) < 0)
	{
	    wprintf("setsockopt KEEPALIVE: %s", strerror(errno));
	}
    }

    xsock->constate = SS_CONNECTING;
    if (ai_connect(xsock->fd, xsock->addr) == 0) {
        /* The connection completed successfully. */
        xsock->constate = SS_CONNECTED;
        return establish(xsock);

#ifdef EINPROGRESS
    } else if (errno == EINPROGRESS) {
        /* The connection needs more time.  It will select() as writable when
         * it has connected, or readable when it has failed.  We select() it
         * briefly here so "fast" looks synchronous to the user.
         */
        fd_set writeable;
        struct timeval tv;
        FD_ZERO(&writeable);
        FD_SET(xsock->fd, &writeable);
        tv.tv_sec = 0;
        tv.tv_usec = CONN_WAIT;
        if (select(xsock->fd + 1, NULL, &writeable, NULL, &tv) > 0) {
            /* The connection completed. */
            return establish(xsock);
        }
        /* select() returned 0, or -1 and errno==EINTR.  Either way, the
         * connection needs more time.  So we add the fd to the set being
         * watched by main_loop(), and don't block here any longer.
         */
        FD_SET(xsock->fd, &writers);
        FD_SET(xsock->fd, &readers);
        return 2;
#endif /* EINPROGRESS */

#if 0  /* this can cause problems on nonbuggy systems, so screw the sysv bug */
    } else if (can_nonblock && (errno == EAGAIN
# ifdef EWOULDBLOCK
                                                || errno == EWOULDBLOCK
# endif
                                                                       )) {
        /* A bug in SVR4.2 causes nonblocking connect() to (sometimes?)
         * incorrectly fail with EAGAIN.  The only thing we can do about
         * it is to try a blocking connect().
         */
        close(xsock->fd);
        can_nonblock = FALSE;
        goto retry; /* try again */
#endif /* 0 */
    }

    /* The connection failed; try the next address. */
    return ICONFAIL_AI(xsock, strerror(errno));
}

#if !HAVE_GETADDRINFO
/* Partial implementation of getaddrinfo():  returns a single address for an
 * IPv6 address OR IPv4 address OR a name that resolves to an IPv4 address.
 * Ignores hints except for AI_NUMERICHOST.  Port is optional.
 */
static int tfgetaddrinfo(const char *nodename, const char *port,
    const struct addrinfo *hints, struct addrinfo **res)
{
    struct servent *service;
    struct hostent *host;
    unsigned short portnum = 0, sa_len = 0;
    int err = 0;
    struct {
	struct addrinfo ai;
	union {
	    struct sockaddr sa;
	    struct sockaddr_in sin4;
# if ENABLE_INET6
	    struct sockaddr_in6 sin6;
# endif
	} sa;
    } *blob;

    blob = MALLOC(sizeof(*blob));
    if (!blob) return TF_EAI_MEMORY;

    /* translate/look up port */
    if (port) {
	if (is_digit(*port)) {
	    portnum = htons(atoi(port));
# ifdef NETDB_H
	} else if ((service = getservbyname(port, "tcp"))) {
	    portnum = service->s_port;
# endif
	} else {
	    err = TF_EAI_SERVICE;
	    goto error;
	}
    }

# if ENABLE_INET6
    /* translate numeric IPv6 address */
    if (inet_pton(AF_INET6, nodename, &blob->sa.sin6.sin6_addr)) {
	sa_len = sizeof(blob->sa.sin6);
	blob->sa.sa.sa_family = PF_INET6;
	blob->sa.sin6.sin6_port = portnum;
	goto found;
    }
# endif

    /* translate numeric IPv4 address */
# if HAVE_INET_ATON
    if (inet_aton(nodename, &blob->sa.sin4.sin_addr))
# elif !defined(dgux)
    /* Most versions of inet_addr() return a long. */
    blob->sa.sin4.sin_addr.s_addr = inet_addr(nodename);
    if (blob->sa.sin4.sin_addr.s_addr != INADDR_NONE)
# else
    /* DGUX's inet_addr() returns a struct in_addr. */
    blob->sa.sin4.sin_addr.s_addr = inet_addr(nodename).s_addr;
    if (blob->sa.sin4.sin_addr.s_addr != INADDR_NONE)
# endif /* HAVE_INET_ATON */
    {
	sa_len = sizeof(blob->sa.sin4);
	blob->sa.sa.sa_family = PF_INET;
	blob->sa.sin4.sin_port = portnum;
        goto found;
    }

    if (hints->ai_flags & AI_NUMERICHOST) {
	FREE(blob);
	*res = NULL;
	return TF_EAI_NODATA;
    }

    /* look up text address (IPv4 only) */
    host = gethostbyname(nodename);
    if (!host) {
	err = h_errno;
	goto error;
    }
    sa_len = sizeof(blob->sa.sin4);
    blob->sa.sa.sa_family = PF_INET;
    blob->sa.sin4.sin_port = portnum;
    memcpy((void *)&blob->sa.sin4.sin_addr, (void *)host->h_addr,
	sizeof(blob->sa.sin4.sin_addr));

found:
# if HAVE_SOCKADDR_SA_LEN
    blob->sa.sa.sa_len = sa_len;
# endif
    blob->ai.ai_family = blob->sa.sa.sa_family;
    blob->ai.ai_socktype = SOCK_STREAM;
    blob->ai.ai_protocol = IPPROTO_TCP;
    blob->ai.ai_addrlen = sa_len;
    blob->ai.ai_canonname = NULL;
    blob->ai.ai_addr = (struct sockaddr*)&blob->sa;
    blob->ai.ai_next = NULL;
    *res = &blob->ai;
    return 0;

error:
    FREE(blob);
    return err;
}
#endif /* !HAVE_GETADDRINFO */

static void tffreeaddrinfo(struct addrinfo *ai)
{
    /* ai and all contents were allocated in single chunk by tfgetaddrinfo()
     * or completed nonblocking resolution in openconn() */
    FREE(ai);
}

/* Convert name or ip number string to a list of struct addrinfo.
 * Returns -1 for failure, 0 for success, or a positive file descriptor
 * connected to a pending name lookup process or thread.
 */
static int get_host_address(Sock *sock, const char **what, int *errp)
{
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_NUMERICHOST;
#if 0 /* AI_ADDRCONFIG is defined on FreeBSD 4.2, but causes an error in
	 getaddrinfo() (this was fixed by 4.8).  It's not important to
	 use the flag, so it's easier to not use it and avoid the problem. */
# ifdef AI_ADDRCONFIG
    hints.ai_flags |= AI_ADDRCONFIG;
# endif
#endif
    *errp = getaddrinfo(sock->host, sock->port, &hints, &sock->addrs);
    *what = NULL;
    if (*errp == 0) return 0;

#ifdef NONBLOCKING_GETHOST
    if (async_name) {
	/* do nonblocking for non-numeric */
	*errp = 0;
	*what = NULL;
	sock->flags |= SOCKALLOCADDRS;
	return nonblocking_gethost(sock->host, sock->port, &sock->addrs,
	    &sock->pid, what);
    } else
#endif
    {
	hints.ai_flags &= ~AI_NUMERICHOST;
	*errp = getaddrinfo(sock->host, sock->port, &hints, &sock->addrs);
	*what = NULL;
	if (*errp == 0) return 0;
    }
    return -1;
}

#ifdef NETDB_H
#ifdef NONBLOCKING_GETHOST

static void waitforhostname(int fd, const char *name, const char *port)
{
    /* Uses writev() so the write is atomic, to avoid race condition between
     * writer and reader processes.  (Really, the reader should loop reading
     * until EOF.)
     */
    struct addrinfo *res, *ai;
    struct addrinfo hints;
    nbgai_hdr_t hdr;
#define NIOV 1+2*16
    struct iovec iov[NIOV];
    int niov;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    hdr.err = getaddrinfo(name, port, &hints, &res);

    if (!hdr.err) {
	hdr.size = 0;
	iov[0].iov_base = (char*)&hdr;
	iov[0].iov_len = sizeof(hdr);
	niov = 1;
	for (ai = res; ai && niov < NIOV; ai = ai->ai_next) {
	    hdr.size += sizeof(*ai) + ai->ai_addrlen;
	    iov[niov].iov_base = (char*)ai;
	    iov[niov].iov_len = sizeof(*ai);
	    niov++;
	    iov[niov].iov_base = (char*)ai->ai_addr;
	    iov[niov].iov_len = ai->ai_addrlen;
	    niov++;
	}
	writev(fd, iov, niov);
	if (res) freeaddrinfo(res);
    } else {
	hdr.size = 0;
	write(fd, &hdr, sizeof(hdr));
    }
    close(fd);
}

# ifdef PLATFORM_OS2
typedef struct _threadpara {
    const char *hostname;
    const char *port;
    int   fd;
} threadpara;

void os2waitforhostname(threadpara *targs)
{
    waitforhostname(targs->fd, targs->hostname, targs->port);
    FREE(targs->hostname);
    FREE(targs->port);
    FREE(targs);
}
# endif /* PLATFORM_OS2 */

static int nonblocking_gethost(const char *name, const char *port,
    struct addrinfo **res, pid_t *pidp, const char **what)
{
    int fds[2];
    int err;

    *what = "pipe";
    if (pipe(fds) < 0) return -1;

#ifdef PLATFORM_UNIX
    {
        *what = "fork";
        *pidp = fork();
        if (*pidp > 0) {          /* parent */
            close(fds[1]);
            return fds[0];
        } else if (*pidp == 0) {  /* child */
            close(fds[0]);
            waitforhostname(fds[1], name, port);
            exit(0);
        }
    }
#endif
#ifdef PLATFORM_OS2
    {
        threadpara *tpara;
  
        if ((tpara = XMALLOC(sizeof(threadpara)))) {
            setmode(fds[0],O_BINARY);
            setmode(fds[1],O_BINARY);
            tpara->fd = fds[1];
            tpara->hostname = STRDUP(name);
            tpara->port = STRDUP(port);

            /* BUG: gethostbyname() isn't threadsafe! */
            *what = "_beginthread";
            if (_beginthread(os2waitforhostname,NULL,0x8000,(void*)tpara) != -1)
                return(fds[0]);
        }
    }
#endif

    /* failed */
    err = errno;
    close(fds[0]);
    close(fds[1]);
    errno = err;
    return -1;
}
#endif /* NONBLOCKING_GETHOST */
#endif /* NETDB_H */


/* Establish a sock for which connect() has completed. */
static int establish(Sock *sock)
{
    xsock = sock;
#if TF_NONBLOCK
    if (xsock->constate == SS_CONNECTING) {
        int err = 0, flags;
        socklen_t len = sizeof(err);

        /* Old Method 1: If read(fd, buf, 0) fails, the connect() failed, and
         * errno will explain why.  Problem: on some systems, a read() of
         * 0 bytes is always successful, even if socket is not connected.
         */
        /* Old Method 2: If a second connect() fails with EISCONN, the first
         * connect() worked.  On the slim chance that the first failed, but
         * the second worked, use that.  Otherwise, use getsockopt(SO_ERROR)
         * to find out why the first failed.  If SO_ERROR isn't available,
         * use read() to get errno.  This method works for all systems, as
         * well as SOCKS 4.2beta.  Problems: Some socket implementations
         * give the wrong errno; extra net traffic on failure.
         */
        /* CURRENT METHOD:  If possible, use getsockopt(SO_ERROR) to test for
         * an error.  This avoids the overhead of a second connect(), and the
         * possibility of getting the error value from the second connect()
         * (linux).  (Potential problem: it's possible that some systems
         * don't clear the SO_ERROR value for successful connect().)
         * If SO_ERROR is not available or we are using SOCKS, we try to read
         * 0 bytes.  If it fails, the connect() must have failed.  If it
         * succeeds, we can't know if it's because the connection really
         * succeeded, or the read() did a no-op for the 0 size, so we try a
         * second connect().  If the second connect() fails with EISCONN, the
         * first connect() worked.  If it works (unlikely), use it.  Otherwise,
         * use read() to get the errno.
         * Tested on:  Linux, HP-UX, Solaris, Solaris with SOCKS5...
         */
        /* Alternative: replace second connect() with getpeername(), and
         * check for ENOTCONN.  Disadvantage: doesn't work with SOCKS, etc.
         */

#ifdef SO_ERROR
# if !SOCKS
#  define USE_SO_ERROR
# endif
#endif

#ifdef USE_SO_ERROR
        if (getsockopt(xsock->fd, SOL_SOCKET, SO_ERROR, (void*)&err, &len) < 0)
        {
            return ICONFAIL(xsock, "getsockopt", strerror(errno));
        }

#else
        {
            char ch;
            if (read(xsock->fd, &ch, 0) < 0) {
		return ICONFAIL(xsock, "nonblocking connect/read", strerror(errno));
            } else if ((ai_connect(xsock->fd, xsock->addr) < 0) &&
                errno != EISCONN)
            {
                read(xsock->fd, &ch, 1);   /* must fail */
		return ICONFAIL(xsock, "nonblocking connect 2/read", strerror(errno));
            }
        }
#endif
        if (err != 0) {
	    return ICONFAIL_AI(xsock, strerror(err));
        }

        /* connect() worked.  Clear the pending stuff, and get on with it. */
        xsock->constate = SS_CONNECTED;
        FD_CLR(xsock->fd, &writers);

        /* Turn off nonblocking (this should help on buggy systems). */
        /* note: 3rd arg to fcntl() is optional on Unix, but required by OS/2 */
        if ((flags = fcntl(xsock->fd, F_GETFL, 0)) >= 0)
            fcntl(xsock->fd, F_SETFL, flags & ~TF_NONBLOCK);
    }
#endif /* TF_NONBLOCK */

    if (xsock->constate == SS_CONNECTED)
	FD_SET(xsock->fd, &readers);

    /* hack: sockaddr_in.sin_port and sockaddr_in6.sin6_port coincide */
    if (xsock->addr &&
	ntohs(((struct sockaddr_in*)xsock->addr->ai_addr)->sin_port) == 23)
	    xsock->flags |= SOCKTELNET;

#if HAVE_SSL
    if (xsock->ssl) {
	int sslret;
	sslret = SSL_connect(xsock->ssl);
	if (sslret <= 0) {
	    setupnextconn(xsock);
	    ssl_io_err(xsock, sslret, xsock->addr ? H_ICONFAIL : H_CONFAIL);
	    if (xsock->addr)
		return openconn(xsock);
	    killsock(xsock);
	    return 0;
	}
    }
#endif

    /* atomicness ends here */

    if (xsock == fsock)   /* possible for a recycled zombie */
        update_status_field(NULL, STAT_WORLD);

    if (xsock->flags & SOCKTELNET)
        preferred_telnet_options();

    if (sock->flags & SOCKPROXY) {
        FREE(sock->host);
        FREE(sock->port);
        sock->host = STRDUP(sock->world->host);
        sock->port = STRDUP(sock->world->port);
        do_hook(H_PROXY, "", "%s", sock->world->name);
    }

    wload(sock->world);

    if (!(sock->flags & SOCKPROXY)) {
#if HAVE_SSL
	if (sock->ssl)
	    do_hook(H_CONNECT, "%% Connected to %s using cipher %s.", "%s %s",
		sock->world->name, SSL_get_cipher_name(sock->ssl));
	else
#endif
	    do_hook(H_CONNECT, "%% Connected to %s.", "%s", sock->world->name);

        if (login && sock->flags & SOCKLOGIN) {
            if (world_character(sock->world) && world_pass(sock->world))
                do_hook(H_LOGIN, NULL, "%s %s %s", sock->world->name,
                    world_character(sock->world), world_pass(sock->world));
            sock->flags &= ~SOCKLOGIN;
        }
    }

    return 1;
}

/* clear most of sock's fields, but leave it consistent and extant */
static void killsock(Sock *sock)
{
    if (sock->constate >= SS_ZOMBIE) return;
#if 0 /* There may be a disconnect hook AFTER this function... */
    if (sock == fsock || sock->queue.list.head || sock->world->screen->nnew) {
	sock->constate = SS_ZOMBIE;
    } else {
	sock->constate = SS_DEAD;
	dead_socks++;
    }
#else /* ... so we must unconditionally enter ZOMBIE state. */
    sock->constate = SS_ZOMBIE;
#endif
    sock->fsastate = '\0';
    sock->attrs = 0;
    VEC_ZERO(&sock->tn_them);
    VEC_ZERO(&sock->tn_them_tog);
    VEC_ZERO(&sock->tn_us);
    VEC_ZERO(&sock->tn_us_tog);
    Stringfree(sock->buffer);
    Stringfree(sock->subbuffer);

    unprompt(sock, sock==fsock);

    if (sock->host) FREE(sock->host);
    if (sock->port) FREE(sock->port);
    if (sock->myhost) FREE(sock->myhost);
    sock->host = sock->port = sock->myhost = NULL;

    if (sock->addrs) {
	if (sock->flags & SOCKALLOCADDRS)
	    tffreeaddrinfo(sock->addrs);
	else
	    freeaddrinfo(sock->addrs);
	sock->addrs = NULL;
    }
    if (sock->myaddr) {
	freeaddrinfo(sock->myaddr);
	sock->myaddr = NULL;
    }
#if HAVE_SSL
    if (sock->ssl) {
	SSL_shutdown(sock->ssl);
	/* Don't SSL_free() yet: we still need to be able to use SSL_error() */
    }
#endif
    if (sock->fd >= 0) {
        FD_CLR(sock->fd, &readers);
        FD_CLR(sock->fd, &writers);
        close(sock->fd);
        sock->fd = -1;
    }
#if HAVE_MCCP
    if (sock->zstream) {
	inflateEnd(sock->zstream);
	FREE(sock->zstream);
    }
#endif
#ifdef NONBLOCKING_GETHOST
# ifdef PLATFORM_UNIX
    if (sock->pid >= 0) {
        kill(sock->pid, SIGTERM);
        if (waitpid(sock->pid, NULL, 0) < 0)
            tfprintf(tferr, "waitpid: %ld: %s", sock->pid, strerror(errno));
        sock->pid = -1;
    }
# endif /* PLATFORM_UNIX */
#endif /* NONBLOCKING_GETHOST */
    if (sock == fsock)
	update_status_field(NULL, STAT_WORLD);
}

/* nukesock
 * Remove socket from list and free memory.  Should only be called on a
 * Sock which is known to have no references other than the socket list.
 */
static void nukesock(Sock *sock)
{
    if (sock->queue.list.head) {
	internal_error(__FILE__, __LINE__, "socket %s has unprocessed lines",
	    !sock ? "!sock" :
	    !sock->world ? "!sock->world" :
	    sock->world->name);
	socks_with_lines--;
    }
    if (sock == xsock) xsock = NULL;
    sock->world->sock = NULL;
    killsock(sock);
    *((sock == hsock) ? &hsock : &sock->prev->next) = sock->next;
    *((sock == tsock) ? &tsock : &sock->next->prev) = sock->prev;
    if (sock->world->screen->active) {
	sock->world->screen->active = 0;
        --active_count;
        update_status_field(NULL, STAT_ACTIVE);
    }
    if (sock->world->flags & WORLD_TEMP) {
        nuke_world(sock->world);
        sock->world = NULL;
    }
#if HAVE_SSL
    if (sock->ssl) {
	SSL_free(sock->ssl);
	sock->ssl = NULL;
    }
#endif
    FREE(sock);
}

#if 0
static void fg_live_sock(void)
{
    /* If the fg sock is dead, find another sock to put in fg.  Since this
     * is called from main_loop(), xsock must be the same as fsock.  We must
     * keep it that way.  Note that a user hook in fg_sock() could kill the
     * new fg sock, so we must loop until the new fg sock stays alive.
     */
    while (fsock && (fsock->constate >= SS_ZOMBIE)) {
        for (xsock = hsock; xsock; xsock = xsock->next) {
            if (xsock->constate < SS_ZOMBIE)
		break;
        }
        fg_sock(xsock, FALSE);
    }
}
#endif

/* delete all dead sockets */
static void nuke_dead_socks(void)
{
    Sock *sock, *next;

    for (sock = hsock; sock; sock = next) {
        next = sock->next;
        if (sock->constate == SS_ZOMBIE) {
	    if (sock->fd >= 0)
		FD_CLR(sock->fd, &readers);
        } else if (sock->constate == SS_DEAD) {
            nukesock(sock);
            dead_socks--;
        }
    }
    if (quitdone && !hsock) quit_flag = 1;
}

static void queue_socket_line(Sock *sock, const conString *line, int offset,
    attr_t attr)
{
    String *new;
    (new = StringnewM(NULL, line->len - offset, 0, sock->world->md))->links++;
    SStringocat(new, line, offset);
    new->attrs |= attr;
    gettime(&new->time);
    sock->time[SOCK_RECV] = new->time;
    new->attrs &= ~F_GAG; /* in case gagged line was passed via fake_recv() */
    if (!sock->queue.list.head)
	socks_with_lines++;
    enqueue(&sock->queue, new);
}

static void dc(Sock *s)
{
    String *buffer = Stringnew(NULL, -1, 0);
    Sock *oldxsock = xsock;
    xsock = s;
    flushxsock();
    Sprintf(buffer, "%% Connection to %s closed.", s->world->name);
    world_output(s->world, CS(buffer));
    if (xsock != fsock) oputline(CS(buffer));
    zombiesock(xsock);
    xsock = oldxsock;
}

/* disconnect a socket */
struct Value *handle_dc_command(String *args, int offset)
{
    Sock *s;

    if (!(args->len - offset)) {
        if (!xsock) {
            eprintf("no current socket");
            return shareval(val_zero);
        }
	dc(xsock);
    } else if (cstrcmp(args->data + offset, "-all") == 0) {
        for (s = hsock; s; s = s->next)
	    if (s->constate < SS_DEAD)
		dc(s);
    } else {
        if ((s = find_sock(args->data + offset)) && (s->constate < SS_DEAD)) {
	    dc(s);
        } else {
            eprintf("Not connected to %s", args->data + offset);
            return shareval(val_zero);
        }
    }
    return shareval(val_one);
}

static int socklinescmp(const void *a, const void *b) {
    return (*(Sock**)a)->world->screen->nnew -
	   (*(Sock**)b)->world->screen->nnew;
}

static int sockidlecmp(const void *a, const void *b) {
    return tvcmp(&(*(Sock**)b)->time[SOCK_RECV],
	         &(*(Sock**)a)->time[SOCK_RECV]);
}

static int socknamecmp(const void *a, const void *b) {
    return nullcstrcmp((*(Sock**)a)->world->name, (*(Sock**)b)->world->name);
}

static int socktypecmp(const void *a, const void *b) {
    return nullcstrcmp((*(Sock**)a)->world->type, (*(Sock**)b)->world->type);
}

static int sockhostcmp(const void *a, const void *b) {
    return nullcstrcmp((*(Sock**)a)->world->host, (*(Sock**)b)->world->host);
}

static int sockportcmp(const void *a, const void *b) {
    return nullcstrcmp((*(Sock**)a)->world->port, (*(Sock**)b)->world->port);
}

static int sockcharcmp(const void *a, const void *b) {
    return nullcstrcmp((*(Sock**)a)->world->character,
		       (*(Sock**)b)->world->character);
}

/* display list of open sockets and their state. */
struct Value *handle_listsockets_command(String *args, int offset)
{
    Sock *sock;
    Vector socks = vector_init(32);
    char idlebuf[16], linebuf[16], addrbuf[64], state;
    const char *ptr;
    time_t now;
    int t, n, opt, i, nnew, nold;
    int error = 0, shortflag = FALSE, mflag = matching, numeric = 0;
    Pattern pat_name, pat_type;
    int typewidth = 11, namewidth = 15, hostwidth = 26;
    int (*cmp)(const void *, const void *) = NULL;

    init_pattern_str(&pat_name, NULL);
    init_pattern_str(&pat_type, NULL);

    startopt(CS(args), "m:sT:nS:");
    while ((opt = nextopt(&ptr, NULL, NULL, &offset))) {
        switch(opt) {
        case 'm':
            if ((mflag = enum2int(ptr, 0, enum_match, "-m")) < 0)
                goto listsocket_error;
            break;
        case 's':
            shortflag = TRUE;
            break;
        case 'T':
            free_pattern(&pat_type);
            error += !init_pattern_str(&pat_type, ptr);
            break;
	case 'n':
	    numeric = 1;
	    break;
	case 'S':
	    n = strlen(ptr);
	    if (n == 0 || cstrncmp(ptr, "name", n) == 0)
		cmp = socknamecmp;
	    else if (cstrncmp(ptr, "lines", n) == 0)
		cmp = socklinescmp;
	    else if (cstrncmp(ptr, "idle", n) == 0)
		cmp = sockidlecmp;
	    else if (cstrncmp(ptr, "type", n) == 0)
		cmp = socktypecmp;
	    else if (cstrncmp(ptr, "host", n) == 0)
		cmp = sockhostcmp;
	    else if (cstrncmp(ptr, "port", n) == 0)
		cmp = sockportcmp;
	    else if (cstrncmp(ptr, "character", n) == 0)
		cmp = sockcharcmp;
	    else if (cstrncmp(ptr, "-", n) == 0)
		cmp = NULL;
	    else { eprintf("invalid sort criterion"); error++; }
	    break;
        default:
            goto listsocket_error;
        }
    }
    if (error) goto listsocket_error;
    init_pattern_mflag(&pat_type, mflag, 'T');
    Stringstriptrail(args);
    if (args->len > offset)
        error += !init_pattern(&pat_name, args->data + offset, mflag);
    if (error) goto listsocket_error;

    if (!hsock) {
        if (!shortflag) eprintf("Not connected to any sockets.");
        goto listsocket_error;
    }

    /* collect matching socks */
    for (sock = hsock; sock; sock = sock->next) {
        if (pat_type.str &&
            !patmatch(&pat_type, NULL,
		sock->world->type ? sock->world->type : ""))
            continue;
        if (args->len - offset && !patmatch(&pat_name, NULL, sock->world->name))
            continue;
	vector_add(&socks, sock);
    }

    if (cmp)
	vector_sort(&socks, cmp);

    if (numeric) {
	namewidth += 3;
	typewidth += 3;
	hostwidth = 20;
    }

    now = time(NULL);

    if (!shortflag)
        oprintf("    %8s %4s %-*s %-*s %-*s %s",
	    "LINES", "IDLE", typewidth, "TYPE", namewidth, "NAME",
	    hostwidth, "HOST", "PORT");
    for (i = 0; i < socks.size; i++) {
	sock = socks.ptrs[i];
        nnew = sock->world->screen->nnew;
        nold = sock->world->screen->nback - nnew;
        if (shortflag) {
            oputs(sock->world->name);
            continue;
        }
        if (sock == fsock)
            sprintf(linebuf, "%8s", "fg");
        else {
	    if (nold) {
		if (nold > 999)
		    sprintf(linebuf, "%2dk", nold / 1000);
		else
		    sprintf(linebuf, "%3d", nold);
		if (nnew > 9999)
		    sprintf(linebuf+3, "+%3dk", nnew / 1000);
		else
		    sprintf(linebuf+3, "+%4d", nnew);
	    } else {
		if (nnew > 99999)
		    sprintf(linebuf, "%7dk", nnew / 1000);
		else
		    sprintf(linebuf, "%8d", nnew);
	    }
	}
        t = now - sock->time[SOCK_RECV].tv_sec;
        if (t < (60))
            sprintf(idlebuf, "%3ds", t);
        else if ((t /= 60) < 60)
            sprintf(idlebuf, "%3dm", t);
        else if ((t /= 60) < 24)
            sprintf(idlebuf, "%3dh", t);
        else if ((t /= 24) < 1000)
            sprintf(idlebuf, "%3dd", t);
        else
            sprintf(idlebuf, "long");

	state =
	    sock->constate < SS_CONNECTED ? '?' :
            sock->constate >= SS_ZOMBIE ? '!' :
	    sock->constate == SS_OPEN ? 'O' :
	    sock->constate == SS_CONNECTED && sock->fsastate == TN_SB ? 'S' :
#if HAVE_SSL
	    sock->constate == SS_CONNECTED && sock->ssl ? 'X' :
#endif
	    sock->constate == SS_CONNECTED ? 'C' :
	    '#' /* shouldn't happen */;
	if (sock->flags & SOCKCOMPRESS)
	    state = lcase(state);
	if (!numeric && sock->addr)
	    sprintf(addrbuf, "%-*.*s %.6s",
		hostwidth, hostwidth, sock->host, sock->port);
        oprintf("%c%c%c"
	    " %s %s %-*.*s %-*.*s %s",
            (sock == xsock ? '*' : ' '),
	    state,
	    (sock->flags & SOCKPROXY ? 'P' : ' '),
            linebuf, idlebuf,
            typewidth, typewidth, sock->world->type,
	    namewidth, namewidth, sock->world->name,
	    !sock->addr ? "" : numeric ? printai(sock->addr, "%-20.20s %d") :
		addrbuf);
    }

listsocket_error:
    free_pattern(&pat_name);
    free_pattern(&pat_type);

    return newint(socks.size);
}

int handle_send_function(conString *string, const char *world,
    const char *flags)
{
    const char *p;
    int result = 0, eol_flag = 1, hook_flag = 0;
    Sock *old_xsock = xsock;

    for (p = flags; *p; p++) {
	switch (*p) {
	case 'o': /* for backward compatability */
	    break;
	case '1': case 'n': /* for backward compatability */
	    eol_flag = 1; break;
	case 'u': /* fall through */
	case '0': case 'f': /* for backward compatability */
	    eol_flag = 0; break;
	case 'h':
	    hook_flag = 1; break;
	default:
	    eprintf("invalid flag %c", *p);
	    return 0;
	}
    }

    xsock = (!world || !*world) ? xsock : find_sock(world);
    if (!hook_flag || !do_hook(H_SEND, NULL, "%S", string))
	result = send_line(string->data, string->len, eol_flag);
    xsock = old_xsock;
    return result;
}

int handle_fake_recv_function(conString *string, const char *world,
    const char *flags)
{
    Sock *sock;
    int raw = 0;
    const char *p;

    for (p = flags; *p; p++) {
	switch (*p) {
	case 'r': raw = 1; break;
	default:
	    eprintf("invalid flag %c", *p);
	    return 0;
	}
    }

    sock = (!world || !*world) ? xsock : find_sock(world);
    if (!sock ||
	sock->constate <= SS_CONNECTING || sock->constate >= SS_ZOMBIE)
    {
        eprintf("no open world %s", world ? world : "");
	return 0;
    }
    if (raw)
	handle_socket_input(string->data, string->len);
    else
	queue_socket_line(sock, string, 0, 0);
    return 1;
}


/* tramsmit text to current socket */
static int transmit(const char *str, unsigned int numtowrite)
{
    int numwritten, err;

    if (!xsock || xsock->constate != SS_CONNECTED)
        return 0;
    while (numtowrite) {
#if HAVE_SSL
	if (xsock->ssl) {
	    numwritten = SSL_write(xsock->ssl, str, numtowrite);
	    if (numwritten <= 0) {
		zombiesock(xsock); /* before hook, so sock state is correct */
		ssl_io_err(xsock, numwritten, H_DISCONNECT);
		return 0;
	    }
	} else
#endif /* HAVE_SSL */
	{
	    numwritten = send(xsock->fd, str, numtowrite, 0);
	    if (numwritten < 0) {
		err = errno;
		/* Socket is blocking; EAGAIN and EWOULDBLOCK are impossible. */
		if (err == EINTR) {
		    return 0;
		} else if (err == EHOSTUNREACH || err == ENETUNREACH) {
		    /* XXX there should be an UNREACHABLE hook here */
		    eprintf("%s: send: %s", xsock->world->name, strerror(err));
		    return 0;
		} else {
		    flushxsock();
		    zombiesock(xsock); /* before hook, so state is correct */
		    DISCON(xsock->world->name, "send", strerror(err));
		    return 0;
		}
	    }
	}
        numtowrite -= numwritten;
        str += numwritten;
        gettime(&xsock->time[SOCK_SEND]);
    }
    return 1;
}

/* send_line
 * Send a line to the server on the current socket.  If there is a prompt
 * associated with the current socket, clear it.
 * RFCs 854 and 1123 technically forbid sending 8-bit data in non-BINARY mode;
 * but if the user types it, we send it regardless of BINARY mode, and trust
 * the user to know what he's doing.  Some servers accept it, some strip the
 * high bit, some ignore the characters.
 */
int send_line(const char *src, unsigned int len, int eol_flag)
{
    int max;
    int i = 0, j = 0;
    static char *buf = NULL;
    static int buflen = 0;

    if (!xsock ||
	(xsock->constate != SS_CONNECTED && !(xsock->flags & SOCKECHO)))
    {
        eprintf("Not connected.");
        return 0;
    }

    if (xsock && xsock->prompt) {
        /* Not the same as unprompt(): we keep attrs, and delete buffer. */
	if (!(xsock->flags & SOCKPROMPT)) {
	    /* Prompt was implicit, so there's still a copy in xsock->buffer */
	    Stringtrunc(xsock->buffer, 0);
	} else {
	    /* Prompt was explicit, so anything in xsock->buffer is an
	     * unrelated partial line that we must keep. */
	}
        conStringfree(xsock->prompt);
        xsock->prompt = NULL;
        xsock->prepromptattrs = 0;
        if (xsock == fsock) update_prompt(xsock->prompt, 1);
    }

    if (secho) {
	String *str = Stringnew(NULL, 0, 0);
	Sprintf(str, "%S%.*s%A", sprefix, len, src, getattrvar(VAR_secho_attr));
	world_output(xsock->world, CS(str));
    }

    max = 2 * len + 3;
    if (buflen < max) {
        buf = XREALLOC(buf, buflen = max);
    }
    while (j < len) {
        if (xsock->flags & SOCKTELNET && src[j] == TN_IAC)
            buf[i++] = TN_IAC;    /* double IAC */
        buf[i] = unmapchar(src[j]);
        i++, j++;
    }

    if (eol_flag) {
        /* In telnet NVT mode, append CR LF; in telnet BINARY mode,
         * append LF, CR, or CR LF, according to variable.
         */
        if (!TELOPT(xsock, us, TN_BINARY) || binary_eol != EOL_LF)
            buf[i++] = '\r';
        if (!TELOPT(xsock, us, TN_BINARY) || binary_eol != EOL_CR)
            buf[i++] = '\n';
    }

    if (xsock->flags & SOCKECHO)
	handle_socket_input(buf, i);
    if (xsock->constate == SS_CONNECTED)
	return transmit(buf, i);
    return 1;
}

static void handle_socket_lines(void)
{
    static int depth = 0;
    conString *line;
    int is_prompt;

    if (depth) return;	/* don't recurse */
    if (!(line = dequeue(&xsock->queue)))
	return;
    depth++;
    do {
	if (!xsock->queue.list.head) /* just dequeued the last line */
	    socks_with_lines--;

	if (line->attrs & (F_TFPROMPT)) {
	    incoming_text = line;
	    handle_prompt(incoming_text, 0, TRUE);
	    continue;
	}

	incoming_text = decode_ansi(line->data, xsock->attrs, emulation,
	    &xsock->attrs);
	incoming_text->time = line->time;
	is_prompt = line->attrs & F_SERVPROMPT;
	conStringfree(line);
	incoming_text->links++;

	if (is_prompt) {
	    if (do_hook(H_PROMPT, NULL, "%S", incoming_text)) {
		Stringfree(incoming_text);
	    } else {
		handle_prompt(incoming_text, 0, TRUE);
	    }

	} else {
	    if (borg || hilite || gag) {
		if (find_and_run_matches(NULL, -1, &incoming_text, xworld(),
		    TRUE, 0))
		{
		    if (xsock != fsock)
			do_hook(H_BGTRIG, "%% Trigger in world %s", "%s %S",
			    xsock->world->name, incoming_text);
		}
	    }

	    if (is_bamf(incoming_text->data) || is_quiet(incoming_text->data) ||
		is_watchdog(xsock->world->history, incoming_text) ||
		is_watchname(xsock->world->history, incoming_text))
	    {
		incoming_text->attrs |= F_GAG;
	    }

	    world_output(xsock->world, CS(incoming_text));
	    Stringfree(incoming_text);
	}
    } while ((line = dequeue(&xsock->queue)));
    depth--;

    /* We just emptied the queue; there may be a partial line pending */
    test_prompt();
}

/* log, record, and display line as if it came from sock */
void world_output(World *world, conString *line)
{
    Sock *saved_xsock = xsock;
    xsock = world ? world->sock : NULL;
    line->links++;
    recordline(world->history, line);
    record_global(line);
    if (world->sock && (world->sock->constate < SS_DEAD) &&
	!(gag && (line->attrs & F_GAG)))
    {
        if (world->sock == fsock) {
            screenout(line);
        } else {
	    static int preactivity_depth = 0; /* avoid recursion */
	    /* line was already tested for gag, so it WILL print */
	    if (!preactivity_depth && !world->screen->active) {
		preactivity_depth++;
                do_hook(H_PREACTIVITY, NULL, "%s", world->name);
		preactivity_depth--;
	    }
            enscreen(world->screen, line); /* ignores preactivity_depth */
            if (!preactivity_depth) {
		if (!world->screen->active && !(line->attrs & F_NOACTIVITY)) {
		    world->screen->active = 1;
		    ++active_count;
		    update_status_field(NULL, STAT_ACTIVE);
		    do_hook(H_ACTIVITY, "%% Activity in world %s", "%s",
			world->name);
		}
		do_hook(H_BGTEXT, NULL, "%s", world->name);
            }
        }
    }
    conStringfree(line);
    xsock = saved_xsock;
}

/* get the prompt for the fg sock */
conString *fgprompt(void)
{
    return (fsock) ? fsock->prompt : NULL;
}

int tog_lp(Var *var)
{
    if (!fsock) {
        /* do nothing */
    } else if (lpflag) {
        if (!fsock->queue.list.head && fsock->buffer->len) {
            fsock->prompt = CS(decode_ansi(fsock->buffer->data, fsock->attrs,
                emulation, &fsock->attrs));
            fsock->prompt->links++;
            set_refresh_pending(REF_PHYSICAL);
        }
    } else {
        if (fsock->prompt && !(fsock->flags & SOCKPROMPT)) {
            unprompt(fsock, 1);
            set_refresh_pending(REF_PHYSICAL);
        }
    }
    return 1;
}

int handle_prompt_func(conString *str)
{
    if (xsock)
	queue_socket_line(xsock, str, 0, F_TFPROMPT);
    return !!xsock;
}

static void handle_prompt(String *str, int offset, int confirmed)
{
    runall(1, xsock->world);	/* run prompted processes */
    if (xsock->prompt) {
        unprompt(xsock, 0);
    }
    assert(str->links == 1);
    xsock->prompt = CS(str);
    /* Old versions did trigger checking here.  Removing it breaks
     * compatibility, but I doubt many users will care.  Leaving
     * it in would not be right for /prompt.
     */
    if (xsock == fsock) update_prompt(xsock->prompt, 1);
    if (confirmed) xsock->flags |= SOCKPROMPT;
    else xsock->flags &= ~SOCKPROMPT;
    xsock->numquiet = 0;
}

static void handle_implicit_prompt(void)
{
    String *new;

    xsock->prompt_timeout = tvzero;
    xsock->prepromptattrs = xsock->attrs; /* save attrs for unprompt() */

    new = decode_ansi(xsock->buffer->data, xsock->attrs, emulation,
	&xsock->attrs);
    new->links++;

    handle_prompt(new, 0, FALSE);
}

/* undo the effects of a false prompt */
static void unprompt(Sock *sock, int update)
{
    if (!sock || !sock->prompt || !(sock->flags & SOCKPROMPT)) return;
    sock->attrs = sock->prepromptattrs;  /* restore original attrs */
    conStringfree(sock->prompt);
    sock->prompt = NULL;
    sock->prepromptattrs = 0;
    if (update) update_prompt(sock->prompt, 1);
}

static void schedule_prompt(Sock *sock)
{
    if (prompt_timeout.tv_sec == 0 ||
	tvcmp(&sock->prompt_timeout, &prompt_timeout) < 0)
    {
	prompt_timeout = sock->prompt_timeout;
    }
}

static void test_prompt(void)
{
    if (lpflag && !xsock->queue.list.head && xsock->buffer->len) {
	if (do_hook(H_PROMPT, NULL, "%S", xsock->buffer)) {
	    /* The hook took care of the unterminated line. */
	    Stringtrunc(xsock->buffer, 0);
	} else if (lpflag) {
	    /* Wait for prompt_wait before treating unterm'd line as prompt. */
	    struct timeval now;
	    gettime(&now);
	    tvadd(&xsock->prompt_timeout, &now, &prompt_wait);
	    schedule_prompt(xsock);
	}
    }
}

static void telnet_subnegotiation(void)
{
    char *p;
    int ttype;
    static conString enum_ttype[] = {
        STRING_LITERAL("TINYFUGUE"),
        STRING_LITERAL("ANSI-ATTR"),
        STRING_LITERAL("ANSI"), /* a lie, but probably has the desired effect */
        STRING_LITERAL("UNKNOWN"),
        STRING_NULL };

    telnet_debug("recv", xsock->subbuffer->data, xsock->subbuffer->len);
    Stringtrunc(xsock->subbuffer, xsock->subbuffer->len - 2);
    p = xsock->subbuffer->data + 2;
    switch (*p) {
    case TN_TTYPE:
	if (!TELOPT(xsock, us, *p)) {
	    no_reply("option was not agreed on");
	    break;
	}
        if (*++p != '\01' || xsock->subbuffer->len != 4) {
	    no_reply("invalid syntax");
	    break;
	}
        Sprintf(telbuf, "%c%c%c%c", TN_IAC, TN_SB, TN_TTYPE, '\0');
        ttype = ++xsock->ttype;
        if (!enum_ttype[ttype].data) { ttype--; xsock->ttype = -1; }
        SStringcat(telbuf, &enum_ttype[ttype]);
        Sappendf(telbuf, "%c%c", TN_IAC, TN_SE);
        telnet_send(telbuf);
        break;
    case TN_COMPRESS:
    case TN_COMPRESS2:
	if (!TELOPT(xsock, them, *p)) {
	    no_reply("option was not agreed on");
	    break;
	}
	xsock->flags |= SOCKCOMPRESS;
	break;
    default:
	no_reply("unknown option");
        break;
    }
    Stringtrunc(xsock->subbuffer, 0);
}

static void f_telnet_recv(int cmd, int opt)
{
    if (telopt) {
        char buf[4];
        sprintf(buf, "%c%c%c", TN_IAC, cmd, opt);
        telnet_debug("recv", buf,
            (cmd==(UCHAR)TN_DO || cmd==(UCHAR)TN_DONT ||
             cmd==(UCHAR)TN_WILL || cmd==(UCHAR)TN_WONT) ?
             3 : 2);
    }
}

#if HAVE_MCCP
static z_stream *new_zstream(void)
{
    z_stream *zstream;
    if (!(zstream = MALLOC(sizeof(z_stream))))
	return NULL;
    zstream->zalloc = Z_NULL;
    zstream->zfree = Z_NULL;
    zstream->opaque = Z_NULL;
    if (inflateInit(zstream) == Z_OK)
	return zstream;
    if (zstream->msg)
	eprintf("unable to start compressed stream: %s", zstream->msg);
    FREE(zstream);
    return NULL;
}
#endif /* HAVE_MCCP */

/* handle input from current socket */
static int handle_socket_input(const char *simbuffer, int simlen)
{
    char rawchar, localchar, inbuffer[4096];
    const char *buffer, *place;
#if HAVE_MCCP
    char outbuffer[4096];
#endif
    fd_set readfds;
    int count, n, received = 0;
    struct timeval timeout;

    if (xsock->constate <= SS_CONNECTING || xsock->constate >= SS_ZOMBIE)
	return 0;

    if (xsock->prompt && !(xsock->flags & SOCKPROMPT)) {
        /* We assumed last text was a prompt, but now we have more text, so
         * we now assume that they are both part of the same long line.  (If
         * we're wrong, the previous prompt appears as output.  But if we did
         * the opposite, a real begining of a line would never appear in the
         * output window; that would be a worse mistake.)
	 * Note that a terminated (EOR or GOAHEAD) prompt will NOT be cleared
	 * when new text arrives (it will only be cleared when there is a new
	 * prompt).
         */
        unprompt(xsock, xsock==fsock);
    }
    xsock->prompt_timeout = tvzero;

    do {  /* while there are more data to be processed */
	if (simbuffer) {
	    buffer = simbuffer;
	    count = simlen;
	} else {
#if HAVE_MCCP
	    if (xsock->zstream && xsock->zstream->avail_in) {
		count = 0;  /* no reading */
	    } else
#endif
#if HAVE_SSL
	    if (xsock->ssl) {
		count = SSL_read(xsock->ssl, inbuffer, sizeof(inbuffer));
		if (count == 0 &&
		    SSL_get_error(xsock->ssl, 0) == SSL_ERROR_SYSCALL &&
		    ERR_peek_error() == 0)
		{
		    /* Treat a count of 0 with no errors as a normal EOF */
		    goto eof;
		} else if (count <= 0) {
		    /* Treat an error or a bad EOF as an error */
		    zombiesock(xsock); /* before hook, so state is correct */
		    ssl_io_err(xsock, count, H_DISCONNECT);
		    return received;
		}
	    } else
#endif
	    {
		/* We could loop while (count < 0 && errno == EINTR), but if we
		 * got here because of a mistake in the active fdset and there
		 * is really nothing to read, the loop would be unbreakable. */
		count = recv(xsock->fd, inbuffer, sizeof(inbuffer), 0);
		eof:
		if (count <= 0) {
		    int err = errno;
		    constate_t state = xsock->constate;
		    if (count < 0 && errno == EINTR)
			return 0;
		    /* Socket is blocking; EAGAIN and EWOULDBLOCK impossible. */
		    if (xsock->buffer->len) {
			queue_socket_line(xsock, CS(xsock->buffer), 0, 0);
			Stringtrunc(xsock->buffer, 0);
		    }
		    flushxsock();
		    /* On some systems, a socket that failed nonblocking connect
		     * selects readable instead of writable.  If SS_CONNECTING,
		     * that's what happened, so we do CONFAIL not DISCONNECT.
		     */
		    zombiesock(xsock); /* before hook, so constate is correct */
		    if (state == SS_CONNECTING)
			CONFAIL(xsock, "recv", strerror(err));
		    else do_hook(H_DISCONNECT, (count < 0) ?
			"%% Connection to %s closed: %s: %s" :
			"%% Connection to %s closed by foreign host.",
			(count < 0) ? "%s %s %s" : "%s",
			xsock->world->name, "recv", strerror(err));
		    return received;
		}
	    }
#if HAVE_MCCP
	    if (xsock->zstream) {
		int zret;
		if (count) {
		    xsock->zstream->next_in = (Bytef*)inbuffer;
		    xsock->zstream->avail_in = count;
		}
		xsock->zstream->next_out = (Bytef*)(buffer = outbuffer);
		xsock->zstream->avail_out = sizeof(outbuffer);
		zret = inflate(xsock->zstream, Z_PARTIAL_FLUSH);
		switch (zret) {
		case Z_OK:
		    count = (char*)xsock->zstream->next_out - outbuffer;
		    break;
		case Z_STREAM_END:
		    /* handle stuff inflated before stream end */
		    count = (char*)xsock->zstream->next_out - outbuffer;
		    received += handle_socket_input(outbuffer, count);
		    /* prepare to handle noncompressed stuff after stream end */
		    buffer = (char*)xsock->zstream->next_in;
		    count = xsock->zstream->avail_in;
		    /* clean up zstream */
		    inflateEnd(xsock->zstream);
		    FREE(xsock->zstream);
		    xsock->zstream = NULL;
		    xsock->flags &= ~SOCKCOMPRESS;
		    break;
		default:
		    flushxsock();
		    zombiesock(xsock); /* before hook, so constate is correct */
		    DISCON(xsock->world->name, "inflate",
			xsock->zstream->msg ? xsock->zstream->msg : "unknown");
		    return received;
		}
	    } else
#endif
		buffer = inbuffer;
	}

        for (place = buffer; place - buffer < count; place++) {

            /* We always accept 8-bit data, even though RFCs 854 and 1123
             * say server shouldn't transmit it unless in BINARY mode.  What
             * we do with it depends on the locale.
             */
            rawchar = *place;
            localchar = localize(rawchar);

            /* Telnet processing
             * If we're not sure yet whether server speaks telnet protocol,
             * we still accept it, but if an invalid telnet command arrives,
             * we leave it alone and disable telnet processing.
             */
            if (xsock->fsastate == TN_IAC &&
                xsock->flags & (SOCKTELNET | SOCKMAYTELNET))
            {
                int valid = 0;
                switch (xsock->fsastate = rawchar) {
                case TN_GA: case TN_EOR:
                    /* This is definitely a prompt. */
                    telnet_recv(rawchar, 0);
		    queue_socket_line(xsock, CS(xsock->buffer), 0, F_SERVPROMPT);
		    Stringtrunc(xsock->buffer, 0);
                    break;
                case TN_SB:
		    if (!(xsock->flags & SOCKTELNET)) {
			/* Telnet subnegotiation can't happen without a
			 * previous telnet option negotation, so treat the
			 * IAC SB as non-telnet, and disable telnet. */
			xsock->flags &= ~SOCKMAYTELNET;
			place--;
			rawchar = xsock->fsastate;
			goto non_telnet;
		    }
                    Sprintf(xsock->subbuffer, "%c%c", TN_IAC, TN_SB);
                    xsock->substate = '\0';
                    break;
                case TN_WILL: case TN_WONT:
                case TN_DO:   case TN_DONT:
                    /* just change state */
                    break;
                case TN_IAC:
                    Stringadd(xsock->buffer, localchar);  /* literal IAC */
                    xsock->fsastate = '\0';
                    break;

                case TN_NOP:
                case TN_DATA_MARK:
                case TN_BRK:
                case TN_IP:
                case TN_AO:
                case TN_AYT:
                case TN_EC:
                case TN_EL:
                    valid = 1;
		    /* Unsupported.  Fall through to default case. */
                default:
                    if (xsock->flags & SOCKTELNET ||
                        (xsock->flags & SOCKMAYTELNET && valid))
                    {
                        /* unsupported or invalid telnet command */
                        telnet_recv(rawchar, 0);
                        xsock->fsastate = '\0';
                        break;
                    }
                    /* Invalid telnet command, and telnet protocol hasn't been
                     * established, so treat the IAC and rawchar as non-telnet,
                     * and disable telnet. */
                    xsock->flags &= ~SOCKMAYTELNET;
                    place--;
                    rawchar = xsock->fsastate;
                    goto non_telnet;
                }

                if (!(xsock->flags & SOCKTELNET)) {
                    /* We now know server groks TELNET */
                    xsock->flags |= SOCKTELNET;
                    preferred_telnet_options();
                }
                continue;  /* avoid non-telnet processing */

            } else if (xsock->fsastate == TN_SB) {
		if (xsock->subbuffer->len > 255) {
		    /* It shouldn't take this long; server is broken.  Abort. */
		    SStringcat(xsock->buffer, CS(xsock->subbuffer));
		    Stringtrunc(xsock->subbuffer, 0);
		} else if (xsock->substate == TN_IAC) {
                    if (rawchar == TN_SE) {
			Sappendf(xsock->subbuffer, "%c%c", TN_IAC, TN_SE);
                        xsock->fsastate = '\0';
                        telnet_subnegotiation();
                    } else {
                        Stringadd(xsock->subbuffer, rawchar);
                        xsock->substate = '\0';
                    }
                } else if (rawchar == TN_IAC) {
                    xsock->substate = TN_IAC;
                } else {
                    Stringadd(xsock->subbuffer, rawchar);
                    xsock->substate = '\0';
#if HAVE_MCCP /* hack for broken MCCPv1 subnegotiation */
		    if (xsock->subbuffer->len == 5 &&
			memcmp(xsock->subbuffer->data, mccp1_subneg, 5) == 0)
		    {
			xsock->fsastate = '\0';
			telnet_subnegotiation();
		    }
#endif
                }
#if HAVE_MCCP
		if (xsock->flags & SOCKCOMPRESS && !xsock->zstream) {
		    /* compression was just enabled. */
		    xsock->zstream = new_zstream();
		    if (!xsock->zstream) {
			zombiesock(xsock);
		    } else {
			xsock->zstream->next_in = (Bytef*)++place;
			xsock->zstream->avail_in = count - (place - buffer);
		    }
		    count = 0; /* abort the buffer scan */
		}
#endif
                continue;  /* avoid non-telnet processing */

            } else if (xsock->fsastate == TN_WILL) {
                xsock->fsastate = '\0';
                telnet_recv(TN_WILL, rawchar);
                if (TELOPT(xsock, them, rawchar)) { /* already there, ignore */
		    no_reply("option was already agreed on");
                    CLR_TELOPT(xsock, them_tog, rawchar);
                } else if (
#if 0  /* many servers think DO SGA means character-at-a-time mode */
                    rawchar == TN_SGA ||
#endif
#if HAVE_MCCP
		    (rawchar == TN_COMPRESS && mccp) ||
		    (rawchar == TN_COMPRESS2 && mccp) ||
#endif
                    rawchar == TN_ECHO ||
                    rawchar == TN_SEND_EOR ||
                    rawchar == TN_BINARY)              /* accept any of these */
                {
                    SET_TELOPT(xsock, them, rawchar);  /* set state */
                    if (TELOPT(xsock, them_tog, rawchar)) {/* we requested it */
                        CLR_TELOPT(xsock, them_tog, rawchar);  /* done */
                    } else {
                        DO(rawchar);  /* acknowledge their request */
                    }
                } else {
                    DONT(rawchar);    /* refuse their request */
                }
                continue;  /* avoid non-telnet processing */

            } else if (xsock->fsastate == TN_WONT) {
                xsock->fsastate = '\0';
                telnet_recv(TN_WONT, rawchar);
                if (!TELOPT(xsock, them, rawchar)) { /* already there, ignore */
		    no_reply("option was already agreed on");
                    CLR_TELOPT(xsock, them_tog, rawchar);
                } else {
                    CLR_TELOPT(xsock, them, rawchar);  /* set state */
                    if (TELOPT(xsock, them_tog, rawchar)) { /* we requested */
                        CLR_TELOPT(xsock, them_tog, rawchar);  /* done */
                    } else {
                        DONT(rawchar);  /* acknowledge their request */
                    }
                }
                continue;  /* avoid non-telnet processing */

            } else if (xsock->fsastate == TN_DO) {
                xsock->fsastate = '\0';
                telnet_recv(TN_DO, rawchar);
                if (TELOPT(xsock, us, rawchar)) { /* already there, ignore */
		    no_reply("option was already agreed on");
                    CLR_TELOPT(xsock, them_tog, rawchar);
                } else if (
                    rawchar == TN_NAWS ||
                    rawchar == TN_TTYPE ||
                    rawchar == TN_BINARY)
                {
                    SET_TELOPT(xsock, us, rawchar);  /* set state */
                    if (TELOPT(xsock, us_tog, rawchar)) { /* we requested it */
                        CLR_TELOPT(xsock, us_tog, rawchar);  /* done */
                    } else {
                        WILL(rawchar);  /* acknowledge their request */
                    }
                    if (rawchar == TN_NAWS) do_naws(xsock);
                } else {
                    WONT(rawchar);      /* refuse their request */
                }
                continue;  /* avoid non-telnet processing */

            } else if (xsock->fsastate == TN_DONT) {
                xsock->fsastate = '\0';
                telnet_recv(TN_DONT, rawchar);
                if (!TELOPT(xsock, us, rawchar)) { /* already there, ignore */
		    no_reply("option was already agreed on");
                    CLR_TELOPT(xsock, us_tog, rawchar);
                } else {
                    CLR_TELOPT(xsock, us, rawchar);  /* set state */
                    if (TELOPT(xsock, us_tog, rawchar)) { /* we requested */
                        CLR_TELOPT(xsock, us_tog, rawchar);  /* done */
                    } else {
                        WONT(rawchar);  /* acknowledge their request */
                    }
                }
                continue;  /* avoid non-telnet processing */

            } else if (rawchar == TN_IAC &&
                xsock->flags & (SOCKTELNET | SOCKMAYTELNET))
            {
                xsock->fsastate = rawchar;
                continue;  /* avoid non-telnet processing */
            }

            /* non-telnet processing*/
non_telnet:
            if (rawchar == '\n') {
                /* Complete line received.  Queue it. */
                queue_socket_line(xsock, CS(xsock->buffer), 0, 0);
		Stringtrunc(xsock->buffer, 0);
                xsock->fsastate = rawchar;

            } else if (emulation == EMUL_DEBUG) {
                if (localchar != rawchar)
                    Stringcat(xsock->buffer, "M-");
                if (is_print(localchar))
                    Stringadd(xsock->buffer, localchar);
                else {
                    Stringadd(xsock->buffer, '^');
                    Stringadd(xsock->buffer, CTRL(localchar));
                }
                xsock->fsastate = '\0';

            } else if (rawchar == '\r' || rawchar == '\0') {
                /* Ignore CR and NUL. */
                xsock->fsastate = rawchar;

            } else if (rawchar == '\b' && emulation >= EMUL_PRINT &&
                xsock->fsastate == '*')
	    {
		/* "*\b" is an LP editor prompt. */
		queue_socket_line(xsock, CS(xsock->buffer), 0, F_SERVPROMPT);
		Stringtrunc(xsock->buffer, 0);
                xsock->fsastate = '\0';
		/* other occurances of '\b' are handled by decode_ansi(), so
		 * ansi codes aren't clobbered before they're interpreted */

            } else {
                /* Normal character. */
                const char *end;
                Stringadd(xsock->buffer, localchar);
                end = ++place;
                /* Quickly skip characters that can't possibly be special. */
                while (is_print(*end) && *end != TN_IAC && end - buffer < count)
                    end++;
                Stringfncat(xsock->buffer, (char*)place, end - place);
                place = end - 1;
                xsock->fsastate = (*place == '*') ? '*' : '\0';
            }
        }

        /* See if anything arrived while we were parsing */

	received += count;

#if HAVE_MCCP
	/* If there's still un-inflated stuff, we must process it before we
	 * exit this loop.
	 */
	if (xsock->zstream && xsock->zstream->avail_in) {
	    n = xsock->zstream->avail_in;
	    continue;
	}
#endif

	if (simbuffer || received > SPAM) break; /* after uninflated check */

        FD_ZERO(&readfds);
        FD_SET(xsock->fd, &readfds);
	timeout = tvzero; /* don't use tvzero directly, select may modify it */
        if ((n = select(xsock->fd + 1, &readfds, NULL, NULL, &timeout)) < 0) {
            if (errno != EINTR) die("handle_socket_input: select", errno);
        }

	if (interrupted()) break;

    } while (n > 0);

    test_prompt();

    return received;
}


static int is_quiet(const char *str)
{
    if (!xsock->numquiet) return 0;
    if (!--xsock->numquiet) return 1;
    /* This should not be hard coded. */
    if ((strncmp("Use the WHO command", str, 19) == 0) ||
        (strcmp("### end of messages ###", str) == 0))
            xsock->numquiet = 0;
    return 1;
}

static int is_bamf(const char *str)
{
    smallstr name, host, port;
    STATIC_BUFFER(buffer);
    World *world;
    Sock *callingsock;  /* like find_and_run_matches(), we must restore xsock */

    callingsock = xsock;

    if (!bamf || restriction >= RESTRICT_WORLD) return 0;
    if (sscanf(str,
        "#### Please reconnect to %64[^ @]@%64s (%*64[^ )]) port %64s ####",
        name, host, port) != 3)
            return 0;

    Stringtrunc(buffer, 0);
    if (bamf == BAMF_UNTER) Stringadd(buffer, '@');
    Stringcat(buffer, name);
    if (!(world = find_world(buffer->data))) {
        if (bamf == BAMF_UNTER && xsock) {
            world = new_world(buffer->data, xsock->world->type,
		host, port, xsock->world->character, xsock->world->pass,
		xsock->world->mfile, WORLD_TEMP, xsock->world->myhost);
        } else {
            world = new_world(buffer->data,"",host,port,"","","",WORLD_TEMP,"");
        }
    }

    do_hook(H_BAMF, "%% Bamfing to %s", "%s", name);
    if (bamf == BAMF_UNTER && xsock) killsock(xsock);
    if (!opensock(world, CONN_AUTOLOGIN))
        eputs("% Connection through portal failed.");
    xsock = callingsock;
    return 1;
}

static void do_naws(Sock *sock)
{
    unsigned int width, height, i;
    UCHAR octet[4];
    Sock *old_xsock;

    width = (emulation != EMUL_RAW && wrapflag && wrapsize) ?
	wrapsize : columns;
    height = winlines() + !visual;

    Sprintf(telbuf, "%c%c%c", TN_IAC, TN_SB, TN_NAWS);
    octet[0] = (width >> 8);
    octet[1] = (width & 0xFF);
    octet[2] = (height >> 8);
    octet[3] = (height & 0xFF);
    for (i = 0; i < 4; i++) {
        if (octet[i] == (UCHAR)TN_IAC) Stringadd(telbuf, TN_IAC);
        Stringadd(telbuf, octet[i]);
    }
    Sappendf(telbuf, "%c%c", TN_IAC, TN_SE);

    old_xsock = xsock;;
    xsock = sock;
    telnet_send(telbuf);
    xsock = old_xsock;
}

static void telnet_debug(const char *dir, const char *str, int len)
{
    String *buffer;
    int state;

    if (telopt) {
        buffer = Stringnew(NULL, 0, 0);
        Sprintf(buffer, "%% %s:", dir);
	if (len == 0)
	    Stringcat(buffer, str);
	else {
	    for (state = 0; len; len--, str++) {
		if (*str == TN_IAC || state == TN_IAC || state == TN_SB ||
		    state == TN_WILL || state == TN_WONT ||
		    state == TN_DO || state == TN_DONT)
		{
		    if (telnet_label[(UCHAR)*str])
			Sappendf(buffer, " %s", telnet_label[(UCHAR)*str]);
		    else
			Sappendf(buffer, " %d", (unsigned int)*str);
		    state = *str;
		} else if (state == TN_TTYPE) {
		    if (*str == (char)0) {
			Stringcat(buffer, " IS \"");
			while (len--, str++, is_print(*str) && !(*str & 0x80))
			    Stringadd(buffer, *str);
			Stringadd(buffer, '\"');
			len++, str--;
		    } else if (*str == (char)1) {
			Stringcat(buffer, " SEND");
		    }
		    state = 0;
		} else {
		    Sappendf(buffer, " %u", (unsigned char)*str);
		    state = 0;
		}
	    }
	}
        nolog++;
        /* norecord++; */
        world_output(xsock->world, CS(buffer));
        /* norecord--; */
        nolog--;
    }
}

static void telnet_send(String *cmd)
{
    transmit(cmd->data, cmd->len);
    telnet_debug("sent", cmd->data, cmd->len);
}

int local_echo(int flag)
{
    if (flag < 0)
        return sockecho();
    if (!xsock || !(xsock->flags & SOCKTELNET))
        return 0;

    if (flag != sockecho()) {
        /* request a change */
        SET_TELOPT(xsock, them_tog, TN_ECHO);
        if (flag)
            DONT(TN_ECHO);
        else
            DO(TN_ECHO);
    }
    return 1;
}


void transmit_window_size(void)
{
    Sock *sock;

    for (sock = hsock; sock; sock = sock->next)
        if (TELOPT(sock, us, TN_NAWS))
            do_naws(sock);
}

static void preferred_telnet_options(void)
{
    SET_TELOPT(xsock, us_tog, TN_NAWS);
    WILL(TN_NAWS);
#if 0
    SET_TELOPT(xsock, us_tog, TN_BINARY);
    WILL(TN_BINARY);		/* allow us to send 8-bit data */
    SET_TELOPT(xsock, them_tog, TN_BINARY);
    DO(TN_BINARY);		/* allow server to send 8-bit data */
#endif
}

/* Find the named world (current world if name is blank) or print an error */
World *named_or_current_world(const char *name)
{
    World *world;
    if (!(world = (*name) ? find_world(name) : xworld())) {
	eprintf("No world %s", name);
    }
    return world;
}

const char *world_info(const char *worldname, const char *fieldname)
{
    World *world;
    const char *result;
 
    world = worldname ? find_world(worldname) : xworld();
    if (!world) return ""; /* not an error */
 
    if (!fieldname || strcmp("name", fieldname) == 0) {
        result = world->name;
    } else if (strcmp("type", fieldname) == 0) {
        result = world_type(world);
    } else if (strcmp("character", fieldname) == 0) {
        result = world_character(world);
    } else if (strcmp("password", fieldname) == 0) {
        result = world_pass(world);
    } else if (strcmp("host", fieldname) == 0) {
        result = worldname ? world->host : world->sock->host;
    } else if (strcmp("port", fieldname) == 0) {
        result = worldname ? world->port : world->sock->port;
    } else if (strcmp("mfile", fieldname) == 0) {
        result = world_mfile(world);
    } else if (strcmp("login", fieldname) == 0) {
        result = world->sock && world->sock->flags & SOCKLOGIN ? "1" : "0";
    } else if (strcmp("proxy", fieldname) == 0) {
        result = world->sock && world->sock->flags & SOCKPROXY ? "1" : "0";
    } else if (strcmp("src", fieldname) == 0) {
        result = worldname ? world->myhost : world->sock->myhost;
    } else if (strcmp("cipher", fieldname) == 0) {
	result =
#if HAVE_SSL
	    (world->sock && world->sock->ssl) ?
	    SSL_get_cipher_name(world->sock->ssl) :
#endif
	    "";
    } else return NULL;
    return result ? result : "";
}

int is_open(const char *worldname)
{
    World *w;

    if (!worldname || !*worldname) {
        if (!xsock) return 0;
        w = xsock->world;
    } else if (!(w = find_world(worldname))) {
        return 0;
    }
    return (w->sock && (w->sock->constate >= SS_CONNECTED) &&
	(w->sock->constate < SS_ZOMBIE));
}

int is_connected(const char *worldname)
{
    World *w;

    if (!worldname || !*worldname) {
        if (!xsock) return 0;
        w = xsock->world;
    } else if (!(w = find_world(worldname))) {
        return 0;
    }
    return (w->sock && (w->sock->constate == SS_CONNECTED));
}

int nactive(const char *worldname)
{
    World *w;

    if (!worldname || !*worldname)
        return active_count;
    if (!(w = find_world(worldname)) || !w->sock)
        return 0;
    return w->screen->active ? w->screen->nnew : 0;
}

