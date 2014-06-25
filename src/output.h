/*************************************************************************
 *  TinyFugue - programmable mud client
 *  Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2002, 2003, 2004, 2005, 2006-2007 Ken Keys
 *
 *  TinyFugue (aka "tf") is protected under the terms of the GNU
 *  General Public License.  See the file "COPYING" for details.
 ************************************************************************/
/* $Id: output.h,v 35004.68 2007/01/13 23:12:39 kkeys Exp $ */

#ifndef OUTPUT_H
#define OUTPUT_H

#if TERMCAP
# define SCREEN
#endif
#if HARDCODE
# define SCREEN
#endif

/* refresh types */
typedef enum { REF_NONE, REF_PHYSICAL, REF_PROMPT, REF_LOGICAL } ref_type_t;

#define set_refresh_pending(type)	(need_refresh |= (type))
#define display_screen	(virtscreen ? fg_screen : default_screen)

extern int lines, columns;
extern struct timeval clock_update;	/* when to update clock */
extern ref_type_t need_refresh;		/* Input needs refresh? */
extern int need_more_refresh;		/* Visual more prompt needs refresh? */
extern struct timeval alert_timeout;	/* when to clear alert */
extern unsigned long alert_id;

extern void dobell(int n);
extern void init_output(void);
extern int  change_term(Var *var);
extern void setup_screen(void);
extern int  winlines(void);
extern int  redraw(void);
extern int  redraw_window(Screen *screen, int already_clear);
extern int  clear_display_screen(void);
extern void oflush(void);
extern int  tog_more(Var *var);
extern int  tog_keypad(Var *var);
extern int  clear_more(int new);
extern int  pause_screen(void);
extern int  ch_visual(Var *var);
extern int  ch_expnonvis(Var *var);
extern int  ch_wrap(Var *var);
extern int  ch_status_int(Var *var);
extern int  ch_status_fields(Var *var);
extern int  ch_status_height(Var *var);
extern void update_status_field(Var *var, stat_id_t internal);
extern void format_status_line(void);
extern int  display_status_line(void);
extern int  update_status_line(Var *var);
extern int handle_status_width_func(const char *name);
extern conString *status_field_text(int row);
extern void fix_screen(void);
extern void minimal_fix_screen(void);
extern void iput(int len);
extern void inewline(void);
extern void idel(int place);
extern int  igoto(int place);
extern int  dokey_page(void);
extern int  dokey_hpage(void);
extern int  dokey_line(void);
extern int  screen_end(int need_redraw);
extern int  selflush(void);
extern void do_refresh(void);
extern void logical_refresh(void);
extern void physical_refresh(void);
extern void reset_outcount(Screen *screen);
extern void enscreen(Screen *screen, conString *line);
extern void screenout(conString *line);
extern void update_prompt(conString *newprompt, int display);
extern int  wraplen(const char *str, int len, int indent);
extern const char *get_keycode(const char *name);

extern int moresize(Screen *screen);
extern int screen_has_filter(struct Screen *screen);
extern void clear_screen_filter(struct Screen *screen);
extern int enable_screen_filter(struct Screen *screen);
extern void set_screen_filter(struct Screen *screen, Pattern *pat,
    attr_t attr_flag, int sense);
extern void alert(conString *msg);
extern void clear_alert(void);


#if USE_DMALLOC
extern void   free_output(void);
#endif

#endif /* OUTPUT_H */
