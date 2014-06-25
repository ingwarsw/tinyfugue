/*************************************************************************
 *  TinyFugue - programmable mud client
 *  Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2002, 2003, 2004, 2005, 2006-2007 Ken Keys
 *
 *  TinyFugue (aka "tf") is protected under the terms of the GNU
 *  General Public License.  See the file "COPYING" for details.
 ************************************************************************/
/* $Id: tty.h,v 35004.15 2007/01/13 23:12:39 kkeys Exp $ */

#ifndef TTY_H
#define TTY_H

extern void init_tty(void);
extern void cbreak_noecho_mode(void);
extern void reset_tty(void);
extern int  get_window_size(void);

extern int no_tty;

#endif /* TTY_H */
