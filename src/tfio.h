/*************************************************************************
 *  TinyFugue - programmable mud client
 *  Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2002, 2003, 2004, 2005, 2006-2007 Ken Keys
 *
 *  TinyFugue (aka "tf") is protected under the terms of the GNU
 *  General Public License.  See the file "COPYING" for details.
 ************************************************************************/
/* $Id: tfio.h,v 35004.66 2007/01/13 23:12:39 kkeys Exp $ */

#ifndef TFIO_H
#define TFIO_H

#ifdef _POSIX_VERSION
# include <sys/types.h>
# define MODE_T mode_t
#else
# define MODE_T unsigned long
#endif

#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef S_IROTH
# define S_IWUSR 00200
# define S_IRUSR 00400
# define S_IRGRP 00040
# define S_IROTH 00004
#endif

/* TFILE types */
typedef enum { TF_NULL, TF_QUEUE, TF_FILE, TF_PIPE } TFILE_type_t;

/* Sprintf flags */
#define SP_APPEND   1	/* don't truncate first, just append */
#define SP_CHECK    2	/* make sure char* args won't SIGSEGV or SIGBUS */

typedef struct PhysLine {
    conString *str;
    int start;
    short len;
    short indent;
    char visible; /* line passed screen_filter() */
    char tmp;     /* should only be displayed once */
} PhysLine;

typedef struct Queue {
    List list;
} Queue;

struct Screen {
    int outcount;		/* lines remaining until pause */
    struct List pline;		/* already displayed physical lines */
/* pline invariant: if one pline corresponding to an lline is in the list,
 * all plines corresponding to that lline are in the list. */
    int npline;			/* number of physical lines in pline */
    int nlline;			/* number of logical lines in pline */
    int maxlline;		/* max number of logical lines in pline */
    int nback;			/* number of lines scrolled back */
    int nnew;			/* number of new lines */
    int nback_filtered;		/* number of filtered lines scrolled back */
    int nnew_filtered;		/* number of filtered new lines */
    ListEntry *top, *bot;	/* top and bottom of view in plines */
    ListEntry *maxbot;		/* last line in plines that bot has reached */
    int viewsize;		/* # of plines between top and bot, inclusive */
    int scr_wrapflag;		/* wrapflag used to wrap plines */
    int scr_wrapsize;		/* wrapsize used to wrap plines */
    int scr_wrapspace;		/* wrapspace used to wrap plines */
    int scr_wrappunct;		/* wrappunct used to wrap plines */
    Pattern filter_pat;		/* filter pattern */
    char filter_enabled;	/* is filter enabled? */
    char filter_sense;		/* 0 = negative, 1 = positive */
    char filter_attr;		/* filter by attributes? */
    char selflush;		/* selective flushing flag */
    char needs_refilter;	/* top and bot need to be recalculated */
    char partialview;		/* do not expand viewsize to fit terminal */
    char paused;		/* paused at a More prompt? */
    char active;		/* has new lines without the A attribute? */
};

/* TF's analogue of stdio's FILE */
typedef struct TFILE {
    int id;
    struct ListEntry *node;
    TFILE_type_t type;
    char *name;
    union {
        Queue *queue;
        FILE *fp;
    } u;
    char buf[1024];
    int off, len;
    MODE_T mode;
    char tfmode;
    short warned;
    short autoflush;
} TFILE;


#ifndef HAVE_DRIVES
# define is_absolute_path(path) \
            ((path)[0] == '/' || (path)[0] == '~')
#else
# define is_absolute_path(path) \
            ((path)[0] == '/' || (path)[0] == '~' || \
            (is_alpha((path)[0]) && (path)[1] == ':'))
#endif


extern TFILE *loadfile;    /* currently /load'ing file */
extern int    loadline;    /* line number of /load'ing file */
extern int    loadstart;   /* line number of command start in /load'ing file */
extern TFILE *tfin;        /* tf input queue */
extern TFILE *tfout;       /* tf output queue */
extern TFILE *tferr;       /* tf error queue */
extern TFILE *tfalert;     /* tf alert file */
extern TFILE *tfkeyboard;  /* keyboard, where tfin usually points */
extern TFILE *tfscreen;    /* screen, where tfout & tferr usually point */
extern Screen*fg_screen;   /* current screen to which tf writes */
extern Screen*default_screen; /* default screen (unconnected or !virtscreen) */
extern int    read_depth;  /* depth of user kb reads */
extern int    readsafe;    /* safe to to a user kb read? */
extern PhysLine *plpool;   /* freelist of PhysLines */

#define operror(str)    eprintf("%s: %s", str, strerror(errno))
#define oputline(line)  tfputline(line, tfout)
#define tfputs(str, f)  tfnputs(str, -1, f)
#define oputs(str)      tfputs(str, tfout)
#define eputs(str)      tfputs(str, tferr)
#define tfputc(c, file) fputc((c), (file)->u.fp)
#define tfflush(file) \
    ((file->type==TF_FILE || file->type==TF_PIPE) ? fflush((file)->u.fp) : 0)

extern void   init_tfio(void);
extern Screen*new_screen(long size);
extern void   free_screen_lines(Screen *screen);
extern void   free_screen(Screen *screen);
extern char  *tfname(const char *name, const char *macname);
extern char  *expand_filename(const char *str);
extern TFILE *tfopen(const char *name, const char *mode);
extern int    tfclose(TFILE *file);
extern void   tfnputs(const char *str, int n, TFILE *file);
extern attr_t tfputansi(const char *str, TFILE *file, attr_t attrs);
extern int    tfputp(const char *str, TFILE *file);
extern void   tfputline(struct conString *line, TFILE *file);
extern void   vSprintf(struct String *buf, int flags,
                     const char *fmt, va_list ap);
extern void   Sprintf(struct String *buf, const char *fmt, ...)
		     format_printf(3, 4);
extern void   Sappendf(struct String *buf, const char *fmt, ...)
		     format_printf(2, 3);
extern void   oprintf(const char *fmt, ...) format_printf(1, 2);
extern void   tfprintf(TFILE *file, const char *fmt, ...)
                     format_printf(2, 3);
extern void   eprefix(String *buffer);
extern void   eprintf(const char *fmt, ...) format_printf(1, 2);
extern void   wprintf(const char *fmt, ...) format_printf(1, 2);
extern char   igetchar(void);
extern int    handle_tfopen_func(const char *name, const char *mode);
extern TFILE *find_tfile(const char *handle);
extern TFILE *find_usable_tfile(const char *handle, int mode);
extern int    tfreadable(TFILE *file);
extern struct String *tfgetS(struct String *str, TFILE *file);

extern void   hide_screen(Screen *screen);
extern void   unhide_screen(Screen *screen);
extern void   switch_screen(int quiet);

#endif /* TFIO_H */
