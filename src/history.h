/*************************************************************************
 *  TinyFugue - programmable mud client
 *  Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2002, 2003, 2004, 2005, 2006-2007 Ken Keys
 *
 *  TinyFugue (aka "tf") is protected under the terms of the GNU
 *  General Public License.  See the file "COPYING" for details.
 ************************************************************************/
/* $Id: history.h,v 35004.30 2007/01/13 23:12:39 kkeys Exp $ */

#ifndef HISTORY_H
#define HISTORY_H

# if !NO_HISTORY

extern void   init_histories(void);
extern struct History *init_history(struct History *hist, int maxsize);
extern void   free_history(struct History *hist);
extern void   recordline(struct History *hist, conString *line);
extern void   record_input(const conString *line);
extern String*recall_input(int n, int mode);
extern int    is_watchdog(struct History *hist, String *line);
extern int    is_watchname(struct History *hist, String *line);
extern String*history_sub(String *line);
extern void   sync_input_hist(void);
extern int    do_recall(String *args, int offset);
extern long   hist_getsize(const struct History *w);

#if USE_DMALLOC
extern void   free_histories(void);
#endif

#define record_global(line)  recordline(globalhist, (line))
#define record_local(line)   recordline(localhist, (line))

extern struct History * const globalhist, * const localhist;
extern int log_count, norecord, nolog;

# else /* NO_HISTORY */

#define init_histories()               /* do nothing */
#define free_history(hist)             /* do nothing */
#define recordline(hist, line)         /* do nothing */
#define record_global(line)            /* do nothing */
#define record_local(line)             /* do nothing */
#define record_input(line, tv)         /* do nothing */
#define recall_history(args, file)     (eprintf("history disabled"), 0)
#define recall_input(n, mode)          (eprintf("history disabled"), 0)
#define check_watch(hist, line)        /* do nothing */
#define history_sub(pattern)           (0)
#define is_watchdog(hist, line)        (0)
#define is_watchname(hist, line)       (0)

#define log_count                      (0)
static int norecord = 0, nolog = 0;

# endif /* NO_HISTORY */

#endif /* HISTORY_H */
