/*************************************************************************
 *  TinyFugue - programmable mud client
 *  Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2002, 2003, 2004, 2005, 2006-2007 Ken Keys
 *
 *  TinyFugue (aka "tf") is protected under the terms of the GNU
 *  General Public License.  See the file "COPYING" for details.
 ************************************************************************/
static const char RCSid[] = "$Id: keyboard.c,v 35004.88 2007/01/13 23:12:39 kkeys Exp $";

/**************************************************
 * Fugue keyboard handling.
 * Handles all keyboard input and keybindings.
 **************************************************/

#include "tfconfig.h"
#include "port.h"
#include "tf.h"
#include "util.h"
#include "pattern.h"	/* for tfio.h */
#include "search.h"
#include "tfio.h"
#include "macro.h"	/* Macro, find_macro(), do_macro()... */
#include "keyboard.h"
#include "output.h"	/* iput(), idel(), redraw()... */
#include "history.h"	/* history_sub() */
#include "expand.h"	/* macro_run() */
#include "cmdlist.h"
#include "variable.h"	/* unsetvar() */

static int literal_next = FALSE;
static TrieNode *keynode = NULL;	/* current node matched by input */
static int kbnum_internal = 0;

int pending_line = FALSE;
int pending_input = FALSE;
struct timeval keyboard_time;

static int  dokey_newline(void);
static int  replace_input(String *line);
static void handle_input_string(const char *input, unsigned int len);


STATIC_BUFFER(scratch);                 /* buffer for manipulating text */
STATIC_BUFFER(current_input);           /* unprocessed keystrokes */
static TrieNode *keytrie = NULL;        /* root of keybinding trie */

AUTO_BUFFER(keybuf);                    /* input buffer */
int keyboard_pos = 0;                   /* current position in buffer */

/*
 * Some dokey operations are implemented internally with names like
 * DOKEY_FOO; others are implemented as macros in stdlib.tf with names
 * like /dokey_foo.  handle_dokey_command() looks first for an internal
 * function in efunc_table[], then for a macro, so all operations can be done
 * with "/dokey foo".  Conversely, internally-implemented operations should
 * have macros in stdlib.tf of the form "/def dokey_foo = /dokey foo",
 * so all operations can be performed with "/dokey_foo".
 */
enum {
#define gencode(id) DOKEY_##id
#include "keylist.h"
#undef gencode
};

static const char *efunc_table[] = {
#define gencode(id) #id
#include "keylist.h"
#undef gencode
};

#define kbnumval	(kbnum ? atoi(kbnum->data) : 1)


void init_keyboard(void)
{
    gettime(&keyboard_time);
}

/* Find the macro associated with <key> sequence. */
Macro *find_key(const char *key)
{
    return (Macro *)trie_find(keytrie, (unsigned char*)key);
}

int bind_key(Macro *spec, const char *key)
{
    int status = intrie(&keytrie, spec, (const unsigned char*)key);
    if (status < 0) {
        eprintf("'%S' is %s an existing keybinding.",
            ascii_to_print(key),
            (status == TRIE_SUPER) ? "prefixed by" : "a prefix of");
        return 0;
    }
    return 1;
}

void unbind_key(const char *key)
{
    untrie(&keytrie, (const unsigned char*)key);
    keynode = NULL;  /* in case it pointed to a node that no longer exists */
}

/* returns 0 at EOF, 1 otherwise */
int handle_keyboard_input(int read_flag)
{
    char buf[64];
    const char *s;
    int i, count = 0;
    static int key_start = 0;
    static int input_start = 0;
    static int place = 0;
    static int eof = 0;

    /* Solaris select() incorrectly reports the terminal as readable if
     * the user typed LNEXT or FLUSH, when in fact there is nothing to read.
     * So we wait for read() to return 0 several times in a row
     * (hopefully, more times than anyone would realistically type FLUSH in
     * a row) before deciding it's EOF.
     */

    if (eof < 100 && read_flag) {
        /* read a block of text */
        if ((count = read(STDIN_FILENO, buf, sizeof(buf))) < 0) {
            /* error or interrupt */
            if (errno == EINTR) return 1;
            die("handle_keyboard_input: read", errno);
        } else if (count > 0) {
            /* something was read */
	    eof = 0;
            gettime(&keyboard_time);
        } else {
            /* nothing was read, and nothing is buffered */
	    /* Don't close stdin; we might be wrong (solaris bug), and we
	     * don't want the fd to be reused anyway. */
	    eof++;
        }
    }

    if (count == 0 && place == 0)
	goto end;

    for (i = 0; i < count; i++) {
        if (istrip) buf[i] &= 0x7F;
        if (buf[i] & 0x80) {
	    if (!literal_next &&
		(meta_esc == META_ON || (!is_print(buf[i]) && meta_esc)))
	    {
		Stringadd(current_input, '\033');
		buf[i] &= 0x7F;
	    }
	    if (!is_print(buf[i]))
		buf[i] &= 0x7F;
        }
        Stringadd(current_input, mapchar(buf[i]));
    }

    s = current_input->data;
    if (!s) /* no good chars; current_input not yet allocated */
	goto end;
    while (place < current_input->len) {
        if (!keynode) keynode = keytrie;
        if ((pending_input = pending_line))
            break;
        if (literal_next) {
            place++;
            key_start++;
            literal_next = FALSE;
            continue;
        }
        while (place < current_input->len && keynode && keynode->children)
            keynode = keynode->u.child[(unsigned char)s[place++]];
        if (!keynode) {
            /* No keybinding match; check for builtins. */
            if (s[key_start] == '\n' || s[key_start] == '\r') {
                handle_input_string(s + input_start, key_start - input_start);
                place = input_start = ++key_start;
                dokey_newline();
                /* handle_input_line(); */
            } else if (s[key_start] == '\b' || s[key_start] == '\177') {
                handle_input_string(s + input_start, key_start - input_start);
                place = input_start = ++key_start;
                do_kbdel(keyboard_pos - kbnumval);
		reset_kbnum();
            } else if (kbnum && is_digit(s[key_start]) &&
		key_start == input_start)
	    {
		int n;
		SStringcpy(scratch, kbnum);
		Stringadd(scratch, s[key_start]);
		place = input_start = ++key_start;
		n = kbnumval < 0 ? -kbnumval : kbnumval;
		if (max_kbnum > 0 && n > max_kbnum)
		    Sprintf(scratch, "%c%d", kbnumval<0 ? '-' : '+', max_kbnum);
		setstrvar(&special_var[VAR_kbnum], CS(scratch), FALSE);
            } else {
                /* No builtin; try a suffix. */
                place = ++key_start;
            }
            keynode = NULL;
        } else if (!keynode->children) {
            /* Total match; process everything up to here and call the macro. */
	    int kbnumlocal = 0;
            Macro *macro = (Macro *)keynode->u.datum;
            handle_input_string(s + input_start, key_start - input_start);
            key_start = input_start = place;
            keynode = NULL;  /* before do_macro(), for reentrance */
	    if (kbnum) {
		kbnum_internal = kbnumlocal = atoi(kbnum->data);
		reset_kbnum();
	    }
            do_macro(macro, NULL, 0, USED_KEY, kbnumlocal);
	    kbnum_internal = 0;
        } /* else, partial match; just hold on to it for now. */
    }

    /* Process everything up to a possible match. */
    handle_input_string(s + input_start, key_start - input_start);

    /* Shift the window if there's no pending partial match. */
    if (key_start >= current_input->len) {
        Stringtrunc(current_input, 0);
        place = key_start = 0;
    }
    input_start = key_start;
    if (pending_line && !read_depth)
        handle_input_line();
end:
    return eof < 100;
}

/* Update the input window and keyboard buffer. */
static void handle_input_string(const char *input, unsigned int len)
{
    int putlen = len, extra = 0;

    if (len == 0) return;
    if (kbnum) {
	if (kbnumval > 1) {
	    extra = kbnumval - 1;
	    putlen = len + extra;
	}
	reset_kbnum();
    }

    /* if this is a fresh line, input history is already synced;
     * if user deleted line, input history is already synced;
     * if called from replace_input, we don't want input history synced.
     */
    if (keybuf->len) sync_input_hist();

    if (keyboard_pos == keybuf->len) {                    /* add to end */
	if (extra) {
	    Stringnadd(keybuf, *input, extra);
	    keyboard_pos += extra;
	}
	Stringncat(keybuf, input, len);
    } else if (insert) {                                  /* insert in middle */
        Stringcpy(scratch, keybuf->data + keyboard_pos);
        Stringtrunc(keybuf, keyboard_pos);
	if (extra) {
	    Stringnadd(keybuf, *input, extra);
	    keyboard_pos += extra;
	}
        Stringncat(keybuf, input, len);
        SStringcat(keybuf, CS(scratch));
    } else if (keyboard_pos + len + extra < keybuf->len) {    /* overwrite */
	while (extra) {
	    keybuf->data[keyboard_pos++] = *input;
	    extra--;
	}
	memcpy(keybuf->data + keyboard_pos, input, len);
    } else {                                              /* write past end */
        Stringtrunc(keybuf, keyboard_pos);
	if (extra) {
	    Stringnadd(keybuf, *input, extra);
	    keyboard_pos += extra;
	}
        Stringncat(keybuf, input, len);
    }                      
    keyboard_pos += len;
    iput(putlen);
}


struct Value *handle_input_command(String *args, int offset)
{
    handle_input_string(args->data + offset, args->len - offset);
    return shareval(val_one);
}


/*
 *  Builtin key functions.
 */

struct Value *handle_dokey_command(String *args, int offset)
{
    const char **ptr;
    STATIC_BUFFER(buffer);
    Macro *macro;
    int n;

    /* XXX We use kbnum_internal here, but a macro would use the local %kbnum.
     * It is possible (though unadvisable) for a macro to change the local
     * %kbnum before this point, making this code behave differently than
     * a /dokey_foo macro would.  Fetching the actual local %kbnum here would
     * make the behavior the same, but is a step in the wrong direction;
     * a better solution would be to make the local %kbnum non-modifiable
     * by macro code (but variable.c doesn't yet support const). */
    n = kbnum_internal ? kbnum_internal : 1;

    ptr = (const char **)bsearch((void*)(args->data + offset),
        (void*)efunc_table,
        sizeof(efunc_table)/sizeof(char*), sizeof(char*), cstrstructcmp);

    if (!ptr) {
        SStringocat(Stringcpy(buffer, "dokey_"), CS(args), offset);
        if ((macro = find_macro(buffer->data)))
            return newint(do_macro(macro, NULL, 0, USED_NAME, 0));
        else eprintf("No editing function %s", args->data + offset); 
        return shareval(val_zero);
    }

    switch (ptr - efunc_table) {

    case DOKEY_CLEAR:      return newint(clear_display_screen());
    case DOKEY_FLUSH:      return newint(screen_end(0));
    case DOKEY_LNEXT:      return newint(literal_next = TRUE);
    case DOKEY_NEWLINE:    return newint(dokey_newline());
    case DOKEY_PAUSE:      return newint(pause_screen());
    case DOKEY_RECALLB:    return newint(replace_input(recall_input(-n,0)));
    case DOKEY_RECALLBEG:  return newint(replace_input(recall_input(-n,2)));
    case DOKEY_RECALLEND:  return newint(replace_input(recall_input(n,2)));
    case DOKEY_RECALLF:    return newint(replace_input(recall_input(n,0)));
    case DOKEY_REDRAW:     return newint(redraw());
    case DOKEY_REFRESH:    return newint((logical_refresh(), keyboard_pos));
    case DOKEY_SEARCHB:    return newint(replace_input(recall_input(-n,1)));
    case DOKEY_SEARCHF:    return newint(replace_input(recall_input(n,1)));
    case DOKEY_SELFLUSH:   return newint(selflush());
    default:               return shareval(val_zero); /* impossible */
    }
}

static int dokey_newline(void)
{
    reset_outcount(NULL);
    inewline();
    /* We might be in the middle of a macro (^M -> /dokey newline) now,
     * so we can't process the input now, or weird things will happen with
     * current_command and mecho.  So we just set a flag and wait until
     * later when things are cleaner.
     */
    pending_line = TRUE;
    return 1;  /* return value isn't really used */
}

static int replace_input(String *line)
{
    if (!line) {
        dobell(1);
        return 0;
    }
    if (keybuf->len) {
        Stringtrunc(keybuf, keyboard_pos = 0);
        logical_refresh();
    }
    handle_input_string(line->data, line->len);
    return 1;
}

int do_kbdel(int place)
{
    if (place >= 0 && place < keyboard_pos) {
        Stringcpy(scratch, keybuf->data + keyboard_pos);
        SStringcat(Stringtrunc(keybuf, place), CS(scratch));
        idel(place);
    } else if (place > keyboard_pos && place <= keybuf->len) {
        Stringcpy(scratch, keybuf->data + place);
        SStringcat(Stringtrunc(keybuf, keyboard_pos), CS(scratch));
        idel(place);
    } else {
        dobell(1);
    }
    sync_input_hist();
    return keyboard_pos;
}

#define is_inword(c) (is_alnum(c) || (wordpunct && strchr(wordpunct, (c))))

int do_kbword(int start, int dir)
{
    int stop = (dir < 0) ? -1 : keybuf->len;
    int place = start<0 ? 0 : start>keybuf->len ? keybuf->len : start;
    place -= (dir < 0);

    while (place != stop && !is_inword(keybuf->data[place])) place += dir;
    while (place != stop && is_inword(keybuf->data[place])) place += dir;
    return place + (dir < 0);
}

int do_kbmatch(int start)
{
    static const char *braces = "(){}[]";
    const char *type;
    int dir, stop, depth = 0;
    int place = start<0 ? 0 : start>keybuf->len ? keybuf->len : start;

    while (1) {
        if (place >= keybuf->len) return -1;
        if ((type = strchr(braces, keybuf->data[place]))) break;
        ++place;
    }
    dir = ((type - braces) % 2) ? -1 : 1;
    stop = (dir < 0) ? -1 : keybuf->len;
    do {
        if      (keybuf->data[place] == type[0])   depth++;
        else if (keybuf->data[place] == type[dir]) depth--;
        if (depth == 0) return place;
    } while ((place += dir) != stop);
    return -1;
}

int handle_input_line(void)
{
    String *line;
    int result;

    SStringcpy(scratch, CS(keybuf));
    Stringtrunc(keybuf, keyboard_pos = 0);
    pending_line = FALSE;

    if (*scratch->data == '^') {
        if (!(line = history_sub(scratch))) {
            oputs("% No match.");
            return 0;
        }
        SStringcpy(keybuf, CS(line));
        iput(keyboard_pos = keybuf->len);
        inewline();
        Stringtrunc(keybuf, keyboard_pos = 0);
    } else
        line = scratch;

    if (kecho)
	tfprintf(tferr, "%S%S%A", kprefix, line, getattrvar(VAR_kecho_attr));
    gettime(&line->time);
    record_input(CS(line));
    readsafe = 1;
    result = macro_run(CS(line), 0, NULL, 0, sub, "\bUSER");
    readsafe = 0;
    return result;
}

#if USE_DMALLOC
void free_keyboard(void)
{
    tfclose(tfkeyboard);
    Stringfree(keybuf);
    Stringfree(scratch);
    Stringfree(current_input);
}
#endif
