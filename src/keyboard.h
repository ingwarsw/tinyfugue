/*************************************************************************
 *  TinyFugue - programmable mud client
 *  Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2002, 2003, 2004, 2005, 2006-2007 Ken Keys
 *
 *  TinyFugue (aka "tf") is protected under the terms of the GNU
 *  General Public License.  See the file "COPYING" for details.
 ************************************************************************/
/* $Id: keyboard.h,v 35004.23 2007/01/13 23:12:39 kkeys Exp $ */

#ifndef KEYBOARD_H
#define KEYBOARD_H

extern struct timeval keyboard_time;
extern int keyboard_pos;
extern Stringp keybuf;
extern int pending_line, pending_input;

extern void          init_keyboard(void);
extern int           bind_key(Macro *spec, const char *key);
extern void          unbind_key(const char *key);
extern struct Macro *find_key(const char *key);
extern int           do_kbdel(int place);
extern int           do_kbword(int start, int dir);
extern int           do_kbmatch(int start);
extern int           handle_keyboard_input(int read_flag);
extern int           handle_input_line(void);

#if USE_DMALLOC
extern void   free_keyboard(void);
#endif

#endif /* KEYBOARD_H */
