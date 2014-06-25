/*************************************************************************
 *  TinyFugue - programmable mud client
 *  Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2002, 2003, 2004, 2005, 2006-2007 Ken Keys
 *
 *  TinyFugue (aka "tf") is protected under the terms of the GNU
 *  General Public License.  See the file "COPYING" for details.
 ************************************************************************/
/* $Id: signals.h,v 35004.21 2007/01/13 23:12:39 kkeys Exp $ */

#ifndef SIGNALS_H
#define SIGNALS_H

extern void init_signals(void);
extern void init_exename(char *name);
extern void process_signals(void);
extern int  shell_status(int result);
extern int  shell(const char *cmd);
extern int  suspend(void);
extern int  interrupted(void);
extern void crash(int internal, const char *fmt,
    const char *file, int line, long n) NORET;
extern void close_all(void); /* defined in socket.c */
extern const char *checkstring(const char *s);

#define core(fmt, file, line, n)	crash(TRUE, fmt, file, line, n)
#define error_exit(fmt, file, line, n)	crash(FALSE, fmt, file, line, n)

#endif /* SIGNALS_H */
