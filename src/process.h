/*************************************************************************
 *  TinyFugue - programmable mud client
 *  Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2002, 2003, 2004, 2005, 2006-2007 Ken Keys
 *
 *  TinyFugue (aka "tf") is protected under the terms of the GNU
 *  General Public License.  See the file "COPYING" for details.
 ************************************************************************/
/* $Id: process.h,v 35004.20 2007/01/13 23:12:39 kkeys Exp $ */

#ifndef PROCESS_H
#define PROCESS_H

# if !NO_PROCESS

extern void kill_procs_by_world(struct World *world);
extern void kill_procs(void);
extern void nuke_dead_procs(void);
extern void runall(int prompted, struct World *world);
extern int  ch_lpquote(Var *var);

extern struct timeval proctime;		/* when next process should run */

# else

#define kill_procs_by_world(world)	/* do nothing */
#define kill_procs()			/* do nothing */
#define nuke_dead_procs()		/* do nothing */
#define runall(prompted, world)		/* do nothing */
#define ch_lpquote			NULL
#define proctime tvzero

# endif /* NO_PROCESS */

#endif /* PROCESS_H */
