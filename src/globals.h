/*************************************************************************
 *  TinyFugue - programmable mud client
 *  Copyright (C) 1996, 1997, 1998, 1999, 2002, 2003, 2004, 2005, 2006-2007 Ken Keys
 *
 *  TinyFugue (aka "tf") is protected under the terms of the GNU
 *  General Public License.  See the file "COPYING" for details.
 ************************************************************************/
/* $Id: globals.h,v 35000.78 2007/01/13 23:12:39 kkeys Exp $ */

#ifndef GLOBALS_H
#define GLOBALS_H

/*************************
 * Global user variables *
 *************************/

/* Note: order of arithmetic types defines how they are promoted */
typedef enum {
    TYPE_ID       = 0x00001,	/* identifier */
    TYPE_STR      = 0x00002,	/* String */
    TYPE_ENUM     = 0x00004,	/* enumerated */
    TYPE_POS      = 0x00008,	/* positive integer */
    TYPE_INT      = 0x00010,	/* integer */
    TYPE_DECIMAL  = 0x00020,	/* 32-bit integer + 6 decimal places */
    TYPE_DTIME    = 0x00040,	/* duration time (seconds and microseconds) */
    TYPE_ATIME    = 0x00080,	/* absolute time (seconds and microseconds) */
    TYPE_FLOAT    = 0x00100,	/* double */
    TYPE_FILE     = 0x00200,	/* tfile (internal use only) */
    TYPE_FUNC     = 0x00400,	/* resolved ExprFunc (internal only) */
    TYPE_CMD      = 0x00800,	/* resolved BuiltinCmd (internal only) */
    TYPE_REGEX    = 0x01000,	/* STR: regular expression (internal only) */
    TYPE_EXPR     = 0x02000,	/* STR: tf expression (internal only) */
    TYPE_ATTR     = 0x04000,	/* STR: attributes (internal only) */
    TYPE_REGMATCH = 0x08000,	/* INT: result of regmatch() (internal only) */
    TYPE_HMS      = 0x10000	/* TIME: was in H:M:S form (internal only) */
} type_t;

/* numeric types */
#define TYPE_EXACTNUM	(TYPE_INT | TYPE_DECIMAL | TYPE_DTIME | TYPE_ATIME)
#if NO_FLOAT
# define TYPE_NUM	(TYPE_EXACTNUM)
#else
# define TYPE_NUM	(TYPE_EXACTNUM | TYPE_FLOAT)
#endif

#define TYPES_BASIC \
    ( TYPE_ID | TYPE_STR | TYPE_ENUM | TYPE_POS | TYPE_NUM \
    | TYPE_FILE | TYPE_FUNC | TYPE_CMD )

typedef union ValueUnion {
    long ival;			/* integer value (ENUM, POS, INT) */
    double fval;		/* float value (FLOAT) */
    struct timeval tval;	/* time value (DTIME, ATIME, DECIMAL) */
    struct RegInfo *ri;		/* compiled regexp (STR|REGEX) */
    struct Program *prog;	/* compiled expression (STR|EXPR) */
    void *p;			/* other pointer type (FILE, FUNC, CMD) */
    unsigned int hash;		/* hash value (ID) */
    attr_t attr;		/* attributes (STR|ATTR) */
    struct Value *next;		/* valpool pointer */
} ValueUnion;

/* Most types use the union for their value, and cache a string value in sval.
 * A pure TYPE_STR does not use the union, but it does if type is or'd with
 * TYPE_REGEX, TYPE_EXPR, or TYPE_ATTR.
 */
typedef struct Value {
    const char *name;		/* identifier name (must be first member!) */
    type_t type;
    int count;			/* reference count */
    conString *sval;		/* string value (any type, not just STR) */
    ValueUnion u;
} Value;

extern Value *val_zero, *val_one, *val_blank;
/* shareval is a func, not a macro, so it doesn't eval v twice */
static inline Value *shareval(Value *v)	{ v->count++; return v;}

#define newval()	newval_fl(__FILE__, __LINE__)
#define newint(i)	newint_fl(i, __FILE__, __LINE__)
#define newtime(s, u, type)	newtime_fl(s, u, type, __FILE__, __LINE__)
#define newdecimal(s,u)	newtime_fl(s, u, TYPE_DECIMAL, __FILE__, __LINE__)
#define newdtime(s, u)	newtime_fl(s, u, TYPE_DTIME, __FILE__, __LINE__)
#define newatime(s, u)	newtime_fl(s, u, TYPE_ATIME, __FILE__, __LINE__)
#define newfloat(f)	newfloat_fl(f, __FILE__, __LINE__)
#define newid(id,l)	newid_fl(id, l, __FILE__, __LINE__)
#define newstr(s,l)	newstr_fl(s, l, __FILE__, __LINE__)
#define newSstr(S)	newSstr_fl(S, __FILE__, __LINE__)
#define newptr(ptr)	newptr_fl(ptr, __FILE__, __LINE__)
#define valval(val)	valval_fl(val, __FILE__, __LINE__)
#define clearval(val)	clearval_fl(val, __FILE__, __LINE__)

extern void clearval_fl(Value *val, const char *file, int line);
extern struct Value *newval_fl(const char *file, int line);
extern struct Value *newfloat_fl(double f, const char *file, int line);
extern struct Value *newint_fl(long i, const char *file, int line);
extern struct Value *newtime_fl(long s, long u, type_t type,
              const char *file, int line);
extern struct Value *newSstr_fl(conString *S, const char *file, int line);
extern struct Value *newstr_fl(const char *s, int len,
              const char *file, int line);
extern struct Value *newid_fl(const char *id, int len,
              const char *file, int line);
extern struct Value *valval_fl(Value *val, const char *file, int line);


typedef struct Var Var;
typedef int (Toggler)(Var *var);

struct Var {
    Value val;			/* value (must be first member!) */
    int flags;
    conString *enumvec;		/* list of valid enum values */
    Toggler *func;		/* called when value changes */
    struct ListEntry *node;	/* backpointer to node in list */
    short statuses;		/* # of status fields watching this var */
    short statusfmts;		/* # of status fields using this var as fmt */
    short statusattrs;		/* # of status fields using this var as attr */
};


enum Vars {
#define varcode(id, name, val, type, flags, enums, ival, uval, func)      id,
#include "varlist.h"
#undef varcode
    NUM_VARS
};


/* Convenient variable access.
 * The get* macros are READONLY; use set* functions to change a value.  The
 * cast enforces readonly-ness in standard C (gcc needs -pedantic to warn).
 */

#ifdef WORLD_VARS
# define get_special_var(id) \
    ((xsock && xsock->world->special_var[(id)]) ? \
        xsock->world->special_var[(id)] : \
	&special_var[(id)])
#else
# define get_special_var(id)	(&special_var[(id)])
#endif

#define strvar(id)	(get_special_var(id)->val.sval)
#define intvar(id)	(get_special_var(id)->val.u.ival)
#define timevar(id)	(get_special_var(id)->val.u.tval)
#define attrvar(id)	(get_special_var(id)->val.u.attr)

#define getattrvar(id)	((attr_t)	attrvar(id))
#define gettimevar(id)	(timevar(id))
#define getintvar(id)	((long)          intvar(id))
#define getstrvar(id)	((conString*)    strvar(id))
#define getstdvar(id)	((char*)         (strvar(id) ? strvar(id)->data : NULL))

#define MAIL		getstdvar(VAR_MAIL)
#define TERM		getstdvar(VAR_TERM)
#define TFLIBDIR	getstdvar(VAR_TFLIBDIR)
#define TFPATH		getstdvar(VAR_TFPATH)
#define TFMAILPATH	getstdvar(VAR_TFMAILPATH)
#define alert_attr	getattrvar(VAR_alert_attr)
#define alert_time	gettimevar(VAR_alert_time)
#define auto_fg		getintvar(VAR_auto_fg)
#define background	getintvar(VAR_background)
#define backslash	getintvar(VAR_backslash)
#define bamf		getintvar(VAR_bamf)
#define beep		getintvar(VAR_beep)
#define bg_output	getintvar(VAR_bg_output)
#define binary_eol	getintvar(VAR_binary_eol)
#define borg		getintvar(VAR_borg)
#define cecho		getintvar(VAR_cecho)
#define cleardone	getintvar(VAR_cleardone)
#define clearfull	getintvar(VAR_clearfull)
#define clock_flag	getintvar(VAR_clock)
#define defcompile	getintvar(VAR_defcompile)
#define emulation 	getintvar(VAR_emulation)
#define error_attr	getattrvar(VAR_error_attr)
#define expand_tabs 	getintvar(VAR_expand_tabs)
#define expnonvis 	getintvar(VAR_expnonvis)
#define gag		getintvar(VAR_gag)
#define async_name	getintvar(VAR_async_name)
#define async_conn	getintvar(VAR_async_conn)
#define gpri		getintvar(VAR_gpri)
#define hilite		getintvar(VAR_hilite)
#define hiliteattr	getattrvar(VAR_hiliteattr)
#define histsize	getintvar(VAR_histsize)
#define hookflag	getintvar(VAR_hook)
#define hpri		getintvar(VAR_hpri)
#define iecho		getintvar(VAR_iecho)
#define info_attr	getattrvar(VAR_info_attr)
#define insert		getintvar(VAR_insert)
#define interactive	getintvar(VAR_interactive)
#define isize		getintvar(VAR_isize)
#define istrip		getintvar(VAR_istrip)
#define kbnum		getstrvar(VAR_kbnum)
#define kecho		getintvar(VAR_kecho)
#define keepalive	getintvar(VAR_keepalive)
#define keypad		getintvar(VAR_keypad)
#define kprefix		getstrvar(VAR_kprefix)
#define login		getintvar(VAR_login)
#define lpflag		getintvar(VAR_lp)
#define lpquote		getintvar(VAR_lpquote)
#define maildelay	gettimevar(VAR_maildelay)
#define matching	getintvar(VAR_matching)
#define max_hook	getintvar(VAR_max_hook)
#define max_instr	getintvar(VAR_max_instr)
#define max_kbnum	getintvar(VAR_max_kbnum)
#define max_recur	getintvar(VAR_max_recur)
#define max_trig	getintvar(VAR_max_trig)
#define mccp		getintvar(VAR_mccp)
#define mecho		getintvar(VAR_mecho)
#define mecho_attr	getattrvar(VAR_mecho_attr)
#define meta_esc	getintvar(VAR_meta_esc)
#define more		getintvar(VAR_more)
#define mprefix		getstrvar(VAR_mprefix)
#define oldslash	getintvar(VAR_oldslash)
#define optimize_user	getintvar(VAR_optimize)
#define pedantic	getintvar(VAR_pedantic)
#define prompt_wait	gettimevar(VAR_prompt_wait)
#define proxy_host	getstdvar(VAR_proxy_host)
#define proxy_port	getstdvar(VAR_proxy_port)
#define process_time	gettimevar(VAR_ptime)
#define qecho		getintvar(VAR_qecho)
#define qprefix		getstrvar(VAR_qprefix)
#define quietflag	getintvar(VAR_quiet)
#define quitdone	getintvar(VAR_quitdone)
#define redef		getintvar(VAR_redef)
#define refreshtime	getintvar(VAR_refreshtime)
#define scroll		getintvar(VAR_scroll)
#define secho		getintvar(VAR_secho)
#define shpause		getintvar(VAR_shpause)
#define sigfigs		getintvar(VAR_sigfigs)
#define snarf		getintvar(VAR_snarf)
#define sockmload	getintvar(VAR_sockmload)
#define sprefix		getstrvar(VAR_sprefix)
#define status_attr	getattrvar(VAR_stat_attr)
#define status_fields	getstdvar(VAR_stat_fields)
#define status_height	getintvar(VAR_stat_height)
#define status_pad	getstdvar(VAR_stat_pad)
#define sub		getintvar(VAR_sub)
#define tabsize		getintvar(VAR_tabsize)
#define telopt		getintvar(VAR_telopt)
#define textdiv		getintvar(VAR_textdiv)
#define textdiv_str	getstrvar(VAR_textdiv_str)
#define tfhost		getstdvar(VAR_tfhost)
#define time_format	getstrvar(VAR_time_format)
#define virtscreen	getintvar(VAR_virtscreen)
#define warn_curly_re	getintvar(VAR_warn_curly_re)
#define warn_def_B	getintvar(VAR_warn_def_B)
#define warn_status	getintvar(VAR_warn_status)
#define warning_attr	getattrvar(VAR_warning_attr)
#define watchdog	getintvar(VAR_watchdog)
#define watchname	getintvar(VAR_watchname)
#define wordpunct	getstdvar(VAR_wordpunct)
#define wrapflag	getintvar(VAR_wrap)
#define wraplog		getintvar(VAR_wraplog)
#define wrappunct	getintvar(VAR_wrappunct)
#define wrapsize	getintvar(VAR_wrapsize)
#define wrapspace	getintvar(VAR_wrapspace)

/* visual is special: initial value of -1 indicates it was never explicitly
 * set, but is still treated like "off".
 */
#define visual		((long)(getintvar(VAR_visual) > 0))

extern Var special_var[];

#define reset_kbnum()	unsetvar(&special_var[VAR_kbnum])

#endif /* GLOBALS_H */
