/*************************************************************************
 *  TinyFugue - programmable mud client
 *  Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2002, 2003, 2004, 2005, 2006-2007 Ken Keys
 *
 *  TinyFugue (aka "tf") is protected under the terms of the GNU
 *  General Public License.  See the file "COPYING" for details.
 ************************************************************************/
/* $Id: world.h,v 35004.32 2007/01/13 23:12:39 kkeys Exp $ */

#ifndef WORLD_H
#define WORLD_H

#define WORLD_TEMP	001
#define WORLD_NOPROXY	002
#define WORLD_SSL	004
#define WORLD_ECHO	010

struct World {		/* World structure */
    char *name;			/* name of world - first, for cstrpppcmp */
    int flags;
    struct World *next;
    char *character;		/* login name */
    char *pass;			/* password */
    char *host;			/* server host name */
    char *port;			/* server port number or service name */
    char *myhost;		/* client host name */
    char *mfile;		/* macro file */
    char *type;			/* user-defined server type (tiny, lp...) */
    struct Sock *sock;		/* open socket, if any */
    List triglist[1];		/* trigger macros for this world */
    List hooklist[1];		/* hook macros for this world */
    Screen *screen;		/* displayed and undisplayed text */
    void *md;			/* mmalloc descriptor */
#if !NO_HISTORY
    struct History *history;	/* history and logging info */
#endif
#ifdef WORLD_VARS
    Var *special_vars[NUM_VARS];
#endif
};

extern World *defaultworld;

/* Macros to get string field from world or defaultworld */
#define world_type(w) \
   (w->type ? w->type : defaultworld ? defaultworld->type : NULL)
#define world_character(w) \
   (w->character ? w->character : defaultworld ? defaultworld->character : NULL)
#define world_pass(w) \
   (w->pass ? w->pass : defaultworld ? defaultworld->pass : NULL)
#define world_mfile(w) \
   (w->mfile ? w->mfile : defaultworld ? defaultworld->mfile : NULL)


extern World *new_world(const char *name, const char *type,
    const char *host, const char *port,
    const char *character, const char *pass,
    const char *mfile, int flags,
    const char *myhost);
extern int    nuke_world(World *w);
extern World *find_world(const char *name);
extern void   mapworld(void (*func)(struct World *world));

#if USE_DMALLOC
extern void   free_worlds(void);
#endif

#endif /* WORLD_H */
