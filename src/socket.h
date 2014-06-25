/*************************************************************************
 *  TinyFugue - programmable mud client
 *  Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2002, 2003, 2004, 2005, 2006-2007 Ken Keys
 *
 *  TinyFugue (aka "tf") is protected under the terms of the GNU
 *  General Public License.  See the file "COPYING" for details.
 ************************************************************************/
/* $Id: socket.h,v 35004.44 2007/01/13 23:12:39 kkeys Exp $ */

#ifndef SOCKET_H
#define SOCKET_H

/* socktime ids */
#define SOCK_RECV	0
#define SOCK_SEND	1

/* /connect flags */
#define CONN_AUTOLOGIN	0x01
#define CONN_QUIETLOGIN	0x02
#define CONN_SSL	0x04
#define CONN_BG		0x08
#define CONN_FG		0x10

struct World   *world_decl;	/* declares struct World */

extern String *incoming_text;
extern int quit_flag;
extern struct Sock *xsock;

extern void    main_loop(void);
extern void    init_sock(void);
extern int     sockecho(void);
extern int     is_active(int fd);
extern void    readers_clear(int fd);
extern void    readers_set(int fd);
extern struct timeval *socktime(const char *name, int dir);
extern int     tog_bg(Var *var);
extern int     tog_keepalive(Var *var);
extern int     openworld(const char *name, const char *port, int flags);
extern void    world_output(struct World *world, conString *line);
extern int     send_line(const char *s, unsigned int len, int eol_flag);
extern conString *fgprompt(void);
extern int     tog_lp(Var *var);
extern void    transmit_window_size(void);
extern int     local_echo(int flag);
extern int     handle_send_function(conString *string, const char *world,
                     const char *flags);
extern int     handle_fake_recv_function(conString *string, const char *world,
		    const char *flags);
extern int     is_connected(const char *worldname);
extern int     is_open(const char *worldname);
extern int     nactive(const char *worldname);
extern int     world_hook(const char *fmt, const char *name);

extern struct World *xworld(void);
extern int	     xsock_is_fg(void);
extern int	     have_active_socks(void);
extern void          xsock_alert_id(void);
extern const char   *fgname(void);
extern const char   *world_info(const char *worldname, const char *fieldname);
extern struct World *named_or_current_world(const char *name);

#endif /* SOCKET_H */
