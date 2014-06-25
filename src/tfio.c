/*************************************************************************
 *  TinyFugue - programmable mud client
 *  Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2002, 2003, 2004, 2005, 2006-2007 Ken Keys
 *
 *  TinyFugue (aka "tf") is protected under the terms of the GNU
 *  General Public License.  See the file "COPYING" for details.
 ************************************************************************/
static const char RCSid[] = "$Id: tfio.c,v 35004.114 2007/01/13 23:12:39 kkeys Exp $";


/***********************************
 * TinyFugue "standard" I/O
 *
 * Written by Ken Keys
 *
 * Provides an interface similar to stdio.
 ***********************************/

#include "tfconfig.h"
#include <sys/types.h>
#if HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif
/* #include <sys/time.h> */   /* for struct timeval, in select() */
#include <sys/stat.h>

#if HAVE_PWD_H
# include <pwd.h>	/* getpwnam() */
#endif

#include "port.h"
#include "tf.h"

#include "util.h"
#include "pattern.h"

#include "search.h"	/* queues */
#include "tfio.h"
#include "tfselect.h"
#include "output.h"
#include "attr.h"
#include "macro.h"	/* macro_body() */
#include "history.h"
#include "signals.h"	/* shell_status() */
#include "variable.h"	/* getvar() */
#include "keyboard.h"	/* keyboard_pos */
#include "expand.h"	/* current_command */
#include "cmdlist.h"

TFILE *loadfile = NULL; /* currently /load'ing file */
int loadline = 0;       /* line number in /load'ing file */
int loadstart = 0;      /* line number of start of command in /load'ing file */
int read_depth = 0;     /* nesting level of user kb reads */
int readsafe = 0;       /* safe to do a user kb read? */
TFILE *tfkeyboard;      /* user input (placeholder) */
TFILE *tfscreen;        /* screen output (placeholder) */
TFILE *tfin;            /* current input queue file */
TFILE *tfout;           /* current output queue file */
TFILE *tferr;           /* current error queue file */
TFILE *tfalert;         /* current alert queue file */
Screen *fg_screen;	/* current screen, to which tf writes */
Screen *default_screen;	/* default screen, used if unconnected or !virtscreen */

static TFILE *filemap[FD_SETSIZE];
static int selectable_tfiles = 0;
static List userfilelist[1];
static int max_fileid = 0;

static void fileputs(const char *str, FILE *fp);
static void filenputs(const char *str, int n, FILE *fp);
static void queueputline(conString *line, TFILE *file);


void init_tfio(void)
{
    int i;

    for (i = 0; i < sizeof(filemap)/sizeof(*filemap); i++)
        filemap[i] = NULL;
    init_list(userfilelist);

    tfin = tfkeyboard = tfopen("<tfkeyboard>", "q");
    tfkeyboard->mode = S_IRUSR;

    tfout = tferr = tfalert = tfscreen = tfopen("<tfscreen>", "q");
    tfscreen->mode = S_IWUSR;
    tfalert = tfopen("<tfalert>", "q");
    tfalert->mode = S_IWUSR;

    fg_screen = default_screen = new_screen(1000/*XXX make configurable*/); 
}

/* tfname
 * Use <name> if given, otherwise use body of <macro> as the name.  A leading
 * "~username" followed by '/' or end of string is expanded to <username>'s
 * home directory; a leading "~" is expanded to the user's home directory.
 */
char *tfname(const char *name, const char *macro)
{
    if (!name || !*name) {
        if (macro) {
            if (!(name=macro_body(macro)) || !*name) {
                eprintf("missing filename, and default macro %s is not defined",
                    macro);
            }
        } else {
            eprintf("missing filename");
        }
    }
    return (name && *name) ? expand_filename(name) : NULL;
}

char *expand_filename(const char *str)
{
    const char *dir, *user;
    STATIC_BUFFER(buffer);

    if (str) {
        if (*str != '~') return (char *)str;
        for (user = ++str; *str && *str != '/'; str++);
        if (str == user) {
            dir = getvar("HOME");
        } else {

#if !(HAVE_GETPWNAM && HAVE_PWD_H)
            wprintf("\"~user\" filename expansion is not supported.");
#else
            struct passwd *pw;
            Stringncpy(buffer, user, str - user);
            if ((pw = getpwnam(buffer->data)))
                dir = pw->pw_dir;
            else
#endif /* HAVE_GETPWNAM */
                return (char*)--user;
        }
        Stringcpy(buffer, dir ? dir : "");
        Stringcat(buffer, str);
    } else {
        Stringtrunc(buffer, 0);
    }
    return buffer->data;
}

Screen *new_screen(long size)
{
    Screen *screen;

    screen = XCALLOC(sizeof(Screen));
    screen->outcount = lines;
    screen->maxlline = size;
    screen->scr_wrapflag = wrapflag;
    screen->scr_wrapsize = wrapsize;
    screen->scr_wrapspace = wrapspace;
    init_list(&screen->pline);
    init_pattern(&screen->filter_pat, NULL, -1);
    return screen;
}

void free_screen_lines(Screen *screen)
{
    PhysLine *pl;

    while (screen->pline.head) {
	pl = unlist(screen->pline.head, &screen->pline);
        if (pl->str) conStringfree(pl->str);
	pfree(pl, plpool, str);
    }
}

void free_screen(Screen *screen)
{
    free_screen_lines(screen);
    free_pattern(&screen->filter_pat);
    FREE(screen);
}

/* tfopen - opens a TFILE.
 * Mode "q" will create a TF_QUEUE.
 * Mode "p" will open a command pipe for reading.
 * Modes "w", "r", and "a" open a regular file for read, write, or append.
 * If mode is "r", and the file is not found, will look for a compressed copy.
 * If tfopen() fails, it will return NULL with errno set as in fopen();
 * if found file is a directory, tfopen() will return NULL with errno==EISDIR.
 */
TFILE *tfopen(const char *name, const char *mode)
{
    int type = TF_FILE;
    FILE *fp;
    TFILE *result = NULL;
    const char *prog, *suffix;
    char *newname = NULL;
    STATIC_BUFFER(buffer);
    struct stat buf;
    MODE_T st_mode = 0;

    if (*mode == 'q') {
        struct tfile_queue { TFILE file; Queue queue; } *fq;
        errno = ENOMEM;  /* in case malloc fails */
        if (!(fq = (struct tfile_queue *)MALLOC(sizeof(*fq)))) return NULL;
        result = &fq->file;
        result->u.queue = &fq->queue;
        result->type = TF_QUEUE;
        result->name = name ? STRDUP(name) : NULL;
        result->id = -1;
        result->node = NULL;
        result->mode = S_IRUSR | S_IWUSR;
        result->tfmode = *mode;
        result->autoflush = 1;
        init_queue(result->u.queue);
        return result;
    }

    if (!name || !*name) {
        errno = ENOENT;
        return NULL;
    }

    if (*mode == 'p') {
#ifdef __CYGWIN32__
        eprintf("TF does not support pipes under cygwin32.");
        errno = EPIPE;
        return NULL;
#else
	if (restriction >= RESTRICT_SHELL) {
	    errno = EPERM;
	    return NULL;
	}
        if (!(fp = popen(name, "r"))) return NULL;
        result = (TFILE *)XMALLOC(sizeof(TFILE));
        result->type = TF_PIPE;
        result->name = STRDUP(name);
        result->id = -1;
        result->mode = S_IRUSR;
        result->tfmode = *mode;
        result->autoflush = 1;
        result->node = NULL;
        result->u.fp = fp;
        result->off = result->len = 0;
        filemap[fileno(fp)] = result;
        selectable_tfiles++;
        return result;
#endif
    }

    if ((fp = fopen(name, mode)) && fstat(fileno(fp), &buf) == 0) {
        if (buf.st_mode & S_IFDIR) {
            fclose(fp);
            errno = EISDIR;  /* must be after fclose() */
            return NULL;
        }
        newname = STRDUP(name);
        st_mode = buf.st_mode;
    }

    /* If file did not exist, look for compressed copy. */
    if (!fp && *mode == 'r' && errno == ENOENT && restriction < RESTRICT_SHELL &&
        (suffix = macro_body("compress_suffix")) && *suffix &&
        (prog = macro_body("compress_read")) && *prog)
    {
        newname = (char*)XMALLOC(strlen(name) + strlen(suffix) + 1);
        strcat(strcpy(newname, name), suffix);

        if ((fp = fopen(newname, mode)) != NULL) {  /* test readability */
            fclose(fp);
#ifdef PLATFORM_UNIX
            Sprintf(buffer, "%s %s 2>/dev/null", prog, newname);
#endif
#ifdef PLATFORM_OS2
            Sprintf(buffer, "%s %s 2>nul", prog, newname);
#endif
            fp = popen(buffer->data, mode);
            type = TF_PIPE;
        }
    }

    if (fp) {
        errno = EAGAIN;  /* in case malloc fails */
        if (!(result = (TFILE*)MALLOC(sizeof(TFILE)))) return NULL;
        result->type = type;
        result->name = newname;
        result->id = 0;
        result->node = NULL;
        result->tfmode = *mode;
        result->autoflush = 1;
        result->u.fp = fp;
        result->off = result->len = 0;
        result->mode = st_mode;
        if (*mode == 'r' || *mode == 'p') {
            result->mode |= S_IRUSR;
            result->mode &= ~S_IWUSR;
        } else {
            result->mode &= ~S_IRUSR;
            result->mode |= S_IWUSR;
        }
        result->warned = 0;
    } else {
        if (newname) FREE(newname);
    }

    return result;
}

/* tfclose
 * Close a TFILE created by tfopen().
 */
int tfclose(TFILE *file)
{
    int result;
    List *list;

    if (!file) return -1;
    if (file->name) FREE(file->name);
    switch(file->type) {
    case TF_QUEUE:
        list = &file->u.queue->list;
        while (list->head)
            Stringfree((String*)unlist(list->head, list));
        /* FREE(file->u.queue); */ /* queue was allocated with file */
        result = 0;
        break;
    case TF_FILE:
        result = fclose(file->u.fp);
        break;
    case TF_PIPE:
        filemap[fileno(file->u.fp)] = NULL;
        selectable_tfiles--;
        result = shell_status(pclose(file->u.fp));
        break;
    default:
        result = -1;
    }
    if (file->node)
        unlist(file->node, userfilelist);
    FREE(file);
    return result;
}

/* tfselect() is like select(), but also checks buffered TFILEs */
int tfselect(int nfds, fd_set *readers, fd_set *writers, fd_set *excepts,
    struct timeval *timeout)
{
    int i, count, tfcount = 0;
    fd_set tfreaders;

    if (!selectable_tfiles)
        return select(nfds, readers, writers, excepts, timeout);

    FD_ZERO(&tfreaders);

    for (i = 0; i < nfds; i++) {
        if (filemap[i] && FD_ISSET(i, readers)) {
            if (filemap[i]->off < filemap[i]->len) {
                FD_SET(i, &tfreaders);
                FD_CLR(i, readers);     /* don't check twice */
                tfcount++;
            }
        }
    }

    if (!tfcount) {
        return select(nfds, readers, writers, excepts, timeout);

    } else {
        /* we found at least one; poll the rest, but don't wait */
        struct timeval zero;
        zero = tvzero;
        count = select(nfds, readers, writers, excepts, &zero);
        if (count < 0) return count;
        count += tfcount;

        for (i = 0; tfcount && i < nfds; i++) {
            if (FD_ISSET(i, &tfreaders)) {
                FD_SET(i, readers);
                tfcount--;
            }
        }

        return count;
    }
}

/**********
 * Output *
 **********/

/* tfnputs
 * Print to a TFILE.
 * Unlike fputs(), tfnputs() always appends a newline when writing to a file.
 */
void tfnputs(const char *str, int n, TFILE *file)
{
    if (!file || file->type == TF_NULL) {
        /* do nothing */
    } else if (file->type == TF_QUEUE) {
        queueputline(CS(Stringnew(str, n, file==tferr ? error_attr : 0)), file);
    } else {
        if (n < 0)
	    fileputs(str, file->u.fp);
	else
	    filenputs(str, n, file->u.fp);
        if (file->autoflush) tfflush(file);
    }
}

/* tfputansi
 * Print to a TFILE, converting embedded ANSI display codes to String attrs.
 */
attr_t tfputansi(const char *str, TFILE *file, attr_t attrs)
{
    conString *line;

    if (file && file->type != TF_NULL) {
        line = CS(decode_ansi(str, attrs, EMUL_ANSI_ATTR, &attrs));
	line->links++;
	tfputline(line, file);
        conStringfree(line);
    }
    return attrs;
}

/* tfputline
 * Print a String to a TFILE, with embedded newline handling.
 */
void tfputline(conString *line, TFILE *file)
{
    /* Many callers pass line with links==0, so the ++ and free are vital. */
    line->links++;
    if (!file || file->type == TF_NULL) {
        /* do nothing */
    } else if (file == tfalert) {
        alert(line);
    } else if (file->type == TF_QUEUE) {
        queueputline(line, file);
    } else {
        filenputs(line->data, line->len, file->u.fp);
        if (file->autoflush) tfflush(file);
    }
    conStringfree(line);
}

static void queueputline(conString *line, TFILE *file)
{
    /* Many callers pass line with links==0, so the ++ and free are vital. */
    line->links++;
    if (file == tfscreen) {
        record_local(line);
        record_global(line);
        screenout(line);
    } else if (!file) {
        /* do nothing */
    } else if (file->type == TF_QUEUE) {
        line->links++;
        enqueue(file->u.queue, line);
    }
    conStringfree(line);
}

/* print string\n to a file, converting embedded newlines to spaces */
static void fileputs(const char *str, FILE *fp)
{
    for ( ; *str; str++) {
	register char c = *str == '\n' ? ' ' : *str;
	putc(c, fp);
    }
    putc('\n', fp);
}

/* print string\n to a file, converting embedded newlines to spaces */
static void filenputs(const char *str, int n, FILE *fp)
{
    for ( ; *str && n > 0; str++, n--) {
	register char c = *str == '\n' ? ' ' : *str;
	putc(c, fp);
    }
    putc('\n', fp);
}


/* vSprintf
 * Similar to vsprintf, except:
 * second arg is a flag, third arg is format.
 * %S is like %s, but takes a String* argument.
 * %q takes a char c and a string s; prints s, with \ before each c.
 * %b is like %q, but nonprinting characters are printed as "\octal".
 * %s, %S, %q, %b arguments may be NULL.
 * %q and %b do not support any width or "*" precision.
 * %A takes an attr_t, and sets the attributes for the whole line
 * (not implemented: %a sets the attrs for the remainder of the line)
 * newlines are not allowed in the format string (this is not enforced).
 */

void vSprintf(String *buf, int flags, const char *fmt, va_list ap)
{
    static smallstr spec, tempbuf;
    const char *q, *sval;
    char *specptr, quote;
    const conString *Sval;
    int len, min, max, leftjust, stars;
    attr_t attrs = buf->attrs;

    if (!(flags & SP_APPEND) && buf->data) Stringtrunc(buf, 0);
    while (*fmt) {
        if (*fmt != '%' || *++fmt == '%') {
            for (q = fmt + 1; *q && *q != '%'; q++);
            Stringfncat(buf, fmt, q - fmt);
            fmt = q;
            continue;
        }

        specptr = spec;
        *specptr++ = '%';
        for (stars = 0; *fmt && !is_alpha(*fmt); fmt++) {
            if (*fmt == '*') stars++;
            *specptr++ = *fmt;
        }
        if (*fmt == 'h' || lcase(*fmt) == 'l') *specptr++ = *fmt++;
        *specptr = *fmt;
        *++specptr = '\0';

        switch (*fmt) {
        case 'd': case 'i':
        case 'x': case 'X': case 'u': case 'o':
        case 'f': case 'e': case 'E': case 'g': case 'G':
        case 'p':
            vsprintf(tempbuf, spec, ap);
            Stringcat(buf, tempbuf);
            /* eat the arguments used by vsprintf() */
            while (stars--) (void)va_arg(ap, int);
            switch (*fmt) {
            case 'd': case 'i':
                (void)va_arg(ap, int); break;
            case 'x': case 'X': case 'u': case 'o':
                (void)va_arg(ap, unsigned int); break;
            case 'f': case 'e': case 'E': case 'g': case 'G':
                (void)va_arg(ap, double); break;
            case 'p':
                (void)va_arg(ap, void *); break;
            }
            break;
        case 'c':
            Stringadd(buf, (char)va_arg(ap, int));
            break;
        case 's':
        case 'S':
            sval = NULL;
            Sval = NULL;
            min = 0;
            max = -1;

            specptr = &spec[1];
            if ((leftjust = (*specptr == '-')))
                specptr++;
            if (*specptr == '*') {
                ++specptr;
                min = va_arg(ap, int);
            } else if (is_digit(*specptr)) {
                min = strtoint(specptr, &specptr);
            }
            if (*specptr == '.') {
                ++specptr;
                if (*specptr == '*') {
                    ++specptr;
                    max = va_arg(ap, int);
                } else if (is_digit(*specptr)) {
                    max = strtoint(specptr, &specptr);
                }
            }

            if (*fmt == 's') {
                sval = va_arg(ap, char *);
		if (flags & SP_CHECK) sval = checkstring(sval);
                len = sval ? strlen(sval) : 0;
            } else {
                Sval = va_arg(ap, const conString *);
                len = Sval ? Sval->len : 0;
            }

            if (max >= 0 && len > max) len = max;
            if (!leftjust && len < min) Stringnadd(buf, ' ', min - len);
            if (Sval)
                SStringncat(buf, Sval, len);
            else
                Stringfncat(buf, sval ? sval : "", len);
            if (leftjust && len < min) Stringnadd(buf, ' ', min - len);
            break;
        case 'q':
        case 'b':
	    max = (spec[1] == '.') ? atoi(&spec[2]) : 0x7FFF;
            if (!(quote = (char)va_arg(ap, int))) break;
            if (!(sval = va_arg(ap, char *))) break;
	    if (flags & SP_CHECK) sval = checkstring(sval);
            for ( ; *sval; sval = q) {
		if (*fmt == 'b' && !is_print(*q)) {
		    if (max < 4) break;
		    max -= 4;
		    sprintf(tempbuf, "\\%03o", *sval++);
		    Stringcat(buf, tempbuf);
                } else if (*sval == quote || *sval == '\\') {
		    if (max < 2) break;
		    max -= 2;
                    Stringadd(buf, '\\');
                    Stringadd(buf, *sval++);
                }
                for (q = sval; max > 0 && *q; q++, max--) {
		    if (*q == quote || *q == '\\') break;
		    if (*fmt == 'b' && !is_print(*q)) break;
		}
                Stringfncat(buf, sval, q - sval);
            }
            break;
	case 'A':
	    attrs = (attr_t)va_arg(ap, attr_t);
	    break;
        default:
            Stringcat(buf, spec);
            break;
        }
        fmt++;
    }
    if (!buf->data) Stringtrunc(buf, 0); /* force buf->data != NULL */
    buf->attrs = attrs;
}

#ifndef oprintf
/* oprintf
 * A newline will appended.  See vSprintf().
 */

void oprintf(const char *fmt, ...)
{
    va_list ap;
    String *buffer;

    buffer = Stringnew(NULL, -1, 0);
    va_start(ap, fmt);
    vSprintf(buffer, 0, fmt, ap);
    va_end(ap);
    oputline(CS(buffer));
}
#endif /* oprintf */

/* tfprintf
 * Print to a TFILE.  A newline will appended.  See vSprintf().
 */

void tfprintf(TFILE *file, const char *fmt, ...)
{
    va_list ap;
    String *buffer;

    buffer = Stringnew(NULL, -1, 0);
    va_start(ap, fmt);
    vSprintf(buffer, 0, fmt, ap);
    va_end(ap);
    tfputline(CS(buffer), file);
}


/* Sprintf
 * Print into a String.  See vSprintf().
 */
void Sprintf(String *buf, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vSprintf(buf, 0, fmt, ap);
    va_end(ap);
}

void Sappendf(String *buf, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vSprintf(buf, SP_APPEND, fmt, ap);
    va_end(ap);
}

void eprefix(String *buffer)
{
    extern char current_opt;
    Stringcpy(buffer, "% ");
    if (loadfile) {
        Sappendf(buffer, "%s, line", loadfile->name);
        if (loadstart == loadline)
            Sappendf(buffer, " %d: ", loadline);
        else
            Sappendf(buffer, "s %d-%d: ", loadstart, loadline);
    }
    if (current_command && *current_command != '\b')
	Sappendf(buffer, current_opt ? "%s -%c: " : "%s: ",
	    current_command, current_opt);
}

static void veprintf(int warning, const char *fmt, va_list ap)
{
    String *buffer;
    buffer = Stringnew(NULL, -1, 0);
    eprefix(buffer);
    if (warning)
	Stringcat(buffer, "Warning: ");
    vSprintf(buffer, SP_APPEND|SP_CHECK, fmt, ap);
    if (!buffer->attrs)
	buffer->attrs = warning ? warning_attr : error_attr;
    tfputline(CS(buffer), tferr);
}

/* print a formatted error message */
void eprintf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    veprintf(0, fmt, ap);
    va_end(ap);
}

/* print a formatted warning message */
void wprintf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    veprintf(1, fmt, ap);
    va_end(ap);
}

static const char interrmsg[] =
    "Please report this to the author, and describe what you did.";

void internal_error(const char *file, int line, const char *fmt, ...)
{
    va_list ap;

    eprintf("Internal error at %s:%d, %s.  %s", file, line, version, interrmsg);
    if (current_command) eprintf("cmd: \"%.32b\"", '\"', current_command);

    va_start(ap, fmt);
    veprintf(0, fmt, ap);
    va_end(ap);
}

void internal_error2(const char *file, int line, const char *file2, int line2,
    const char *fmt, ...)
{
    va_list ap;

    eprintf("Internal error at %s:%d (%s:%d), %s.  %s",
	file, line, file2, line2, version, interrmsg);
    if (current_command) eprintf("cmd: \"%.32b\"", '\"', current_command);

    va_start(ap, fmt);
    veprintf(0, fmt, ap);
    va_end(ap);
}


/*********
 * Input *
 *********/

/* read one char from keyboard, with blocking */
char igetchar(void)
{
    char c;
    fd_set readers;

    FD_ZERO(&readers);
    FD_SET(STDIN_FILENO, &readers);
    while(select(1, &readers, NULL, NULL, NULL) <= 0);
    read(STDIN_FILENO, &c, 1);
    return c;
}

int tfreadable(TFILE *file)
{
    if (!file) {
        return 1; /* tfread will imeediately return error */

    } else if (file == tfkeyboard) {
	return 0; /* tfread will not return immediately */

    } else if (file->type == TF_QUEUE) {
	return 1; /* tfread will immediately return line or EOF */

    } else {
	struct timeval timeout = tvzero;
	fd_set readers;
	int count;

        if (file->len < 0) return 1;  /* tfread will return eof or error */

	FD_ZERO(&readers);
	FD_SET(fileno(file->u.fp), &readers);

	count = select(fileno(file->u.fp) + 1, &readers, NULL, NULL, &timeout);
	if (count < 0) {
	    return -1;
	} else if (count == 0) {
	    return 0;
	}
	return 1;
    }
}

/* Unlike fgets, tfgetS() does not retain terminating newline. */
String *tfgetS(String *str, TFILE *file)
{
    if (!file) {
        return NULL;

    } else if (file == tfkeyboard) {
        /* This is a hack.  It's a useful feature, but doing it correctly
         * without blocking tf would require making the macro language
         * suspendable, which would have required a major redesign.  The
         * nested main_loop() method was easy to add, but leads to a few
         * quirks, like the odd handling of /dokey newline.
         */
        TFILE *oldtfout, *oldtfin;

        if (!readsafe) {
            eprintf("keyboard can only be read from a command line command.");
            return NULL;
        }
        if (read_depth) wprintf("nested keyboard read");
        oldtfout = tfout;
        oldtfin = tfin;
        tfout = tfscreen;
        tfin = tfkeyboard;
        readsafe = 0;
        read_depth++; update_status_field(NULL, STAT_READ);
        main_loop();
        read_depth--; update_status_field(NULL, STAT_READ);
        readsafe = 1;
        tfout = oldtfout;
        tfin = oldtfin;
        if (interrupted())
            return NULL;

        SStringcpy(str, CS(keybuf));
        Stringtrunc(keybuf, keyboard_pos = 0);
        return str;

    } else if (file->type == TF_QUEUE) {
        conString *line;
        do {
            if (!(line = dequeue(file->u.queue))) return NULL;
            if (!((line->attrs & F_GAG) && gag)) break;
            conStringfree(line);
        } while (1);
        SStringcpy(str, line); /* TODO: get rid of this copy */
        conStringfree(line);
        return str;

    } else {
        int next;

        if (file->len < 0) return NULL;  /* eof or error */

        Stringtrunc(str, 0);

        do {
            while (file->off < file->len) {
                next = file->off + 1;
                if (file->buf[file->off] == '\n') {
                    file->off++;
                    return str;
                } else if (file->buf[file->off] == '\t') {
                    file->off++;
                    Stringnadd(str, ' ', tabsize - str->len % tabsize);
                }
                while (is_print(file->buf[next]) && next < file->len)
                    next++;
                Stringfncat(str, file->buf + file->off,
                    next - file->off);
                file->off = next;
            }
            file->off = 0;
            file->len = read(fileno(file->u.fp), file->buf, sizeof(file->buf));
        } while (file->len > 0);

        file->len = -1;  /* note eof */
        return str->len ? str : NULL;
    }
}


/**************
 * User level *
 **************/

int handle_tfopen_func(const char *name, const char *mode)
{
    TFILE *file;

    if (restriction >= RESTRICT_FILE) {
	/* RESTRICT_SHELL is checked by tfopen() */
        eprintf("restricted");
        return -1;
    }

    if (mode[1] || !strchr("rwapq", mode[0])) {
        eprintf("invalid mode '%s'", mode);
        return -1;
    }
    file = tfopen(expand_filename(name), mode);
    if (!file) {
        eprintf("%s: %s", name, strerror(errno));
        return -1;
    }

    file->node = inlist(file, userfilelist, userfilelist->tail);
    file->id = ++max_fileid;
    return file->id;
}

TFILE *find_tfile(const char *handle)
{
    ListEntry *node;
    int id;

    if (is_alpha(handle[0]) && !handle[1]) {
        switch(lcase(handle[0])) {
            case 'i':  return tfin;
            case 'o':  return tfout;
            case 'e':  return tferr;
            case 'a':  return tfalert;
            default:   break;
        }
    } else {
        id = atoi(handle);
        for (node = userfilelist->head; node; node = node->next) {
            if (((TFILE*)node->datum)->id == id)
                return (TFILE*)node->datum;
        }
    }
    eprintf("%s: bad handle", handle);
    return NULL;
}

TFILE *find_usable_tfile(const char *handle, int mode)
{
    TFILE *tfile;

    if (!(tfile = find_tfile(handle)))
        return NULL;

    if (mode) {
        if (!(tfile->mode & mode) ||
            (mode & S_IRUSR && (tfile == tfout || tfile == tferr || tfile == tfalert)) ||
            (mode & S_IWUSR && (tfile == tfin))) {
            eprintf("stream %s is not %sable", handle,
                mode == S_IRUSR ? "read" : "writ");
            return NULL;
        }
    }

    return tfile;
}

struct Value *handle_liststreams_command(String *args, int offset)
{
    int count = 0;
    TFILE *file;
    ListEntry *node;

    if (!userfilelist->head) {
        oprintf("% No open streams.");
        return shareval(val_zero);
    }
    oprintf("HANDLE MODE FLUSH NAME");
    for (node = userfilelist->head; node; node = node->next) {
        file = (TFILE*)node->datum;
        oprintf("%6d   %c   %3s  %s", file->id, file->tfmode,
            (file->tfmode == 'w' || file->tfmode == 'a') ?
                enum_flag[file->autoflush].data : "",
            file->name ? file->name : "");
        count++;
    }

    return newint(count);
}

