/*************************************************************************
 *  TinyFugue - programmable mud client
 *  Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2002, 2003, 2004, 2005, 2006-2007 Ken Keys
 *
 *  TinyFugue (aka "tf") is protected under the terms of the GNU
 *  General Public License.  See the file "COPYING" for details.
 ************************************************************************/
static const char RCSid[] = "$Id: output.c,v 35004.242 2007/01/14 00:44:19 kkeys Exp $";


/*****************************************************************
 * Fugue output handling
 *
 * Written by Ken Keys (Hawkeye).
 * Handles all screen-related phenomena.
 *****************************************************************/

#define TERM_vt100	1
#define TERM_vt220	2
#define TERM_ansi	3

#include "tfconfig.h"
#include "port.h"
#include "tf.h"
#include "util.h"
#include "pattern.h"
#include "search.h"
#include "tfio.h"
#include "socket.h"	/* fgprompt(), fgname() */
#include "output.h"
#include "attr.h"
#include "macro.h"	/* add_ibind(), rebind_key_macros() */
#include "tty.h"	/* init_tty(), get_window_wize() */
#include "variable.h"
#include "expand.h"	/* current_command */
#include "parse.h"	/* expr_value_safe() */
#include "keyboard.h"	/* keyboard_pos */
#include "cmdlist.h"

#ifdef EMXANSI
# define INCL_VIO
# include <os2.h>
#endif

/* Terminal codes and capabilities.
 * Visual mode requires at least clear_screen and cursor_address.  The other
 *   codes are good to have, but are not strictly necessary.
 * Scrolling in visual mode requires set_scroll_region (preferred), or
 *   insert_line and delete_line (may appear jumpy), or EMXANSI.
 * Fast insert in visual mode requires insert_start and insert_end (preferred),
 *   or insert_char.
 * Fast delete in visual mode requires delete_char.
 * Attributes require the appropriate attribute code and attr_off; but if
 *   attr_off is not defined, underline and standout (replacement for bold)
 *   can still be used if underline_off and standout_off are defined.
 */

#define DEFAULT_LINES   24
#define DEFAULT_COLUMNS 80

#if HARDCODE
# define origin 1      /* top left corner is (1,1) */
# if HARDCODE == TERM_vt100
#  define TERMCODE(id, vt100, vt220, ansi)   static const char *(id) = (vt100);
# elif HARDCODE == TERM_vt220
#  define TERMCODE(id, vt100, vt220, ansi)   static const char *(id) = (vt220);
# elif HARDCODE == TERM_ansi
#  define TERMCODE(id, vt100, vt220, ansi)   static const char *(id) = (ansi);
# endif
#else /* !HARDCODE */
# define origin 0      /* top left corner is (0,0) */
# define TERMCODE(id, vt100, vt220, ansi)   static const char *(id) = NULL;
#endif /* HARDCODE */

/*				vt100		vt220		ansi */
/*				-----		-----		---- */
#ifdef __CYGWIN32__  /* "\033[J" is broken in CYGWIN32 b18. */
TERMCODE (clear_screen,		NULL,		NULL,		NULL)
TERMCODE (clear_to_eos,		NULL,		NULL,		NULL)
#else
TERMCODE (clear_screen,		"\033[H\033[J", "\033[H\033[J", "\033[H\033[J")
TERMCODE (clear_to_eos,		"\033[J",	"\033[J",	"\033[J")
#endif
TERMCODE (clear_to_eol,		"\033[K",	"\033[K",	"\033[K")
TERMCODE (cursor_address,	"\033[%d;%dH",	"\033[%d;%dH",	"\033[%d;%dH")
TERMCODE (enter_ca_mode,	NULL,		NULL,		NULL)
TERMCODE (exit_ca_mode,		NULL,		NULL,		NULL)
TERMCODE (set_scroll_region,	"\033[%d;%dr",	"\033[%d;%dr",	NULL)
TERMCODE (insert_line,		NULL,		"\033[L",	"\033[L")
TERMCODE (delete_line,		NULL,		"\033[M",	"\033[M")
TERMCODE (delete_char,		NULL,		"\033[P",	"\033[P")
#ifdef __CYGWIN32__  /* "\033[@" is broken in CYGWIN32 b18. */
TERMCODE (insert_char,		NULL,		NULL,		NULL)
#else
TERMCODE (insert_char,		NULL,		"\033[@",	"\033[@")
#endif
TERMCODE (insert_start,		NULL,		NULL,		"\033[4h")
TERMCODE (insert_end,		NULL,		NULL,		"\033[4l")
TERMCODE (keypad_on,		"\033[?1h\033=",NULL,		NULL)
TERMCODE (keypad_off,		"\033[?1l\033>",NULL,		NULL)
TERMCODE (bell,			"\007",		"\007",		"\007")
TERMCODE (underline,		"\033[4m",	"\033[4m",	"\033[4m")
TERMCODE (reverse,		"\033[7m",	"\033[7m",	"\033[7m")
TERMCODE (flash,		"\033[5m",	"\033[5m",	"\033[5m")
TERMCODE (dim,			NULL,		NULL,		NULL)
TERMCODE (bold,			"\033[1m",	"\033[1m",	"\033[1m")
TERMCODE (attr_off,		"\033[m",	"\033[m",	"\033[m")
TERMCODE (attr_on,		NULL,		NULL,		NULL)
/* these are only used if others are missing */
TERMCODE (underline_off,	NULL,		NULL,		NULL)
TERMCODE (standout,		NULL,		NULL,		NULL)
TERMCODE (standout_off,		NULL,		NULL,		NULL)

/* end HARDCODE section */


/* If var==NULL and internal<0, the status is a constant string */
typedef struct statusfield {
    char *name;
    stat_id_t internal;	/* index of internal status being watched */
    Var *var;		/* variable being watched */
    Var *fmtvar;	/* variable containing format expression */
    Var *attrvar;	/* variable containing attribute string */
    int width;
    int rightjust;
    int column;
    int row;
    attr_t attrs;	/* attibutes from status_fields */
    attr_t vattrs;	/* attibutes from status_attr_{int,var}_<name> */
} StatusField;

static Var bogusvar;   /* placeholder for StatusField->var */

#define true_status_pad (status_pad && *status_pad ? *status_pad : ' ')
#define sidescroll \
    (intvar(VAR_sidescroll) <= Wrap/2 ? intvar(VAR_sidescroll) : Wrap/2)

static void  init_term(void);
static int   fbufputc(int c);
static void  bufflush(void);
static void  tbufputs(const char *str);
static void  tdirectputs(const char *str);
static void  xy(int x, int y);
static void  clr(void);
static void  clear_line(void);
static void  clear_input_line(void);
static void  clear_lines(int start, int end);
static void  clear_input_window(void);
static void  setscroll(int top, int bottom);
static void  scroll_input(int n);
static void  ictrl_put(const char *s, int n);
static int   ioutputs(const char *str, int len);
static void  ioutall(int kpos);
static void  attributes_off(attr_t attrs);
static void  attributes_on(attr_t attrs);
static void  color_on(const char *prefix, long color);
static void  hwrite(conString *line, int start, int len, int indent);
static int   check_more(Screen *screen);
static int   next_physline(Screen *screen);
static void  output_novisual(PhysLine *pl);
#ifdef SCREEN
static void  output_noscroll(PhysLine *pl);
static void  output_scroll(PhysLine *pl);
#endif

static void  (*tp)(const char *str);

#if TERMCAP
#define tpgoto(seq,x,y)  tp(tgoto(seq, (x)-1+origin, (y)-1+origin))
#else
#define tpgoto(seq,x,y)  Sappendf(outbuf,seq,(y)-1+origin,(x)-1+origin)
#endif

#define ipos()		xy(ix, iy)

/* Buffered output */

#define bufputStr(Str)		SStringcat(outbuf, Str)
#define bufputs(s)		Stringcat(outbuf, s)
#define bufputns(s, n)		Stringncat(outbuf, s, n)
#define bufputc(c)		Stringadd(outbuf, c)
#define bufputnc(c, n)		Stringnadd(outbuf, c, n)

#ifdef EMXANSI /* OS2 */
   static void crnl(int n);  
#else
# if USE_SGTTY  /* CRMOD is off (tty.c) */
#  define crnl(count)  do { bufputc('\r'); bufputnc('\n', count); } while (0)
# else             /* ONLCR is on (tty.c) */
#  define crnl(count)  bufputnc('\n', count)
# endif
#endif


/* Others */

#define Wrap (wrapsize ? wrapsize : columns)
#define moremin 1
#define morewait 50

String status_line[][1] = {	    /* formatted status lines, without alert */
    STATIC_BUFFER_INIT, STATIC_BUFFER_INIT, STATIC_BUFFER_INIT,
    STATIC_BUFFER_INIT, STATIC_BUFFER_INIT, STATIC_BUFFER_INIT };
#define max_status_height   (sizeof(status_line)/sizeof(String))
static StatusField *variable_width_field[max_status_height];

STATIC_BUFFER(outbuf);              /* output buffer */
static int top_margin = -1, bottom_margin = -1;	/* scroll region */
static int cx = -1, cy = -1;        /* Real cursor ((-1,-1)==unknown) */
static int ox = 1, oy = 1;          /* Output cursor */
static int ix, iy;                  /* Input cursor */
static int old_ix = -1;		    /* original ix before output clobbered it */
static int in_top, in_bot;	    /* top & bottom line # of input window */
#define out_top 1		    /* top line # of output window */
static int out_bot;		    /* bottom line # of output window */
static int stat_top, stat_bot;	    /* top & bottom line # of status area */
static int istarty, iendy, iendx;   /* start/end of current input line */
static conString *prompt;           /* current prompt */
static attr_t have_attr = 0;        /* available attributes */
static int screen_mode = -1;        /* -1=unset, 0=nonvisual, 1=visual */
static int output_disabled = 1;     /* is it safe to oflush()? */
static int can_have_visual = FALSE;
static int can_have_expnonvis = FALSE;
static List statusfield_list[max_status_height][1];
static int status_left[max_status_height];  /* size of status line left part */
static int status_right[max_status_height]; /* size of status line right part */
static int alert_row = 0;	    /* row of status area where alert appears */
static int alert_pos = 0;	    /* column where alert appears */
static int alert_len = 0;	    /* length of current alert message */

STATIC_STRING(moreprompt, "--More--", F_BOLD | F_REVERSE);  /* pager prompt */

#ifndef EMXANSI
# define has_scroll_region (set_scroll_region != NULL)
#else
# define has_scroll_region (1)
#endif


#if HARDCODE
# if HARDCODE == TERM_vt100
#  define KEYCODE(vt100, vt220, ansi)   (vt100)
# else
#  if HARDCODE == TERM_vt220
#   define KEYCODE(vt100, vt220, ansi)   (vt220)
#  else
#   if HARDCODE == TERM_ansi
#    define KEYCODE(vt100, vt220, ansi)   (ansi)
#   endif
#  endif
# endif
#else
# define KEYCODE(vt100, vt220, ansi)   NULL
#endif

typedef struct Keycode {
    const char *name, *capname, *code;
} Keycode;

static Keycode keycodes[] = {  /* this list is sorted by tolower(name)! */
/*                                     vt100       vt220       ansi      */
/*                                     -----       -----       ----      */
/*  { "Back Tab",       "kB", KEYCODE( NULL,       NULL,       NULL ) }, */
    { "Backspace",      "kb", KEYCODE( "\010",     "\010",     "\010" ) },
/*  { "Clear All Tabs", "ka", KEYCODE( NULL,       NULL,       NULL ) }, */
    { "Clear EOL",      "kE", KEYCODE( NULL,       NULL,       NULL ) },
    { "Clear EOS",      "kS", KEYCODE( NULL,       NULL,       NULL ) },
    { "Clear Screen",   "kC", KEYCODE( NULL,       NULL,       NULL ) },
/*  { "Clear Tab",      "kt", KEYCODE( NULL,       NULL,       NULL ) }, */
    { "Delete",         "kD", KEYCODE( NULL,       "\033[3~",  NULL ) },
    { "Delete Line",    "kL", KEYCODE( NULL,       NULL,       NULL ) },
    { "Down",           "kd", KEYCODE( "\033OB",   "\033[B",   "\033[B" ) },
/*  { "End Insert",     "kM", KEYCODE( NULL,       NULL,       NULL ) }, */
    { "F0",             "k0", KEYCODE( "\033Oy",   NULL,       NULL ) },
    { "F1",             "k1", KEYCODE( "\033OP",   "\033OP",   "\033[M" ) },
    { "F10",            "k;", KEYCODE( NULL,       NULL,       NULL ) },
    { "F11",            "F1", KEYCODE( NULL,       NULL,       NULL ) },
    { "F12",            "F2", KEYCODE( NULL,       NULL,       NULL ) },
    { "F13",            "F3", KEYCODE( NULL,       NULL,       NULL ) },
    { "F14",            "F4", KEYCODE( NULL,       NULL,       NULL ) },
    { "F15",            "F5", KEYCODE( NULL,       NULL,       NULL ) },
    { "F16",            "F6", KEYCODE( NULL,       NULL,       NULL ) },
    { "F17",            "F7", KEYCODE( NULL,       NULL,       NULL ) },
    { "F18",            "F8", KEYCODE( NULL,       NULL,       NULL ) },
    { "F19",            "F9", KEYCODE( NULL,       NULL,       NULL ) },
    { "F2",             "k2", KEYCODE( "\033OQ",   "\033OQ",   "\033[N" ) },
    { "F3",             "k3", KEYCODE( "\033OR",   "\033OR",   "\033[O" ) },
    { "F4",             "k4", KEYCODE( "\033OS",   "\033OS",   "\033[P" ) },
    { "F5",             "k5", KEYCODE( "\033Ot",   "\033[17~", "\033[Q" ) },
    { "F6",             "k6", KEYCODE( "\033Ou",   "\033[18~", "\033[R" ) },
    { "F7",             "k7", KEYCODE( "\033Ov",   "\033[19~", "\033[S" ) },
    { "F8",             "k8", KEYCODE( "\033Ol",   "\033[20~", "\033[T" ) },
    { "F9",             "k9", KEYCODE( "\033Ow",   "\033[21~", "\033[U" ) },
    { "Home",           "kh", KEYCODE( NULL,       "\033[H",   "\033[H" ) },
    { "Home Down",      "kH", KEYCODE( NULL,       NULL,       NULL ) },
    { "Insert",         "kI", KEYCODE( NULL,       "\033[2~",  "\033[L" ) },
    { "Insert Line",    "kA", KEYCODE( NULL,       NULL,       NULL ) },
    { "KP1",            "K1", KEYCODE( "\033Oq",   NULL,       NULL ) },
    { "KP2",            "K2", KEYCODE( "\033Or",   NULL,       NULL ) },
    { "KP3",            "K3", KEYCODE( "\033Os",   NULL,       NULL ) },
    { "KP4",            "K4", KEYCODE( "\033Op",   NULL,       NULL ) },
    { "KP5",            "K5", KEYCODE( "\033Oq",   NULL,       NULL ) },
    { "Left",           "kl", KEYCODE( "\033OD",   "\033[D",   "\033[D" ) },
    { "PgDn",           "kN", KEYCODE( NULL,       "\033[6~",  NULL ) },
    { "PgUp",           "kP", KEYCODE( NULL,       "\033[5~",  NULL ) },
    { "Right",          "kr", KEYCODE( "\033OC",   "\033[C",   "\033[C" ) },
    { "Scroll Down",    "kF", KEYCODE( NULL,       NULL,       NULL ) },
    { "Scroll Up",      "kR", KEYCODE( NULL,       NULL,       NULL ) },
/*  { "Set Tab",        "kT", KEYCODE( NULL,       NULL,       NULL ) }, */
    { "Up",             "ku", KEYCODE( "\033OA",   "\033[A",   "\033[A" ) }
};

#define N_KEYCODES	(sizeof(keycodes)/sizeof(Keycode))

int lines   = DEFAULT_LINES;
int columns = DEFAULT_COLUMNS;
ref_type_t need_refresh = 0;        /* does input need refresh? */
int need_more_refresh = 0;          /* does visual more prompt need refresh? */
struct timeval alert_timeout = { 0, 0 };    /* when to clear alert */
unsigned long alert_id = 0;
struct timeval clock_update ={0,0}; /* when clock needs to be updated */
PhysLine *plpool = NULL;	    /* freelist of PhysLines */

#if TERMCAP
extern int   tgetent(char *buf, const char *name);
extern int   tgetnum(const char *id);
extern char *tgetstr(const char *id, char **area);
extern char *tgoto(const char *code, int destcol, int destline);
extern char *tparm(const char *code, ...);
extern int   tputs(const char *cp, int affcnt, int (*outc)(int));
static int   func_putchar(int c);
#endif

/****************************
 * BUFFERED OUTPUT ROUTINES *
 ****************************/

static void bufflush(void)
{
    int written = 0, result, n;
    while (written < outbuf->len) {
	n = outbuf->len - written;
	if (n > 2048)
	    n = 2048;
        result = write(STDOUT_FILENO, outbuf->data + written, n);
	if (result < 0) break;
	written += result;
    }
    Stringtrunc(outbuf, 0);
}

static int fbufputc(int c)
{
    Stringadd(outbuf, c);
    return c;
}

#ifdef EMXANSI
void crnl(int n)
{
    int off = (cy + n) - bottom_margin;
    if (off < 0 || !visual) off = 0;
    bufputnc('\n', n - off);
    if (off) {
        bufflush();
        VioScrollUp(top_margin-1, 0, bottom_margin-1, columns, off, " \x07", 0);
        bufputc('\r');
    }
}
#endif

void dobell(int n)
{
    if (beep) {
        while (n--) bufputs(bell);
        bufflush();
    }
}

/********************/

int change_term(Var *var)
{
    fix_screen();
    init_term();
    redraw();
    rebind_key_macros();
    return 1;
}

static void init_line_numbers(void)
{
    /* out_top = 1; */
    out_bot = lines - isize - status_height;
    stat_top = out_bot + 1;
    stat_bot = out_bot + status_height;
    in_top = stat_bot + 1;
    in_bot = stat_bot + isize;
}

/* Initialize output data. */
void init_output(void)
{
    const char *str;
    int i;

    tp = tbufputs;
    init_tty();

    /* Window size: clear defaults, then try:
     * environment, ioctl TIOCGWINSZ, termcap, defaults.
     */
    lines = ((str = getvar("LINES"))) ? atoi(str) : 0;
    columns = ((str = getvar("COLUMNS"))) ? atoi(str) : 0;
    if (lines <= 0 || columns <= 0) get_window_size();
    init_line_numbers();
    top_margin = 1;
    bottom_margin = lines;

    prompt = fgprompt();
    old_ix = -1;

    init_term();
    for (i = 0; i < max_status_height; i++) {
	init_list(statusfield_list[i]);
	Stringninit(status_line[i], columns);
	check_charattrs(status_line[i], columns, 0, __FILE__, __LINE__);
    }

    redraw();
    output_disabled = 0;
}

/********************
 * TERMCAP ROUTINES *
 ********************/

static void init_term(void)
{
#if TERMCAP
    int i;
    /* Termcap entries are supposed to fit in 1024 bytes.  But, if a 'tc'
     * field is present, some termcap libraries will just append the second
     * entry to the original.  Also, some overzealous versions of tset will
     * also expand 'tc', so there might be *2* entries appended to the
     * original.  And, newer termcap databases have additional fields (e.g.,
     * linux adds 'li' and 'co'), so it could get even longer.  To top it all
     * off, some termcap libraries don't do any length checking in tgetent().
     * We should be safe with 4096.
     */
    char termcap_entry[4096];
    /* termcap_buffer will hold at most 1 copy of any field; 1024 is enough. */
    static char termcap_buffer[1024];
    char *area = termcap_buffer;

    have_attr = 0;
    can_have_visual = FALSE;
    can_have_expnonvis = FALSE;
    clear_screen = clear_to_eos = clear_to_eol = NULL;
    set_scroll_region = insert_line = delete_line = NULL;
    delete_char = insert_char = insert_start = insert_end = NULL;
    enter_ca_mode = exit_ca_mode = cursor_address = NULL;
    keypad_on = keypad_off = NULL;
    standout = underline = reverse = flash = dim = bold = bell = NULL;
    standout_off = underline_off = attr_off = attr_on = NULL;

    {
	/* Sanity check:  a valid termcap entry should end in ':'.  In
	 * particular, csh and tcsh put a limit of 1023 bytes on the TERMCAP
	 * variable, which may cause libpcap to give us garbage.  (I really
	 * hate it when people blame tf for a bug in another program,
	 * especially when it's the evil csh or tcsh.)
	 */
	const char *str, *shell;
	int len, is_csh;
	if ((str = getvar("TERMCAP")) && (len = strlen(str)) > 0) {
	    is_csh = ((shell = getenv("SHELL")) && smatch("*csh", shell) == 0);
	    if (str[len-1] != ':') {
		wprintf("unsetting invalid TERMCAP variable%s.",
		    is_csh ?  ", which appears to have been corrupted by your "
		    "broken *csh shell" : "");
		unsetvar(ffindglobalvar("TERMCAP"));
	    } else if (len == 1023 && (!getenv("TF_FORCE_TERMCAP")) && is_csh) {
		wprintf("unsetting the TERMCAP environment variable because "
		    "it looks like it has been truncated by your broken *csh "
		    "shell.  To force TF to use TERMCAP, restart TF with the "
		    "TF_FORCE_TERMCAP environment variable set.");
		unsetvar(ffindglobalvar("TERMCAP"));
	    }
	}
    }

    if (!TERM || !*TERM) {
        wprintf("TERM undefined.");
    } else if (tgetent(termcap_entry, TERM) <= 0) {
        wprintf("\"%s\" terminal unsupported.", TERM);
    } else {
        if (columns <= 0) columns = tgetnum("co");
        if (lines   <= 0) lines   = tgetnum("li");

        clear_screen         = tgetstr("cl", &area);
        clear_to_eol         = tgetstr("ce", &area);
        clear_to_eos         = tgetstr("cd", &area);
        enter_ca_mode        = tgetstr("ti", &area);
        exit_ca_mode         = tgetstr("te", &area);
        cursor_address       = tgetstr("cm", &area);
        set_scroll_region    = tgetstr("cs", &area);
        delete_char          = tgetstr("dc", &area);
        insert_char          = tgetstr("ic", &area);
        insert_start         = tgetstr("im", &area);
        insert_end           = tgetstr("ei", &area);
        insert_line          = tgetstr("al", &area);
        delete_line          = tgetstr("dl", &area);
        keypad_on            = tgetstr("ks", &area);
        keypad_off           = tgetstr("ke", &area);

        bell		= tgetstr("bl", &area);
        underline	= tgetstr("us", &area);
        reverse		= tgetstr("mr", &area);
        flash		= tgetstr("mb", &area);
        dim		= tgetstr("mh", &area);
        bold		= tgetstr("md", &area);
        standout	= tgetstr("so", &area);
        underline_off	= tgetstr("ue", &area);
        standout_off	= tgetstr("se", &area);
        attr_off	= tgetstr("me", &area);
        attr_on		= tgetstr("sa", &area);

        if (!attr_off) {
            /* can't exit all attrs, but maybe can exit underline/standout */
            reverse = flash = dim = bold = NULL;
            if (!underline_off) underline = NULL;
            if (!standout_off) standout = NULL;
        }

        for (i = 0; i < N_KEYCODES; i++) {
            keycodes[i].code = tgetstr(keycodes[i].capname, &area);
#if 0
            fprintf(stderr, "(%2s) %-12s = %s\n",
                keycodes[i].capname, keycodes[i].name,
                keycodes[i].code ? ascii_to_print(keycodes[i].code)->data : "NULL");
#endif
        }

	if (!keypad_off) keypad_on = NULL;

        if (strcmp(TERM, "xterm") == 0) {
#if 0	    /* Now that tf has virtual screens, the secondary buffer is ok. */
            enter_ca_mode = exit_ca_mode = NULL; /* Avoid secondary buffer. */
#endif
            /* Many old "xterm" termcaps mistakenly omit "cs". */
            if (!set_scroll_region)
                set_scroll_region = "\033[%i%d;%dr";
        }
        if (strcmp(TERM, "linux") == 0) {
            /* Many "linux" termcaps mistakenly omit "ks" and "ke". */
            if (!keypad_on && !keypad_off) {
		keypad_on = "\033[?1h\033=";
		keypad_off = "\033[?1l\033>";
	    }
        }
    }

    if (!bell) bell = "\007";

#ifdef EMXANSI
    VioSetAnsi(1,0);                   /* ensure ansi-mode */
#endif /* EMXANSI */

#endif /* TERMCAP */

#if 1
    /* The insert_start code in iput() is apparently buggy.  Until it is
     * fixed, we ignore the insert_start capability. */
    insert_start = NULL;
#endif
    if (!insert_end) insert_start = NULL;

    if (columns <= 0) columns = DEFAULT_COLUMNS;
    if (lines   <= 0) lines   = DEFAULT_LINES;
    set_var_by_id(VAR_wrapsize, columns - 1);
    ix = 1;
    can_have_visual = (clear_screen || clear_to_eol) && cursor_address;
    can_have_expnonvis = delete_char && cursor_address;
    set_var_by_id(VAR_scroll, has_scroll_region||(insert_line&&delete_line));
    set_var_by_id(VAR_keypad, !!keypad_on);
    have_attr = F_BELL;
    if (underline) have_attr |= F_UNDERLINE;
    if (reverse)   have_attr |= F_REVERSE;
    if (flash)     have_attr |= F_FLASH;
    if (dim)       have_attr |= F_DIM;
    if (bold)      have_attr |= F_BOLD;
    if (standout)  have_attr |= F_BOLD;
}

static void setscroll(int top, int bottom)
{
    if (top_margin == top && bottom_margin == bottom) return; /* optimization */
#ifdef EMXANSI
    bufflush();
#else
    if (!set_scroll_region) return;
    tpgoto(set_scroll_region, (bottom), (top));
    cx = cy = -1;   /* cursor position is undefined */
#endif
    bottom_margin = bottom;
    top_margin = top;
    if (top != 1 || bottom != lines) set_refresh_pending(REF_PHYSICAL);
}

static void xy(int x, int y)
{
    if (x == cx && y == cy) return;                    /* already there */
    if (cy < 0 || cx < 0 || cx > columns) {            /* direct movement */
        tpgoto(cursor_address, x, y);
    } else if (x == 1 && y == cy) {                    /* optimization */
        bufputc('\r');
    } else if (x == 1 && y > cy && y < cy + 5 &&       /* optimization... */
        cy >= top_margin && y <= bottom_margin)        /* if '\n' is safe */
    {
        /* Some broken emulators (including CYGWIN32 b18) lose
         * attributes when \r\n is printed, so we print \n\r instead.
         */
        bufputnc('\n', y - cy);
        if (cx != 1) bufputc('\r');
    } else if (y == cy && x < cx && x > cx - 7) {      /* optimization */
        bufputnc('\010', cx - x);
    } else {                                           /* direct movement */
        tpgoto(cursor_address, x, y);
    }
    cx = x;  cy = y;
}

static void clr(void)
{
    if (clear_screen)
        tp(clear_screen);
    else {
        clear_lines(1, lines);
        xy(1, 1);
    }
    cx = 1;  cy = 1;
}

static void clear_line(void)
{
    if (cx != 1) bufputc('\r');
    cx = 1;
    if (clear_to_eol) {
        tp(clear_to_eol);
    } else {
        bufputnc(' ', Wrap);
        bufputc('\r');
    }
}

#if TERMCAP
/* in case broken lib defines a putchar macro but not a putchar function */
/* Note: WATCOM compiler (QNX) has a function named fputchar. */
static int func_putchar(int c)
{
    return putchar(c);
}
#endif

static void tdirectputs(const char *str)
{
    if (str)
#if TERMCAP
        tputs(str, 1, func_putchar);
#else
        puts(str);
#endif
}

static void tbufputs(const char *str)
{
    if (str)
#if TERMCAP
        tputs(str, 1, fbufputc);
#else
        bufputs(str);
#endif
}

const char *get_keycode(const char *name)
{
    Keycode *keycode;

    keycode = (Keycode *)bsearch(name, keycodes, N_KEYCODES,
        sizeof(Keycode), cstrstructcmp);
    return !keycode ? NULL : keycode->code ? keycode->code : "";
}

/*******************
 * WINDOW HANDLING *
 *******************/

void setup_screen(void)
{
    setscroll(1, lines);
    output_disabled++;

    if (visual && (!can_have_visual)) {
        eprintf("Visual mode is not supported on this terminal.");
        set_var_by_id(VAR_visual, 0);  /* XXX recursion problem?? */
    } else if (visual && lines < 1/*input*/ + 1/*status*/ + 2/*output*/) {
        eprintf("Screen is too small for visual mode.");
        set_var_by_id(VAR_visual, 0);  /* XXX recursion problem?? */
    }
    screen_mode = visual;

    if (!visual) {
        prompt = display_screen->paused ? moreprompt : fgprompt();
	old_ix = -1;

#ifdef SCREEN
    } else {
        prompt = fgprompt();
	if (isize + status_height + 2 > lines) {
	    if ((1 + status_height + 2) <= lines) {
		set_var_by_id(VAR_isize, lines - (status_height + 2));
	    } else {
		set_var_by_id(VAR_isize, 1);
		set_var_by_id(VAR_stat_height, lines - (isize + 2));
	    }
	}
	init_line_numbers();
#if 0
        outcount = out_bot - out_top + 1;
#endif
        if (enter_ca_mode) tp(enter_ca_mode);
    
        if (scroll && !(has_scroll_region || (insert_line && delete_line))) {
            set_var_by_id(VAR_scroll, 0);
        }
        update_status_line(NULL);
        ix = iendx = oy = 1;
        iy = iendy = istarty = in_top;
        ipos();
#endif
    }

    if (keypad && keypad_on)
	tp(keypad_on);

    set_refresh_pending(REF_LOGICAL);
    output_disabled--;
}

#define interesting(pl)  ((pl)->attrs & F_HWRITE || (pl)->charattrs)

/* returns true if str passes filter */
static int screen_filter(Screen *screen, conString *str)
{
    if (str == textdiv_str) return visual;
    return (!screen->selflush || interesting(str))
	&&
	(!screen->filter_enabled ||
	(screen->filter_attr ? interesting(str) :
	patmatch(&screen->filter_pat, str, NULL) == screen->filter_sense));
}

#define purge_old_lines(screen) \
    do { \
	if (screen->nlline > screen->maxlline) \
	    f_purge_old_lines(screen); \
    } while (0) \

static void f_purge_old_lines(Screen *screen)
{
    ListEntry *node;
    PhysLine *pl;

    /* Free old lines if over maximum and they're off the top of the screen.
     * (Freeing them when they fall out of history doesn't work: a long-lived
     * line from a different history could trap lines from the history
     * corresponding to this screen.) */
    node = screen->pline.head;
    while (screen->nlline > screen->maxlline && node != screen->top) {
	node = node->next;
	pl = node->datum;
	if (pl->start == 0) { /* beginning of a new lline */
	    /* free all plines (corresponding to a single lline) above node */
	    screen->nlline--;
	    while (screen->pline.head != node) {
		pl = unlist(screen->pline.head, &screen->pline);
		conStringfree(pl->str);
		pfree(pl, plpool, str);
	    }
	}
    }
}

/* Add a line to the bottom.  bot does not move if nothing below it matches
 * filter. */
static int nextbot(Screen *screen)
{
    PhysLine *pl;
    ListEntry *bot;
    int nback = screen->nback;
    int passed_maxbot;

    if (screen->bot) {
	passed_maxbot = (screen->bot == screen->maxbot);
	bot = screen->bot->next;
    } else {
	bot = screen->pline.head;
	passed_maxbot = 1;
    }
    while (bot) {
	pl = bot->datum;
	nback--;
	/* shouldn't need to recalculate visible, but there's a bug somewhere
	 * in maintaining visible flags in (bot,tail] after /unlimit. */
	if ((pl->visible = screen_filter(screen, pl->str))) {
	    screen->bot = bot;
	    screen->nback_filtered--;
	    screen->nback = nback;
	    if (passed_maxbot) {
		screen->maxbot = bot;
		screen->nnew_filtered--;
		screen->nnew = nback;
	    }
	    if (!screen->top)
		screen->top = screen->bot;
	    screen->viewsize++;
	    if (screen->viewsize >= winlines())
		screen->partialview = 0;
	    return 1;
	}
	if (bot == screen->maxbot)
	    passed_maxbot = 1;
	bot = bot->next;
    }
    return 0;
}

/* Add a line to the top.  top does not move if nothing above it matches
 * filter. */
static int prevtop(Screen *screen)
{
    PhysLine *pl;
    ListEntry *prev;

    /* (top == NULL && bot != NULL) happens after clear_display_screen() */
    if (!screen->bot) return 0;
    prev = screen->top ? screen->top->prev : screen->bot;
    while (prev) {
	pl = prev->datum;
	/* visible flags are not maintained above top, must recalculate */
	if ((pl->visible = screen_filter(screen, pl->str))) {
	    screen->top = prev;
	    screen->viewsize++;
	    if (screen->viewsize >= winlines())
		screen->partialview = 0;
	    return 1;
	}
	/* XXX optimize for pl's that share same ll */
	prev = prev->prev;
    }
    return 0;
}

/* Remove a line from the bottom.  bot always moves. */
static int prevbot(Screen *screen)
{
    ListEntry *node;
    PhysLine *pl;
    int viewsize_changed = 0;

    if (!screen->bot) return 0;
    while (screen->bot->prev) {
	pl = (node = screen->bot)->datum;
	/* visible flags are maintained in [top,bot], we needn't recalculate */
	if (pl->visible) {
	    /* line being knocked off was visible */
	    screen->viewsize--;
	    screen->nback_filtered += !pl->tmp;
	    viewsize_changed = 1;
	}
	screen->bot = screen->bot->prev;
	screen->nback += !pl->tmp;

	if (pl->tmp) {
	    /* line being knocked off was temporary */
	    if (screen->maxbot == node)
		screen->maxbot = screen->maxbot->prev;
	    unlist(node, &screen->pline);
	    conStringfree(pl->str);
	    pfree(pl, plpool, str);
	}

	/* stop if the viewsize has changed and we've found a new visible bot */
	pl = screen->bot->datum;
	if (viewsize_changed && pl->visible)
	    break;
    }
    return viewsize_changed;
}

/* Remove a line from the top.  top always moves. */
static int nexttop(Screen *screen)
{
    PhysLine *pl;
    ListEntry *newtop;
    int viewsize_changed = 0;

    if (!screen->top) return 0;
    while (screen->top->next) {
	pl = screen->top->datum;
	newtop = screen->top->next;
	/* visible flags are maintained in [top,bot], we needn't recalculate */
	if (pl->visible) {
	    /* line being knocked off was visible */
	    screen->viewsize--;
	    viewsize_changed = 1;
	}
	if (pl->tmp) {
	    /* line being knocked off was temporary */
	    unlist(screen->top, &screen->pline);
	    conStringfree(pl->str);
	    pfree(pl, plpool, str);
	}
	screen->top = newtop;

	/* stop if the viewsize has changed and we've found a new visible top */
	pl = screen->top->datum;
	if (viewsize_changed && pl->visible)
	    break;
    }

    purge_old_lines(screen);
    return viewsize_changed;
}

/* recalculate counters and visible flags in (bot, tail] */
static void screen_refilter_bottom(Screen *screen)
{
    PhysLine *pl;
    ListEntry *node;

    if (screen_has_filter(screen)) {
	screen->nback_filtered = 0;
	screen->nnew_filtered = 0;
	node = screen->pline.tail;
	for ( ; node != screen->maxbot; node = node->prev) {
	    pl = node->datum;
	    if ((pl->visible = screen_filter(screen, pl->str)))
		screen->nnew_filtered++;
	}
	screen->nback_filtered = screen->nnew_filtered;
	for ( ; node != screen->bot; node = node->prev) {
	    pl = node->datum;
	    if ((pl->visible = screen_filter(screen, pl->str)))
		screen->nback_filtered++;
	}
    } else {
	screen->nback_filtered = screen->nback;
	screen->nnew_filtered = screen->nnew;
    }
}

static int screen_refilter(Screen *screen)
{
    PhysLine *pl;
    int want;
    Screen oldscreen;
    ListEntry *lowestvisible;

    screen->needs_refilter = 0;
    if (!screen->bot)
	if (!(screen->bot = screen->pline.tail))
	    return 0;
    oldscreen = *screen;
    /* NB: screen->bot should stay in the same place, for when the filter
     * is removed or changed. */
    lowestvisible = screen->bot;
    while (!screen_filter(screen, (pl = lowestvisible->datum)->str)) {
	pl->visible = 0;
	lowestvisible = lowestvisible->prev;
	if (!lowestvisible) {
	    if (screen_has_filter(screen)) {
		*screen = oldscreen; /* restore original state */
		clear_screen_filter(screen);
		screen_refilter(screen);
	    }
	    return 0;
	}
    }
    (pl = lowestvisible->datum)->visible = 1;
    /* recalculate top: start at bot and move top up until view is full */
    screen->viewsize = 1;
    screen->partialview = 0;
    screen->top = lowestvisible;
    want = winlines();
    while ((screen->viewsize < want) && prevtop(screen))
	/* empty loop */;

    screen_refilter_bottom(screen);

    return screen->viewsize;
}

int screen_has_filter(Screen *screen)
{
    return screen->filter_enabled || screen->selflush;
}

void clear_screen_filter(Screen *screen)
{
    screen->filter_enabled = 0;
    screen->selflush = 0;
    screen->needs_refilter = 1;
}

void set_screen_filter(Screen *screen, Pattern *pat, attr_t attr_flag,
    int sense)
{
    if (screen->filter_pat.str)
	free_pattern(&screen->filter_pat);
    if (pat) screen->filter_pat = *pat;
    screen->filter_attr = attr_flag;
    screen->filter_sense = sense;
    screen->filter_enabled = 1;
    screen->selflush = 0;
    screen->needs_refilter = 1;
}

int enable_screen_filter(Screen *screen)
{
    if (!screen->filter_pat.str && !screen->filter_attr)
	return 0;
    screen->filter_enabled = 1;
    screen->selflush = 0;
    screen->needs_refilter = 1;
    return 1;
}

int winlines(void)
{
    return visual ? out_bot - out_top + 1 : lines - 1;
}

/* wraplines
 * Split logical line <ll> into physical lines and append them to <plines>.
 */
static int wraplines(conString *ll, List *plines, int visible)
{
    PhysLine *pl;
    int offset = 0, n = 0;

    do {
	palloc(pl, PhysLine, plpool, str, __FILE__, __LINE__);
	pl->visible = visible;
	pl->tmp = 0;
	(pl->str = ll)->links++;
	pl->start = offset;
	pl->indent = wrapflag && pl->start && wrapspace < Wrap ? wrapspace : 0;
	pl->len = wraplen(ll->data + offset, ll->len - offset, pl->indent);
	offset += pl->len;
	inlist(pl, plines, plines->tail);
	n++;
    } while (offset < ll->len);
    return n;
}

static void rewrap(Screen *screen)
{
    PhysLine *pl;
    ListEntry *node;
    List old_pline;
    int wrapped, old_bot_visible, old_maxbot_visible;

    if (!screen->bot) return;
    hide_screen(screen); /* delete temp lines */

    old_pline.head = screen->pline.head;
    old_pline.tail = screen->pline.tail;
    screen->pline.head = screen->pline.tail = NULL;
    screen->nnew = screen->nnew_filtered = 0;
    screen->nback = screen->nback_filtered = 0;

    pl = screen->bot->datum;
    old_bot_visible = pl->start + pl->len;
    pl = screen->maxbot->datum;
    old_maxbot_visible = pl->start + pl->len;

    /* XXX possible optimization: don't rewrap [head, top) until needed.
     * [top, bot] is needed for display, and (bot, tail] is needed for
     * nback and nnew. */

    /* rewrap llines corresponding to [head, bot] */
    do {
	node = old_pline.head;
	pl = unlist(old_pline.head, &old_pline);
	if (pl->start == 0) {
	    wrapped = wraplines(pl->str, &screen->pline, pl->visible);
	    screen->npline += wrapped;
	}
	conStringfree(pl->str);
	pfree(pl, plpool, str);
    } while (node != screen->bot);

    /* recalculate bot within last lline */
    screen->bot = screen->pline.tail;
    while (screen->bot->prev) {
	pl = screen->bot->datum;
	if (pl->start == 0 || pl->start < old_bot_visible) break;
	screen->bot = screen->bot->prev;
	screen->nback++;
    }

    /* rewrap llines corresponding to (bot, maxbot] */
    while (node != screen->maxbot) {
	node = old_pline.head;
	pl = unlist(old_pline.head, &old_pline);
	if (pl->start == 0) {
	    wrapped = wraplines(pl->str, &screen->pline, pl->visible);
	    screen->npline += wrapped;
	    screen->nback += wrapped;
	}
	conStringfree(pl->str);
	pfree(pl, plpool, str);
    }

    /* recalculate maxbot within last lline */
    screen->maxbot = screen->pline.tail;
    while (screen->maxbot->prev) {
	pl = screen->maxbot->datum;
	if (pl->start == 0 || pl->start < old_maxbot_visible) break;
	screen->maxbot = screen->maxbot->prev;
	screen->nnew++;
    }

    /* rewrap llines corresponding to (maxbot, tail] */
    while (old_pline.head) {
	pl = unlist(old_pline.head, &old_pline);
	if (pl->start == 0) {
	    wrapped = wraplines(pl->str, &screen->pline, pl->visible);
	    screen->npline += wrapped;
	    screen->nback += wrapped;
	    screen->nnew += wrapped;
	}
	conStringfree(pl->str);
	pfree(pl, plpool, str);
    }

    /* recalculate nback_filtered, nnew_filtered, viewsize, top */
    /* XXX TODO: should honor screen->partialview */
    screen_refilter(screen);

    screen->scr_wrapflag = wrapflag;
    screen->scr_wrapsize = Wrap;
    screen->scr_wrapspace = wrapspace;
    screen->scr_wrappunct = wrappunct;
}

int redraw_window(Screen *screen, int already_clear)
{
    if (screen->needs_refilter && !screen_refilter(screen))
	return 0;

    if (!already_clear)
	clear_lines(1, visual ? out_bot : lines);

    if (screen->scr_wrapflag != wrapflag ||
	screen->scr_wrapsize != Wrap ||
	screen->scr_wrapspace != wrapspace ||
	screen->scr_wrappunct != wrappunct)
    {
	rewrap(screen);
    } else {
	/* if terminal height decreased or textdiv was inserted */
	while (screen->viewsize > winlines())
	    nexttop(screen);
	/* if terminal height increased */
	if (!screen->partialview) {
	    while (screen->viewsize < winlines())
		if (!prevtop(screen)) break;
	}
    }

    if (!visual) xy(1, lines);

    /* viewsize may be 0 after clear_display_screen(), even if bot != NULL
     * or nplines > 0 */
    if (screen->viewsize) {
        PhysLine *pl;
	ListEntry *node;
	int first;

        if (visual) xy(1, out_bot - (screen->viewsize - 1));

	node = screen->top;
	first = 1;
	while (1) {
	    pl = node->datum;
	    if (pl->visible) {
		if (!first) crnl(1);
		first = 0;
		hwrite(pl->str, pl->start,
		    pl->len < Wrap - pl->indent ? pl->len : Wrap - pl->indent,
		    pl->indent);
	    }
	    if (node == screen->bot)
		break;
	    node = node->next;
        }
	if (visual) {
	    cx = 1; cy = out_bot;
	} else {
	    crnl(1);
	    cx = 1; cy = lines;
	}
    }

    bufflush();
    old_ix = -1;
    set_refresh_pending(REF_PHYSICAL);
    ox = cx;
    oy = cy;

    return 1;
}

int redraw(void)
{
    alert_timeout = tvzero;
    alert_pos = 0;
    alert_len = 0;
    if (visual) {
	top_margin = bottom_margin = -1;  /* force scroll region reset */
        clr();
    } else {
        bufputnc('\n', lines);     /* scroll region impossible */
    }
    setup_screen();

    if (virtscreen) {
	hide_screen(display_screen);
	unhide_screen(display_screen);
    }
    return redraw_window(display_screen, 1);
}

static void clear_screen_view(Screen *screen)
{
    if (screen->bot) {
	screen->top = screen->bot->next;
	screen->viewsize = 0;
	screen->partialview = 1;
	reset_outcount(screen);
    }
}

int clear_display_screen(void)
{
    if (!display_screen->bot) return 0;
    clear_screen_view(display_screen);
    return redraw_window(display_screen, 0);
}

static const char *parse_statusfield(StatusField *field, const char *s)
{
    const char *t;

    memset(field, 0, sizeof(*field));
    field->internal = -1;

    if (is_quote(*s)) {                                /* string literal */
	STATIC_BUFFER(buffer);
	if (!stringliteral(buffer, &s)) {
	    eprintf("%S", buffer);
	    return NULL;
	}
	field->name = STRDUP(buffer->data);
    } else if (*s == '@') {                            /* internal */
	for (t = ++s; is_alnum(*s) || *s == '_'; s++);
	field->name = strncpy(XMALLOC(s - t + 1), t, s - t);
	field->name[s-t] = '\0';
	field->internal = enum2int(field->name, 0, enum_status,
	    "status_fields internal status");
	FREE(field->name);
	field->name = NULL;
	if (field->internal < 0)
	    return NULL;
    } else if (is_alnum(*s) || *s == '_') {            /* variable */
	for (t = s++; is_alnum(*s) || *s == '_'; s++);
	field->name = strncpy(XMALLOC(s - t + 1), t, s - t);
	field->name[s-t] = '\0';
	field->var = &bogusvar;
    } else {                                           /* blank */
	field->name = NULL;
    }

    if (*s == ':') {
	while (*++s == '-')
	    field->rightjust = !field->rightjust;
	field->width = strtoint(s, &s);
    }

    if (*s == ':') {
	for (t = s + 1; *s && !is_space(*s); s++);
	if (!parse_attrs(t, &field->attrs, ' '))
	    return NULL;
    }

    if (*s && !is_space(*s)) {
	eprintf("garbage in status field: %.8s", s);
	return NULL;
    }
    return s;
}

static Var *status_var(const char *type, const char *suffix)
{
    STATIC_BUFFER(name);
    Stringcat(Stringcat(Stringcpy(name, "status_"), type), suffix);
    return findorcreateglobalvar(name->data);
}

conString *status_field_text(int row)
{
    ListEntry *node;
    StatusField *f;
    String *buf = Stringnew(NULL, columns, 0);
    int width;

    if (row < 0 || row >= max_status_height)
	return CS(buf);
    for (node = statusfield_list[row]->head; node; node = node->next) {
	if (node != statusfield_list[row]->head)
	    Stringadd(buf, ' ');
	f = (StatusField*)node->datum;
	width = f->width;
	if (f->internal >= 0) {
	    Stringadd(buf, '@');
	    SStringcat(buf, &enum_status[f->internal]);
	} else if (f->var) {
	    Stringcat(buf, f->var->val.name);
	} else if (f->name) {
	    Sappendf(buf, "\"%q\"", '"', f->name);
	    if (width == strlen(f->name)) width = 0;
	}
	if (!width && !f->attrs && !f->rightjust)
	    continue;
	Stringadd(buf, ':');
	if (width || f->rightjust)
	    Sappendf(buf, "%s%d", f->rightjust ? "-" : "", width);
	if (!f->attrs)
	    continue;
	Stringadd(buf, ':');
	attr2str(buf, f->attrs);
    }
    return CS(buf);
}

static void regen_status_fields(int row)
{
    ListEntry *node;
    StatusField *f;
    int column;

    /* finish calculations on status_fields_list */
    for (node = statusfield_list[row]->head; node; node = node->next) {
        f = (StatusField*)node->datum;
        if (f->var) { /* f->var == &bogusvar */
	    if (f->var == &bogusvar) {
		f->var = findorcreateglobalvar(f->name);
		FREE(f->name);
		f->name = NULL;
	    }
            f->var->statuses++;
	    f->fmtvar = status_var("var_", f->var->val.name);
	    f->attrvar = status_var("attr_var_", f->var->val.name);
	} else if (f->internal >= 0) {
	    f->fmtvar = status_var("int_", enum_status[f->internal].data);
	    f->attrvar = status_var("attr_int_", enum_status[f->internal].data);
	} else {
	    continue;
	}
	f->fmtvar->statusfmts++;
	f->attrvar->statusattrs++;
	if (ch_attr(f->attrvar))
	    f->vattrs = f->attrvar->val.u.attr;
    }

    clock_update.tv_sec = 0;
    variable_width_field[row] = NULL;
    status_left[row] = status_right[row] = 0;
    column = 0;
    for (node = statusfield_list[row]->head; node; node = node->next) {
        f = (StatusField*)node->datum;
        status_left[row] = f->column = column;
        if (f->width == 0) {
	    variable_width_field[row] = f;
	    break;
	}
        column += f->width;
    }
    column = 0;
    if (node) {
        for (node = statusfield_list[row]->tail; node; node = node->prev) {
            f = (StatusField*)node->datum;
            if (f->width == 0) break;
            status_right[row] = -(f->column = (column -= f->width));
        }
    }

    if (row == 0) {
	/* regenerate the value of %status_fields for backward compatibility */
	conString *buf = status_field_text(row);
	/* set_str_var_direct avoids error checking and recursion */
	set_str_var_direct(&special_var[VAR_stat_fields], TYPE_STR, buf);
    }

    update_status_line(NULL);
}

static ListEntry *find_statusfield_by_template(StatusField *target,
    int full_comparison, const char *label)
{
    ListEntry *node;
    StatusField *f;
    int row;

    for (row = 0; row < status_height; row++) {
	if (target->row >= 0 && row != target->row) continue;
	for (node = statusfield_list[row]->head; node; node = node->next) {
	    f = (StatusField*)node->datum;
	    if (target->internal >= 0) {
		if (target->internal != f->internal) continue;
	    } else if (target->var) {
		if (!f->var || strcmp(target->name, f->var->val.name) != 0)
		    continue;
	    } else if (target->name) {
		if (!f->name || cstrcmp(target->name, f->name) != 0)
		    continue;
	    }
	    if (full_comparison) {
		if (target->width && target->width != f->width) continue;
		if (target->attrs && target->attrs != f->attrs) continue;
	    }
	    return node;
	}
    }
    if (label) {
	eprintf(
	    target->row<0 ? "%s: no such field" : "%s: no such field in row %d",
	    label, target->row);
    }
    return NULL;
}

static ListEntry *find_statusfield_by_name(int row, const char *spec)
{
    ListEntry *result = NULL;
    StatusField target;
    if (parse_statusfield(&target, spec)) {
	target.row = row;
	result = find_statusfield_by_template(&target, TRUE, spec);
    }
    if (target.name) FREE(target.name);
    return result;
}

int ch_status_int(Var *var)
{
    if (warn_status)
	wprintf("the default value of %s has "
	    "changed between tf version 4 and 5.", var->val.name);
    return 1;
}

static void free_statusfield(StatusField *field)
{
    if (field->var && field->var != &bogusvar && !--field->var->statuses)
	freevar(field->var);
    if (field->fmtvar && !--field->fmtvar->statuses)
	freevar(field->fmtvar);
    if (field->attrvar && !--field->attrvar->statuses)
	freevar(field->attrvar);
    if (field->name)
	FREE(field->name);
    FREE(field);
}

static int status_add(int reset, int nodup, int row, ListEntry *where,
    const char *s,
    long spacer) /* spacer>0 goes after new field, spacer<0 goes before */
{
    int totwidth = 0, vwf_found = 0;
    List newlist[1];
    StatusField *field;

    init_list(newlist);

    if (!reset) {
	vwf_found = !!variable_width_field[row];
	totwidth = status_left[row] + status_right[row];
    }

    /* validate and insert new fields */
    while (1) {
        while(is_space(*s)) s++;
        if (!*s) break;
        field = XMALLOC(sizeof(*field));
	if (!(s = parse_statusfield(field, s)))
            goto status_add_error;
	field->row = row;

        if (!field->width && !field->var && field->internal < 0 && field->name)
            field->width = strlen(field->name);

	if (nodup && find_statusfield_by_template(field, FALSE, NULL)) {
	    free_statusfield(field);
	    continue;
	}

        if (field->width != 0) {
            totwidth += field->width;
	} else {
	    if (vwf_found) {
		eprintf("Only one variable width status field is allowed.");
		goto status_add_error;
	    }
	    vwf_found = 1;
	}

	inlist(field, newlist, newlist->tail);
    }

    if (spacer && newlist->head && !reset && statusfield_list[row]->head) {
        field = XCALLOC(sizeof(*field));
        field->internal = -1;
	field->row = row;
	totwidth += (field->width = spacer < 0 ? -spacer : spacer);
	inlist(field, newlist, spacer >= 0 ? NULL : newlist->tail);
    }

    if (totwidth > columns) {
        wprintf("total status width (%d) is wider than screen (%d)",
	    totwidth, columns);
    }

    if (reset) {
	/* delete old fields and clean up referents */
	while (statusfield_list[row]->head)
	    free_statusfield(unlist(statusfield_list[row]->head,
		statusfield_list[row]));
	*statusfield_list[row] = *newlist;
    } else {
	/* insert new fields into list */
	/* (We could be faster with ptr swapping, but this isn't critical) */
	while (newlist->head) {
	    field = (StatusField*)unlist(newlist->head, newlist);
	    where = inlist(field, statusfield_list[row], where);
	}
    }

    regen_status_fields(row);
    return 1;

status_add_error:
    free_statusfield(field);
    while (newlist->head)
	free_statusfield(unlist(newlist->head, newlist));
    return 0;
}

struct Value *handle_status_add_command(String *args, int offset)
{
    int opt, nodup = FALSE, row = -1, reset = 0;
    ValueUnion uval;
    long spacer = 1;
    const char *ptr, *before = NULL, *after = NULL;
    ListEntry *where;

    startopt(CS(args), "A:B:s#r#xc");
    while ((opt = nextopt(&ptr, &uval, NULL, &offset))) {
	switch (opt) {
	case 'A':  after = STRDUP(ptr);   before = NULL;  break;
	case 'B':  before = STRDUP(ptr);  after = NULL;   break;
	case 's':  spacer = uval.ival;  break;
	case 'r':  row = uval.ival;     break;
	case 'x':  nodup = TRUE;  break;
	case 'c':  reset = 1;     break;
        default:   return shareval(val_zero);
	}
    }

    if (after) {
	if (*after) {
	    where = find_statusfield_by_name(row, after);
	    if (!where) return shareval(val_zero);
	    row = ((StatusField*)(where->datum))->row;
	} else {
	    if (row < 0) row = 0;
	    where = statusfield_list[row]->tail;
	}
	FREE(after);
    } else if (before) {
	spacer = -spacer;
	if (*before) {
	    where = find_statusfield_by_name(row, before);
	    if (!where) return shareval(val_zero);
	    row = ((StatusField*)(where->datum))->row;
	    where = where->prev;
	} else {
	    if (row < 0) row = 0;
	    where = NULL;
	}
	FREE(before);
    } else {
	if (row < 0) row = 0;
	where = statusfield_list[row]->tail;
    }

    return shareval(
	status_add(reset, nodup, row, where, args->data + offset, spacer) ?
	val_one : val_zero);
}

int ch_status_fields(Var *var)
{
    if (warn_status) {
	wprintf("setting status_fields directly is deprecated, "
	    "and may clobber useful new features introduced in version 5.  "
	    "The recommended way to change "
	    "status fields is with /status_add, /status_rm, or /status_edit. "
	    "(This warning can be disabled with \"/set warn_status=off\".)");
    }
    return status_add(TRUE, FALSE, 0, NULL,
	status_fields ? status_fields : "", 0);
}

static int getspacer(ListEntry *node)
{
    StatusField *f;
    if (!node) return 0;
    f = node->datum;
    return (f->var || f->internal >= 0 || f->name) ? 0 : f->width;
}

struct Value *handle_status_rm_command(String *args, int offset)
{
    ValueUnion uval;
    long row = -1;
    int opt;
    ListEntry *node;

    startopt(CS(args), "r#");
    while ((opt = nextopt(NULL, &uval, NULL, &offset))) {
	switch (opt) {
	case 'r':  row = uval.ival; break;
        default:   return shareval(val_zero);
	}
    }

    node = find_statusfield_by_name(row, args->data + offset);
    if (!node) return shareval(val_zero);
    row = ((StatusField*)(node->datum))->row;
    if (variable_width_field[row] == node->datum)
	variable_width_field[row] = NULL;
    if (!node->prev && getspacer(node->next)) {
	unlist(node->next, statusfield_list[row]); /* remove leading spacer */
    } else if (!node->next && getspacer(node->prev)) {
	unlist(node->prev, statusfield_list[row]); /* remove trailing spacer */
    } else if (getspacer(node->prev) && getspacer(node->next)) {
	/* remove smaller of two neighboring spacers */
	if (getspacer(node->prev) < getspacer(node->next))
	    unlist(node->prev, statusfield_list[row]);
	else
	    unlist(node->next, statusfield_list[row]);
    }
    unlist(node, statusfield_list[row]);
    regen_status_fields(row);
    return shareval(val_one);
}

struct Value *handle_status_edit_command(String *args, int offset)
{
    ValueUnion uval;
    long row = -1;
    int opt;
    StatusField *field;
    ListEntry *node;

    startopt(CS(args), "r#");
    while ((opt = nextopt(NULL, &uval, NULL, &offset))) {
	switch (opt) {
	case 'r':  row = uval.ival; break;
        default:   return shareval(val_zero);
	}
    }

    field = XMALLOC(sizeof(*field));
    if (!parse_statusfield(field, args->data + offset)) {
	free_statusfield(field);
	return shareval(val_zero);
    }
    field->row = row;
    node = find_statusfield_by_template(field, FALSE, args->data + offset);
    if (!node) {
	free_statusfield(field);
	return shareval(val_zero);
    }
    row = field->row = ((StatusField*)(node->datum))->row;
    if (!field->width && variable_width_field[row] &&
	node->datum != variable_width_field[row])
    {
	eprintf("Only one variable width status field per row is allowed.");
	free_statusfield(field);
	return shareval(val_zero);
    }
    free_statusfield(node->datum);
    node->datum = field;
    regen_status_fields(field->row);
    return shareval(val_one);
}

/* returns column of field, or -1 if field is obscured by alert */
static inline int statusfield_column(StatusField *field)
{
    int column;
    if (field->column >= 0)
	return (field->column > columns) ? columns : field->column;
    column = field->column +
	((status_left[field->row] + status_right[field->row] > columns) ?
	status_left[field->row] + status_right[field->row] : columns);
    return (column > columns) ? columns : column;
}

static int status_width(StatusField *field, int start)
{
    int width;
    if (start >= columns) return 0;
    width = (field->width == 0) ?
	columns - status_right[field->row] - status_left[field->row] :
	(field->width > 0) ? field->width : -field->width;
    if (width > columns - start)
        width = columns - start;
    if (width < 0)
	width = 0;
    return width;
}

int handle_status_width_func(const char *name)
{
    /* XXX need to be able to specify row */
    StatusField *field;
    field = (StatusField*)find_statusfield_by_name(-1, name)->datum;
    return field ? status_width(field, statusfield_column(field)) : 0;
}

static int format_statusfield(StatusField *field)
{
    STATIC_BUFFER(scratch);
    const char *old_command;
    Value *fmtval, *val = NULL;
    Program *prog;
    int width, x, i;

    output_disabled++;
    Stringtrunc(scratch, 0);
    if (field->internal >= 0 || field->var) {
        fmtval = getvarval(field->fmtvar);
        old_command = current_command;
        if (fmtval) {
            current_command = fmtval->name;
	    if (fmtval->type & TYPE_EXPR) {
		prog = fmtval->u.prog;
	    } else if (fmtval->type == TYPE_STR) {
		prog = compile_tf(valstr(fmtval), 0, -1, 1, 2);
		if ((fmtval->u.prog = prog))
		    fmtval->type |= TYPE_EXPR;
	    } else {
		prog = compile_tf(valstr(fmtval), 0, -1, 1, 0);
	    }
	    if (prog) {
		val = expr_value_safe(prog);
		if (!(fmtval->type & TYPE_EXPR))
		    prog_free(prog);
	    }
	    if (val) {
		SStringcpy(scratch, valstr(val));
		freeval(val);
	    } else {
		SStringcpy(scratch, blankline);
	    }
        } else if (field->var) {
            current_command = field->var->val.name;
            val = getvarval(field->var);
            SStringcpy(scratch, val ? valstr(val) : blankline);
        }
        current_command = old_command;
    } else if (field->name) {   /* string literal */
        Stringcpy(scratch, field->name);
    }

    x = statusfield_column(field);
    width = status_width(field, x);
    if (scratch->len > width)
        Stringtrunc(scratch, width);

    if (field->rightjust && scratch->len < width) {          /* left pad */
	for (i = 0; i < width - scratch->len; i++, x++) {
	    status_line[field->row]->data[x] = true_status_pad;
	    status_line[field->row]->charattrs[x] = status_attr;
	}
    }

    if (scratch->len) {                                      /* value */
        attr_t attrs = scratch->attrs;
        attrs = adj_attr(attrs, status_attr);
        attrs = adj_attr(attrs, field->attrs);
        attrs = adj_attr(attrs, field->vattrs);
	for (i = 0; i < scratch->len; i++, x++) {
	    status_line[field->row]->data[x] = scratch->data[i];
	    status_line[field->row]->charattrs[x] = scratch->charattrs ?
		adj_attr(attrs, scratch->charattrs[i]) : attrs;
	}
    }

    if (!field->rightjust && scratch->len < width) {         /* right pad */
	for (i = 0; i < width - scratch->len; i++, x++) {
	    status_line[field->row]->data[x] = true_status_pad;
	    status_line[field->row]->charattrs[x] = status_attr;
	}
    }

    if (field->internal == STAT_MORE)
        need_more_refresh = 0;
    if (field->internal == STAT_CLOCK) {
        struct tm *local;
	time_t sec = time(NULL);
        /* note: localtime()->tm_sec won't compile if prototype is missing */
        local = localtime(&sec);
        clock_update.tv_sec = sec + 60 - local->tm_sec;
    }

#if 0
    if (field->var && !field->var->status)   /* var was unset */
        field->var = NULL;
#endif
    output_disabled--;
    return width;
}

static void display_status_segment(int row, int start, int width)
{
    if (!alert_len || row != alert_row ||
	start + width <= alert_pos || start >= alert_pos + alert_len)
    {
	/* no overlap with alert */
	xy(start + 1, stat_top + row);
	hwrite(CS(status_line[row]), start, width, 0);
    } else {
	if (start < alert_pos) {
	    /* segment starts left of alert */
	    xy(start + 1, stat_top + row);
	    hwrite(CS(status_line[row]), start, alert_pos - start, 0);
	}
	if (start + width >= alert_pos) {
	    /* segment ends right of alert */
	    xy(alert_pos + alert_len + 1, stat_top + row);
	    hwrite(CS(status_line[row]), alert_pos + alert_len,
		start + width - (alert_pos + alert_len), 0);
	}
    }
}

void update_status_field(Var *var, stat_id_t internal)
{
    ListEntry *node;
    StatusField *field;
    int row, column, width;
    int count = 0;

    if (screen_mode < 1) return;

    if (var && var->statusattrs) {
	if (!ch_attr(var))
	    return; /* error */
    }

    for (row = 0; row < status_height; row++) {
	for (node = statusfield_list[row]->head; node; node = node->next) {
	    field = (StatusField*)node->datum;
	    if (var) {
		if (field->var == var)
		    /* do nothing */;
		else if (field->fmtvar == var)
		    /* do nothing */;
		else if (field->attrvar == var) 
		    field->vattrs = var->val.u.attr;
		else
		    continue;
	    }
	    if (internal >= 0 && field->internal != internal) continue;
	    column = statusfield_column(field);
	    if (column >= columns) /* doesn't fit, nor will any later fields */
		break;
	    count++;
	    width = format_statusfield(field);
	    display_status_segment(row, column, width);
	}
    }

    if (count) {
	bufflush();
	set_refresh_pending(REF_PHYSICAL);
    }
}

void format_status_line(void)
{
    ListEntry *node;
    StatusField *field;
    int row, column, width;

    for (row = 0; row < status_height; row++) {
	column = 0;
	width = 0;
	for (node = statusfield_list[row]->head; node; node = node->next) {
	    field = (StatusField*)node->datum;

	    if ((column = statusfield_column(field)) >= columns)
		break;
	    width = format_statusfield(field);
	}

	for (column += width; column < columns; column++) {
	    status_line[row]->data[column] = true_status_pad;
	    status_line[row]->charattrs[column] = status_attr;
	}
    }
}

int display_status_line(void)
{
    int row;

    if (screen_mode < 1) return 0;

    for (row = 0; row < status_height; row++) {
	display_status_segment(row, 0, columns);
    }

    bufflush();
    set_refresh_pending(REF_PHYSICAL);
    return 1;
}

int update_status_line(Var *var)
{
    /* XXX optimization:  some code that calls update_status_line() without
     * any change in status_line could call display_status_line() directly,
     * avoiding reformatting (in particular, status_{int,var}_* execution). */
    format_status_line();
    display_status_line();
    return 1; /* for variable func */
}

void alert(conString *msg)
{
    int row = 0;
    int new_pos, new_len;
    ListEntry *node;
    StatusField *field;
    attr_t orig_attrs;

    if (msg->attrs & F_GAG && gag)
	return;
    alert_id++;
    msg->links++; /* some callers pass msg with links==0, so we ++ and free */
    if (!visual) {
	tfputline(msg, tferr);
    } else {
	/* default to position 0 */
	new_pos = 0;
	new_len = msg->len > Wrap ? Wrap : msg->len;
	if (msg->len < Wrap) {
	    /* if there's a field after @world, and msg fits there, use it */
	    for (node = statusfield_list[row]->head; node; node = node->next) {
		field = (StatusField*)node->datum;
		if (field->internal == STAT_WORLD && node->next) {
		    field = (StatusField*)node->next->datum;
		    break;
		}
	    }
	    if (node) {
		new_pos = (field->column < 0 ? columns : 0) + field->column;
		if (new_pos + new_len > Wrap)
		    new_pos = 0;
	    }
	}

	if (alert_len &&
	    (alert_pos < new_pos || alert_pos + alert_len > new_pos + new_len))
	{
	    /* part of old alert would still be visible under new alert */
	    /* XXX this could be optimized */
	    clear_alert();
	}

	alert_len = new_len;
	alert_pos = new_pos;

	gettime(&alert_timeout);
	tvadd(&alert_timeout, &alert_timeout, &alert_time);

	xy(alert_pos + 1, stat_top + alert_row);
	orig_attrs = msg->attrs;
	msg->attrs = adj_attr(msg->attrs, alert_attr);
	hwrite(msg, 0, alert_len, 0);
	msg->attrs = orig_attrs;

	bufflush();
	set_refresh_pending(REF_PHYSICAL);
    }
    conStringfree(msg);
}

void clear_alert(void)
{
    if (!alert_len) return;
    xy(alert_pos + 1, stat_top + alert_row);
    hwrite(CS(status_line[alert_row]), alert_pos, alert_len, 0);
    bufflush();
    set_refresh_pending(REF_PHYSICAL);
    alert_timeout = tvzero;
    alert_pos = 0;
    alert_len = 0;
    alert_id = 0;
}

/* used by %{visual}, %{isize}, SIGWINCH */
int ch_visual(Var *var)
{
    int need_redraw = 0, resized = 0, row;

    for (row = 0; row < status_height; row++) {
	if (status_line[row]->len < columns)
	    Stringnadd(status_line[row], '?', columns - status_line[row]->len);
	Stringtrunc(status_line[row], columns);
    }

    if (screen_mode < 0) {                /* e.g., called by init_variables() */
        return 1;
    } else if (var == &special_var[VAR_visual]) {      /* %visual changed */
        need_redraw = resized = 1;
	alert_timeout = tvzero;
	alert_pos = 0;
	alert_len = 0;
    } else if (var == &special_var[VAR_isize]) {      /* %isize changed */
        need_redraw = resized = visual;
#ifdef SCREEN
    } else {                              /* SIGWINCH */
        need_redraw = 1;
	resized = 1;
        cx = cy = -1;  /* unknown */
        top_margin = bottom_margin = -1;  /* unknown */
#endif
    }

    if (need_redraw)
        redraw();
    if (resized)
        transmit_window_size();
    return 1;
}

int ch_status_height(Var *var)
{
    if (status_height > max_status_height) {
	eprintf("%s must be <= %d", var->val.name, max_status_height);
	return 0;
    }
    if (visual) redraw();
    transmit_window_size();
    return 1;
}

int ch_expnonvis(Var *var)
{
    if (!can_have_expnonvis && expnonvis) {
        eprintf("expnonvis mode is not supported on this terminal.");
	return 0;
    }
    if (!visual) {
	old_ix = -1;
	redraw();
    }
    return 1;
}

/* used by %{wrap}, %{wrappunct}, %{wrapsize}, %{wrapspace} */
int ch_wrap(Var *var)
{
    if (screen_mode < 0)	/* e.g., called by init_variables() */
	return 1;

    redraw_window(display_screen, 0);
    transmit_window_size();
    return 1;
}

void fix_screen(void)
{
    oflush();
    if (keypad && keypad_off)
	tp(keypad_off);
    if (screen_mode <= 0) {
        clear_line();
#ifdef SCREEN
    } else {
	top_margin = bottom_margin = -1;  /* force scroll region reset */
        setscroll(1, lines);
        clear_lines(stat_top, lines);
        xy(1, stat_top);
        if (exit_ca_mode) tp(exit_ca_mode);
#endif
    }
    cx = cy = -1;
    bufflush();
    screen_mode = -1;
}

/* minimal_fix_screen() avoids use of possibly corrupted structures. */
void minimal_fix_screen(void)
{
    tp = tdirectputs;
    fg_screen = default_screen;
    default_screen->pline.head = default_screen->pline.tail = NULL;
    default_screen->top = default_screen->bot = NULL;
    outbuf->data = NULL;
    outbuf->len = 0;
    outbuf->size = 0;
    output_disabled++;
    fix_screen();
    fflush(stdout);
}

static void clear_lines(int start, int end)
{
    if (start > end) return;
    xy(1, start);
    if (end >= lines && clear_to_eos) {
        tp(clear_to_eos);  /* cx,cy were set by xy() */
    } else {
        clear_line();
        while (start++ < end) {
             bufputc('\n');
             clear_line();
        }
        cy = end;
    }
}

/* clear entire input window */
static void clear_input_window(void)
{
    /* only called in visual mode */
    clear_lines(in_top, lines);
    ix = iendx = 1;
    iy = iendy = istarty = in_top;
    ipos();
}

/* clear logical input line */
static void clear_input_line(void)
{
    if (!visual) clear_line();
    else clear_lines(istarty, iendy);
    ix = iendx = 1;
    iy = iendy = istarty;
    if (visual) ipos();
}

/* affects iendx, iendy, istarty.  No effect on ix, iy. */
static void scroll_input(int n)
{
    if (n > isize) {
        clear_lines(in_top, lines);
        iendy = in_top;
    } else if (delete_line) {
        xy(1, in_top);
        for (iendy = lines + 1; iendy > lines - n + 1; iendy--)
            tp(delete_line);
    } else if (has_scroll_region) {
        setscroll(in_top, lines);
        xy(1, lines);
        crnl(n);  /* DON'T: cy += n; */
        iendy = lines - n + 1;
    }
    xy(iendx = 1, iendy);
}


/***********************************************************************
 *                                                                     *
 *                        INPUT WINDOW HANDLING                        *
 *                                                                     *
 ***********************************************************************/

/* ictrl_put
 * display n characters of s, with control characters converted to printable
 * bold reverse (so the physical width is also n).
 */
static void ictrl_put(const char *s, int n)
{
    int attrflag;
    char c;

    for (attrflag = 0; n > 0; s++, n--) {
        c = unmapchar(localize(*s));
        if (is_cntrl(c)) {
            if (!attrflag)
                attributes_on(F_BOLD | F_REVERSE), attrflag = 1;
            bufputc(CTRL(c));
        } else {
            if (attrflag)
                attributes_off(F_BOLD | F_REVERSE), attrflag = 0;
            bufputc(c);
        }
    }
    if (attrflag) attributes_off(F_BOLD | F_REVERSE);
}

/* ioutputs
 * Print string within bounds of input window.  len is the number of
 * characters to print; return value is the number actually printed,
 * which may be less than len if string doesn't fit in the input window.
 * precondition: iendx,iendy and real cursor are at the output position.
 */
static int ioutputs(const char *str, int len)
{
    int space, written;

    for (written = 0; len > 0; len -= space) {
        if ((space = Wrap - iendx + 1) <= 0) {
            if (!visual || iendy == lines) break;   /* at end of window */
            if (visual) xy(iendx = 1, ++iendy);
            space = Wrap;
        }
        if (space > len) space = len;
        ictrl_put(str, space);  cx += space;
        str += space;
        written += space;
        iendx += space;
    }
    return written;
}

/* ioutall
 * Performs ioutputs() for input buffer starting at kpos.
 * A negative kpos means to display that much of the end of the prompt.
 */
static void ioutall(int kpos)
{
    int ppos;

    if (kpos < 0) {                  /* posible only if there's a prompt */
        kpos = -(-kpos % Wrap);
        ppos = prompt->len + kpos;
        if (ppos < 0) ppos = 0;
        hwrite(prompt, ppos, prompt->len - ppos, 0);
        iendx = -kpos + 1;
        kpos = 0;
    }
    if (sockecho())
        ioutputs(keybuf->data + kpos, keybuf->len - kpos);
}

void iput(int len)
{
    const char *s;
    int count, scrolled = 0, oiex = iendx, oiey = iendy;

    s = keybuf->data + keyboard_pos - len;

    if (!sockecho()) return;
    if (visual) physical_refresh();

    if (keybuf->len - keyboard_pos > 8 &&     /* faster than redisplay? */
        visual && insert && clear_to_eol &&
        (insert_char || insert_start) &&      /* can insert */
        cy + (cx + len) / Wrap <= lines &&    /* new text will fit in window */
        Wrap - len > 8)                       /* faster than redisplay? */
    {
        /* fast insert */
        iy = iy + (ix - 1 + len) / Wrap;
        ix = (ix - 1 + len) % Wrap + 1;
        iendy = iendy + (iendx - 1 + len) / Wrap;
        iendx = (iendx - 1 + len) % Wrap + 1;
        if (iendy > lines) { iendy = lines; iendx = Wrap + 1; }
        if (cx + len <= Wrap) {
            if (insert_start) tp(insert_start);
            else for (count = len; count; count--) tp(insert_char);
            ictrl_put(s, len);
            s += Wrap - (cx - 1);
            cx += len;
        } else {
            ictrl_put(s, Wrap - (cx - 1));
            s += Wrap - (cx - 1);
            cx = Wrap + 1;
            if (insert_start) tp(insert_start);
        }
        while (s < keybuf->data + keybuf->len) {
            if (Wrap < columns && cx <= Wrap) {
                xy(Wrap + 1, cy);
                tp(clear_to_eol);
            }
            if (cy == lines) break;
            xy(1, cy + 1);
            if (!insert_start)
                for (count = len; count; count--) tp(insert_char);
            ictrl_put(s, len);  cx += len;
            s += Wrap;
        }
        if (insert_start) tp(insert_end);
        ipos();
        bufflush();
        return;
    }

    iendx = ix;
    iendy = iy;

    /* Display as much as possible.  If it doesn't fit, scroll and repeat
     * until the whole string has been displayed.
     */
    while (count = ioutputs(s, len), s += count, (len -= count) > 0) {
        scrolled++;
        if (!visual) {
	    if (expnonvis) {
		int i;
		int prompt_len = 0;
		bufputc('\r');
		if (prompt) {
		    prompt_len = prompt->len % Wrap;
		    hwrite(prompt, prompt->len - prompt_len, prompt_len, 0);
		}
		for (i = 0; i < sidescroll; i++)
		    tp(delete_char);
		iendx -= i;
		cx = cy = -1;
		xy(iendx, lines);
	    } else {
		crnl(1);  cx = 1;
		iendx = ix = 1;
	    }
        } else if (scroll && !clearfull) {
            scroll_input(1);
            if (istarty > in_top) istarty--;
        } else {
            clear_input_window();
        }
    }
    ix = iendx;
    iy = iendy;

    if (insert || scrolled) {
        /* we must (re)display tail */
        ioutputs(keybuf->data + keyboard_pos, keybuf->len - keyboard_pos);
        if (visual) ipos();
        else { bufputnc('\010', iendx - ix);  cx -= (iendx - ix); }

    } else if ((iendy - oiey) * Wrap + iendx - oiex < 0) {
        /* if we didn't write past old iendx/iendy, restore them */
        iendx = oiex;
        iendy = oiey;
    }

    bufflush();
}

void inewline(void)
{
    ix = iendx = 1;
    if (!visual) {
	if (expnonvis) {
	    clear_input_line();
	} else {
	    crnl(1);  cx = 1; cy++;
	}
        if (prompt) set_refresh_pending(REF_PHYSICAL);

    } else {
        if (cleardone) {
            clear_input_window();
        } else {
            iy = iendy + 1;
            if (iy > lines) {
                if (scroll && !clearfull) {
                    scroll_input(1);
                    iy--;
                } else {
                    clear_input_window();
                }
            }
        }
        istarty = iendy = iy;
        set_refresh_pending(prompt ? REF_LOGICAL : REF_PHYSICAL);
    }

    bufflush();
}

/* idel() assumes place is in bounds and != keyboard_pos. */
void idel(int place)
{
    int len;
    int oiey = iendy;

    if ((len = place - keyboard_pos) < 0) keyboard_pos = place;
    if (!sockecho()) return;
    if (len < 0) ix += len;
    
    if (!visual) {
	int prompt_len = prompt ? prompt->len % Wrap : 0;
        if (ix < prompt_len + 1 || need_refresh) {
            physical_refresh();
            return;
        }
        if (expnonvis && ix == prompt_len + 1 && keyboard_pos == keybuf->len) {
	    /* there would be nothing left; slide the window so there is */
            physical_refresh();
            return;
        }
        if (len < 0) { bufputnc('\010', -len);  cx += len; }

    } else {
        /* visual */
        if (ix < 1) {
            iy -= ((-ix) / Wrap) + 1;
            ix = Wrap - ((-ix) % Wrap);
        }
        if (iy < in_top) {
            logical_refresh();
            return;
        }
        physical_refresh();
    }

    if (len < 0) len = -len;

    if (visual && delete_char &&
        keybuf->len - keyboard_pos > 3 && len < Wrap/3)
    {
        /* hardware method */
        int i, space, pos;

        iendy = iy;
        if (ix + len <= Wrap) {
            for (i = len; i; i--) tp(delete_char);
            iendx = Wrap + 1 - len;
        } else {
            iendx = ix;
        }
        pos = keyboard_pos - ix + iendx;

        while (pos < keybuf->len) {
            if ((space = Wrap - iendx + 1) <= 0) {
                if (iendy == lines) break;   /* at end of window */
                xy(iendx = 1, ++iendy);
                for (i = len; i; i--) tp(delete_char);
                space = Wrap - len;
                if (space > keybuf->len - pos) space = keybuf->len - pos;
            } else {
                xy(iendx, iendy);
                if (space > keybuf->len - pos) space = keybuf->len - pos;
                ictrl_put(keybuf->data + pos, space);  cx += space;
            }
            iendx += space;
            pos += space;
        }

        /* erase tail */
        if (iendy < oiey) {
            crnl(1); cx = 1; cy++;
            clear_line();
        }

    } else {
        /* redisplay method */
        iendx = ix;
        iendy = iy;
        ioutputs(keybuf->data + keyboard_pos, keybuf->len - keyboard_pos);

        /* erase tail */
        if (len > Wrap - cx + 1) len = Wrap - cx + 1;
        if (visual && clear_to_eos && (len > 2 || cy < oiey)) {
            tp(clear_to_eos);
        } else if (clear_to_eol && len > 2) {
            tp(clear_to_eol);
            if (visual && cy < oiey) clear_lines(cy + 1, oiey);
        } else {
            bufputnc(' ', len);  cx += len;
            if (visual && cy < oiey) clear_lines(cy + 1, oiey);
        }
    }
    
    /* restore cursor */
    if (visual) ipos();
    else { bufputnc('\010', cx - ix);  cx = ix; }

    bufflush();
}

int igoto(int place)
{
    int diff;

    if (place < 0)
        place = 0;
    if (place > keybuf->len)
        place = keybuf->len;
    diff = place - keyboard_pos;
    keyboard_pos = place;

    if (!diff) {
        /* no physical change */
	dobell(1);

    } else if (!sockecho()) {
        /* no physical change */

    } else if (!visual) {
	int prompt_len = prompt ? prompt->len % Wrap : 0;
        ix += diff;
        if (ix-1 < prompt_len) { /* off left edge of screen/prompt */
	    if (expnonvis && insert_char && prompt_len - (ix-1) <= Wrap/2) {
		/* can scroll, and amount of scroll needed is <= half screen */
		int i, n;
		bufputc('\r');
		if (prompt)
		    hwrite(prompt, prompt->len - prompt_len, prompt_len, 0);
		n = prompt_len - (ix-1); /* get ix onto screen */
		if (sidescroll > n) n = sidescroll; /* bring up to minimum */
		if (n > keyboard_pos-diff) n = keyboard_pos-diff; /* too far? */
		for (i = 0; i < n; i++)
		    tp(insert_char);
		ix += i;
		ictrl_put(keybuf->data + keyboard_pos + prompt_len - (ix-1), i);
                bufputnc('\010', (prompt_len + i) - (ix - 1));
		cx = ix;
		cy = lines;
	    } else {
		physical_refresh();
	    }
        } else if (ix > Wrap) { /* off right edge of screen */
	    if (expnonvis) {
		if (ix - Wrap > Wrap/2) {
		    physical_refresh();
		} else {
		    /* amount of scroll needed is <= half screen */
		    int offset = place - ix + iendx;
		    int i;
		    bufputc('\r');
		    if (prompt)
			hwrite(prompt, prompt->len - prompt_len, prompt_len, 0);
		    for (i = 0; i < sidescroll || i < ix - Wrap; i++)
			tp(delete_char);
		    iendx -= i;
		    ix -= i;
		    cx = prompt_len + 1;
		    cy = lines;
		    xy(iendx, lines);
		    ioutputs(keybuf->data + offset, keybuf->len - offset);
		    diff -= i;
		    offset += i;
		    xy(ix, lines);
		}
	    } else {
		crnl(1);  cx = 1;  /* old text scrolls up, for continutity */
		physical_refresh();
	    }
        } else { /* on screen */
            cx += diff;
            if (diff < 0)
                bufputnc('\010', -diff);
            else 
                ictrl_put(keybuf->data + place - diff, diff);
        }

    /* visual */
    } else {
        int new = (ix - 1) + diff;
        iy += ndiv(new, Wrap);
        ix = nmod(new, Wrap) + 1;

        if ((iy > lines) && (iy - lines < isize) && scroll) {
            scroll_input(iy - lines);
            ioutall(place - (ix - 1) - (iy - lines - 1) * Wrap);
            iy = lines;
            ipos();
        } else if ((iy < in_top) || (iy > lines)) {
            logical_refresh();
        } else {
            ipos();
        }
    }

    bufflush();
    return keyboard_pos;
}

void do_refresh(void)
{
    if (visual && need_more_refresh) update_status_field(NULL, STAT_MORE);
    if (need_refresh >= REF_LOGICAL) logical_refresh();
    else if (need_refresh >= REF_PHYSICAL) physical_refresh();
}

void physical_refresh(void)
{
    if (visual) {
        setscroll(1, lines);
        ipos();
    } else {
	int start;
	int prompt_len = 0;
        clear_input_line();
	if (prompt) {
	    prompt_len = (prompt->len + 1) % Wrap - 1;
	    hwrite(prompt, prompt->len - prompt_len, prompt_len, 0);
	    iendx = prompt_len + 1;
	}
	ix = (sockecho()?keyboard_pos:0) % (Wrap - prompt_len) + 1 + prompt_len;
        start = (sockecho()?keyboard_pos:0) - (ix - 1) + prompt_len;
	if (start == keybuf->len && keybuf->len > 0) { /* would print nothing */
	    /* slide window so something is visible */
	    if (start > Wrap - prompt_len) {
		ix += Wrap - prompt_len;
		start -= Wrap - prompt_len;
	    } else {
		ix += start;
		start = 0;
	    }
	}
	ioutall(start);
        bufputnc('\010', iendx - ix);  cx -= (iendx - ix);
    }
    bufflush();
    if (need_refresh <= REF_PHYSICAL) need_refresh = 0;
    old_ix = -1; /* invalid */
}

void logical_refresh(void)
{
    int kpos, nix, niy;

    if (!visual)
        oflush();  /* no sense refreshing if there's going to be output after */

    kpos = prompt ? -(prompt->len % Wrap) : 0;
    nix = ((sockecho() ? keyboard_pos : 0) - kpos) % Wrap + 1;

    if (visual) {
        setscroll(1, lines);
        niy = istarty + (keyboard_pos - kpos) / Wrap;
        if (niy <= lines) {
            clear_input_line();
        } else {
            clear_input_window();
            kpos += (niy - lines) * Wrap;
            niy = lines;
        }
        ioutall(kpos);
        ix = nix;
        iy = niy;
        ipos();
    } else {
        clear_input_line();
	if (expnonvis) {
	    int plen = 0;
	    if (prompt) {
		plen = (prompt->len + 1) % Wrap - 1;
		hwrite(prompt, prompt->len - plen, plen, 0);
		iendx = plen + 1;
	    }
	    ix = (old_ix >= iendx && old_ix <= Wrap) ? old_ix :
		(sockecho()?keyboard_pos:0) % (Wrap - plen) + 1 + plen;
	    old_ix = -1; /* invalid */
	    kpos = (sockecho()?keyboard_pos:0) - (ix - 1) + plen;
	    if (kpos == keybuf->len && keybuf->len > 0) {
		/* would print nothing; slide window so something is visible */
		if (kpos > Wrap/2) {
		    ix += Wrap/2;
		    kpos -= Wrap/2;
		} else {
		    ix += kpos;
		    kpos -= kpos;
		}
	    }
	    ioutall(kpos);
	} else {
	    ioutall(kpos);
	    kpos += Wrap;
	    while ((sockecho() && kpos <= keyboard_pos) || kpos < 0) {
		crnl(1);  cx = 1;
		iendx = 1;
		ioutall(kpos);
		kpos += Wrap;
	    }
	    ix = nix;
	}
	bufputnc('\010', iendx - ix);  cx -= (iendx - ix);
    }
    bufflush();
    if (need_refresh <= REF_LOGICAL) need_refresh = 0;
}

void update_prompt(conString *newprompt, int display)
{
    conString *oldprompt = prompt;

    if (oldprompt == moreprompt) return;
    prompt = newprompt;
    old_ix = -1;
    if ((oldprompt || prompt) && display)
        set_refresh_pending(REF_LOGICAL);
}


/*****************************************************
 *                                                   *
 *                  OUTPUT HANDLING                  *
 *                                                   *
 *****************************************************/

/*************
 * Utilities *
 *************/

static void attributes_off(attr_t attrs)
{
    const char *ctlseq;

    if (attrs & F_HILITE) attrs |= hiliteattr;
    if (have_attr & attrs & F_SIMPLE) {
        if (attr_off) tp(attr_off);
        else {
            if (have_attr & attrs & F_UNDERLINE) tp(underline_off);
            if (have_attr & attrs & F_BOLD     ) tp(standout_off);
        }
    }
    if ((attrs & F_COLORS) && (ctlseq = getvar("end_color"))) {
        print_to_ascii(outbuf, ctlseq);
    }
}

static void attributes_on(attr_t attrs)
{
    if (attrs & F_HILITE)
        attrs |= hiliteattr;

#if 0
    if (attr_on) {
        /* standout, underline, reverse, blink, dim, bold, blank, prot., ACS */
        tp(tparm(attr_on, 
            (have_attr & attrs & F_BOLD && !bold),
            (have_attr & attrs & F_UNDERLINE),
            (have_attr & attrs & F_REVERSE),
            (have_attr & attrs & F_FLASH),
            (have_attr & attrs & F_DIM),
            (have_attr & attrs & F_BOLD && bold),
            0, 0, 0));
        } else
#endif
    {
        /* Some emulators only show the last, so we do most important last. */
        if (have_attr & attrs & F_DIM)       tp(dim);
        if (have_attr & attrs & F_BOLD)      tp(bold ? bold : standout);
        if (have_attr & attrs & F_UNDERLINE) tp(underline);
        if (have_attr & attrs & F_REVERSE)   tp(reverse);
        if (have_attr & attrs & F_FLASH)     tp(flash);
    }

    if (attrs & F_FGCOLOR)  color_on("", attr2fgcolor(attrs));
    if (attrs & F_BGCOLOR)  color_on("bg", attr2bgcolor(attrs));
}

static void color_on(const char *prefix, long color)
{
    const char *ctlseq;
    smallstr buf;

    sprintf(buf, "start_color_%s%s", prefix, enum_color[color].data);
    if ((ctlseq = getvar(buf))) {
        print_to_ascii(outbuf, ctlseq);
    } else {
        sprintf(buf, "start_color_%s%ld", prefix, color);
        if ((ctlseq = getvar(buf)))
            print_to_ascii(outbuf, ctlseq);
    }
}

static void hwrite(conString *line, int start, int len, int indent)
{
    attr_t attrs = line->attrs & F_HWRITE;
    attr_t current = 0;
    attr_t new;
    int i, ctrl;
    int col = 0;
    char c;

    if (line->attrs & F_BELL && start == 0) {
        dobell(1);
    }

    if (indent) {
        bufputnc(' ', indent);
        cx += indent;
    }

    cx += len;

    if (!line->charattrs && hilite && attrs)
        attributes_on(current = attrs);

    for (i = start; i < start + len; ++i) {
        new = line->charattrs ? adj_attr(attrs, line->charattrs[i]) : attrs;
        c = unmapchar(localize(line->data[i]));
        ctrl = (emulation > EMUL_RAW && is_cntrl(c) && c != '\t');
        if (ctrl)
            new |= F_BOLD | F_REVERSE;
        if (new != current) {
            if (current) attributes_off(current);
            current = new;
            if (current) attributes_on(current);
        }
        if (c == '\t') {
            bufputnc(' ', tabsize - col % tabsize);
            col += tabsize - col % tabsize;
        } else {
            bufputc(ctrl ? CTRL(c) : c);
            col++;
        }
    }
    if (current) attributes_off(current);
}

void reset_outcount(Screen *screen)
{
    if (!screen) screen = display_screen;
    screen->outcount = visual ?
        (scroll ? (out_bot - out_top + 1) : screen->outcount) :
        lines - 1;
}

/* return TRUE if okay to print */
static int check_more(Screen *screen)
{
    if (!screen->paused && more && interactive && screen->outcount-- <= 0)
    {
        /* status bar is updated in oflush() to avoid scroll region problems */
        screen->paused = 1;
        do_hook(H_MORE, NULL, "");
    }
    return !screen->paused;
}

int pause_screen(void)
{
    if (display_screen->paused)
	return 0;
    display_screen->outcount = 0;
    display_screen->paused = 1;
    do_hook(H_MORE, NULL, "");
    update_status_field(NULL, STAT_MORE);
    return 1;
}

int clear_more(int new)
{
    PhysLine *pl;
    int use_insert, need_redraw = 0, scrolled = 0;

    if (new < 0) {
	if (!visual /* XXX || !can_scrollback */) return 0;
	use_insert = insert_line && -new < winlines();
	setscroll(1, out_bot);
	while (scrolled > new && prevtop(display_screen)) {
	    pl = display_screen->top->datum;
	    if (display_screen->viewsize <= winlines()) {
		/* visible area is not full:  add to the top of visible area */
		xy(1, winlines() - display_screen->viewsize + 1);
		hwrite(pl->str, pl->start,
		    pl->len < Wrap - pl->indent ? pl->len : Wrap - pl->indent,
		    pl->indent);
	    } else {
		/* visible area is full:  insert at top and push bottom off */
		display_screen->paused = 1;
		display_screen->outcount = 0;
		if (use_insert) {
		    xy(1, 1);
		    tp(insert_line);
		    hwrite(pl->str, pl->start,
			pl->len < Wrap-pl->indent ? pl->len : Wrap-pl->indent,
			pl->indent);
		} else {
		    need_redraw++;
		}
		prevbot(display_screen);
	    }
	    scrolled--;
	}
	while (scrolled > new && display_screen->viewsize > 1) {
	    /* no more lines in list to scroll on to top. insert blanks. */
	    display_screen->paused = 1;
	    display_screen->outcount = 0;
	    if (use_insert) {
		xy(1, 1);
		tp(insert_line);
	    } else {
		need_redraw++;
	    }
	    prevbot(display_screen);
	    scrolled--;
	}
	if (need_redraw) {
	    redraw();
	    return scrolled;
	}
	update_status_field(NULL, STAT_MORE);
    } else { /* new >= 0 */
	if (!display_screen->paused) return 0;
	if (display_screen->nback_filtered) {
	    if (visual) {
		setscroll(1, out_bot);
		if (cy != out_bot)
		    xy(columns, out_bot);
	    } else {
		if (!need_refresh)
		    old_ix = ix; /* physical_refresh() will restore ix */
		clear_input_line();
	    }
	    while (scrolled < new && nextbot(display_screen)) {
		pl = display_screen->bot->datum;
		if (visual) output_scroll(pl); else output_novisual(pl);
		scrolled++;
	    }
	}
	while (display_screen->viewsize > winlines())
	    nexttop(display_screen);
	if (scrolled < new) {
	    display_screen->paused = 0;
	    display_screen->outcount = new - scrolled;
	    if (visual) {
		update_status_field(NULL, STAT_MORE);
		if (!scroll) display_screen->outcount = out_bot;
	    } else {
		prompt = fgprompt();
		old_ix = -1;
		clear_input_line();
	    }
	}
    }
    set_refresh_pending(REF_PHYSICAL);
    return scrolled;
}

int tog_more(Var *var)
{
    if (!more) clear_more(display_screen->outcount);
    else reset_outcount(display_screen);
    return 1;
}

int tog_keypad(Var *var)
{
    if (!keypad_on) {
	if (keypad)
	    eprintf("don't know how to enable keypad on %s terminal", TERM);
	return 0;
    }
    tp(keypad ? keypad_on : keypad_off);
    return 1;
}

int screen_end(int need_redraw)
{
    Screen *screen = display_screen;
    int oldmore = more;

    hide_screen(screen);
    if (screen->nback_filtered) {
	screen->nback_filtered = screen->nback = 0;
	screen->nnew_filtered = screen->nnew = 0;
	special_var[VAR_more].val.u.ival = 0;

	/* XXX optimize if (jump < screenful) (but what about tmp lines?) */
	need_redraw = 1;
	screen->maxbot = screen->bot = screen->pline.tail;
	screen_refilter(screen);

	special_var[VAR_more].val.u.ival = oldmore;
    }

    screen->paused = 0;
    reset_outcount(screen);
    if (need_redraw) {
	redraw_window(screen, 0);
	if (visual) {
	    update_status_field(NULL, STAT_MORE);
	} else {
	    prompt = fgprompt();
	}
    }
    return 1;
}

int selflush(void)
{
    display_screen->selflush = 1;
    screen_refilter_bottom(display_screen);
    clear_more(winlines());
    return 1;
}


/* next_physline
 * Increment bottom of screen.  Checks for More and SELFLUSH termination.
 * Returns 1 if there is a displayable line, 0 if not.
 */
static int next_physline(Screen *screen)
{
    if (screen->paused)
	return 0;
    if (!nextbot(screen)) {
	if (screen->selflush) {
	    if (screen->selflush == 1) {
		screen->selflush++;
		screen->paused = 1;
		update_status_field(NULL, STAT_MORE);
	    } else {
		screen->selflush = 0;
		clear_screen_filter(screen);
		screen_end(1);
	    }
	}
	return 0;
    }
    if (!check_more(screen)) {
	/* undo the nextbot() */
	if (screen->maxbot == screen->bot) {
	    screen->maxbot = screen->maxbot->prev;
	    screen->nnew++;
	    screen->nnew_filtered++;
	}
	screen->bot = screen->bot->prev;
	screen->nback_filtered++;
	screen->nback++;
	screen->viewsize--;
	return 0;
    }
    if (display_screen->viewsize > winlines())
	nexttop(screen);
    return 1;
}

/* returns length of prefix of str that will fit in {wrapsize} */
int wraplen(const char *str, int len, int indent)
{
    int total, max, visible;

    if (emulation == EMUL_RAW) return len;

    max = Wrap - indent;

    for (visible = total = 0; total < len && visible < max; total++) {
	if (str[total] == '\t')
	    visible += tabsize - visible % tabsize;
	else
	    visible++;
    }

    if (total == len) return len;
    len = total;
    if (wrapflag) {
        while (len && !is_space(str[len-1]))
	    --len;
	if (wrappunct > 0 && len < total - wrappunct) {
	    len = total;
	    while (len && !is_space(str[len-1]) && !is_punct(str[len-1]))
		--len;
	}
    }
    return len ? len : total;
}


/****************
 * Main drivers *
 ****************/

int moresize(Screen *screen) {
    if (!screen) screen = display_screen;
    return screen->nback_filtered;
}

/* write to display_screen (no history) */
void screenout(conString *line)
{
    enscreen(display_screen, line);
    oflush();
}

void enscreen(Screen *screen, conString *line)
{
    int wrapped, visible;

    if (!hilite)
        line->attrs &= ~F_HWRITE;
    if (line->attrs & F_GAG && gag)
        return;

    if (!screen->pline.head) { /* initialize wrap state */
	screen->scr_wrapflag = wrapflag;
	screen->scr_wrapsize = Wrap;
	screen->scr_wrapspace = wrapspace;
	screen->scr_wrappunct = wrappunct;
    }
    visible = screen_filter(screen, line);
    wrapped = wraplines(line, &screen->pline, visible);
    screen->nlline++;
    screen->npline += wrapped;
    screen->nback += wrapped;
    screen->nnew += wrapped;
    if (visible) {
	screen->nback_filtered += wrapped;
	screen->nnew_filtered += wrapped;
    }
    purge_old_lines(screen);
}

void oflush(void)
{
    static int lastsize;
    int waspaused, count = 0;
    PhysLine *pl;
    Screen *screen = display_screen;

    if (output_disabled) return;

    if (!(waspaused = screen->paused)) {
        lastsize = 0;
        while (next_physline(screen)) {
            pl = screen->bot->datum;
            if (count++ == 0) {  /* first iteration? */
                if (screen_mode < 1) {
		    if (!need_refresh)
			old_ix = ix; /* physical_refresh() will restore ix */
                    clear_input_line();
                } else if (scroll && has_scroll_region) {
                    setscroll(1, out_bot);
                    if (cy != out_bot)
			xy(columns, out_bot);
                }
            }
            if (screen_mode < 1) output_novisual(pl);
#ifdef SCREEN
            else if (scroll) output_scroll(pl);
            else output_noscroll(pl);
#endif
            set_refresh_pending(REF_PHYSICAL);
            bufflush();
        }
    }

    if (screen->paused) {
        if (!visual) {
            if (!waspaused) {
                prompt = moreprompt;
		old_ix = -1;
                set_refresh_pending(REF_LOGICAL);
            }
        } else if (!waspaused ||
	    moresize(screen) / morewait > lastsize / morewait)
	{
            update_status_field(NULL, STAT_MORE);
        } else if (lastsize != moresize(screen)) {
            need_more_refresh = 1;
        }
        lastsize = moresize(screen);
    }
}

static void output_novisual(PhysLine *pl)
{
    hwrite(pl->str, pl->start, pl->len, pl->indent);
    crnl(1);  cx = 1; cy++;
}

#ifdef SCREEN
static void output_noscroll(PhysLine *pl)
{
    setscroll(1, lines);   /* needed after scroll_input(), etc. */
    xy(1, (oy + 1) % (out_bot) + 1);
    clear_line();
    xy(ox, oy);
    hwrite(pl->str, pl->start, pl->len, pl->indent);
    oy = oy % (out_bot) + 1;
}

static void output_scroll(PhysLine *pl)
{
    if (has_scroll_region) {
        crnl(1);
        /* Some brain damaged emulators lose attributes under cursor
         * when that '\n' is printed.  Too bad. */
    } else {
        xy(1, 1);
        tp(delete_line);
        xy(1, out_bot);
        tp(insert_line);
    }
    hwrite(pl->str, pl->start, pl->len, pl->indent);
}
#endif

void hide_screen(Screen *screen)
{
    ListEntry *node, *next;
    PhysLine *pl;

    if (!screen) screen = fg_screen;
    if (screen->viewsize > 0) {
	/* delete any temp lines in [top,bot] */
	int done;
	for (done = 0, node = screen->top; !done; node = next) {
	    done = (node == screen->bot);
	    next = node->next;
	    pl = node->datum;
	    if (pl->tmp) {
		if (screen->top == node)
		    screen->top = node->next;
		if (screen->bot == node)
		    screen->bot = node->prev;
		if (screen->maxbot == node)
		    screen->maxbot = node->prev;
		unlist(node, &screen->pline);
		conStringfree(pl->str);
		pfree(pl, plpool, str);
		screen->viewsize--;
		screen->npline--;
		screen->nlline--;
	    }
	}
    }
}

void unhide_screen(Screen *screen)
{
    PhysLine *pl;

    if (!virtscreen || !textdiv || !visual) {
	return;

    } else if (textdiv == TEXTDIV_CLEAR) {
	clear_screen_view(screen);

    } else if (textdiv_str && fg_screen->maxbot &&
	((PhysLine*)(fg_screen->maxbot->datum))->str != textdiv_str &&
	(textdiv == TEXTDIV_ALWAYS ||
	    fg_screen->maxbot != fg_screen->bot || fg_screen->maxbot->next))
	/* If textdiv is enabled and there's no divider at maxbot already... */
    {
	/* insert divider at maxbot */
	palloc(pl, PhysLine, plpool, str, __FILE__, __LINE__);
	pl->visible = 1;
	pl->tmp = 1;
	(pl->str = textdiv_str)->links++;
	pl->start = 0;
	pl->indent = 0;
	pl->len = wraplen(textdiv_str->data, textdiv_str->len, 0);
	inlist(pl, &fg_screen->pline, fg_screen->maxbot);
	if (fg_screen->bot == fg_screen->maxbot) {
	    /* insert ABOVE bot, so it doesn't look like new activity */
	    fg_screen->bot = fg_screen->maxbot->next;
	    if (fg_screen->viewsize == 0)
		fg_screen->top = fg_screen->bot;
	    fg_screen->viewsize++;
	    if (fg_screen->viewsize >= winlines())
		fg_screen->partialview = 0;
	} else {
	    /* inserting BELOW bot; we must increment nback* */
	    fg_screen->nback++;
	    fg_screen->nback_filtered++;
	}
	fg_screen->maxbot = fg_screen->maxbot->next;
	fg_screen->npline++;
	fg_screen->nlline++;
    }
}

/*
 * Switch to new fg_screen.
 */
void switch_screen(int quiet)
{
    if (fg_screen != display_screen) {	/* !virtscreen */
        /* move lines from fg_screen to display_screen */
	/* XXX optimize when no filter */
	PhysLine *pl;
        List *dest = &display_screen->pline;
	List *src = &fg_screen->pline;
	while (src->head) {
	    dest->tail->next = src->head;
	    dest->tail = dest->tail->next;
	    src->head = src->head->next;
	    display_screen->nnew++;
	    display_screen->nback++;
	    pl = dest->tail->datum;
	    if (screen_filter(display_screen, pl->str)) {
		display_screen->nnew_filtered++;
		display_screen->nback_filtered++;
	    }
	}
        src->head = src->tail = NULL;
        fg_screen->nback_filtered = fg_screen->nback = 0;
        fg_screen->nnew_filtered = fg_screen->nnew = 0;
        fg_screen->npline = 0;
        fg_screen->nlline = 0;
        fg_screen->maxbot = fg_screen->bot = fg_screen->top = NULL;
    }
    update_status_field(NULL, STAT_WORLD);
    if (quiet) {
        /* jump to end */
	screen_end(1);
    } else {
	if (virtscreen) {
	    redraw_window(display_screen, 0);
	}
	/* display new lines */
	oflush();
	update_status_field(NULL, STAT_MORE);
    }
}

#if USE_DMALLOC
void free_output(void)
{
    int row;

    tfclose(tfscreen);
    tfclose(tfalert);
    free_screen(default_screen);
    fg_screen = default_screen = NULL;
    Stringfree(outbuf);
    for (row = 0; row < max_status_height; row++) {
	Stringfree(status_line[row]);
	while (statusfield_list[row]->head)
	    free_statusfield(unlist(statusfield_list[row]->head,
		statusfield_list[row]));
    }

    pfreepool(PhysLine, plpool, str);
}
#endif

