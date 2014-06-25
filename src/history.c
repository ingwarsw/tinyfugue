/*************************************************************************
 *  TinyFugue - programmable mud client
 *  Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2002, 2003, 2004, 2005, 2006-2007 Ken Keys
 *
 *  TinyFugue (aka "tf") is protected under the terms of the GNU
 *  General Public License.  See the file "COPYING" for details.
 ************************************************************************/
static const char RCSid[] = "$Id: history.c,v 35004.114 2007/01/13 23:12:39 kkeys Exp $";


/****************************************************************
 * Fugue history and logging                                    *
 *                                                              *
 * Maintains the circular lists for input and output histories. *
 * Handles text queuing and file I/O for logs.                  *
 ****************************************************************/

#include <limits.h>
#include "tfconfig.h"
#include "port.h"
#include "tf.h"
#include "util.h"
#include "pattern.h"
#include "search.h"		/* CQueue; List in recall_history() */
#include "tfio.h"
#include "history.h"
#include "socket.h"		/* xworld() */
#include "world.h"
#include "output.h"		/* update_status_field(), etc */
#include "attr.h"
#include "macro.h"		/* add_new_macro() */
#include "cmdlist.h"
#include "keyboard.h"		/* keybuf */
#include "variable.h"		/* set_var_by_*() */
#include "signals.h"		/* interrupted() */

const int feature_history = !(NO_HISTORY - 0);
#if !NO_HISTORY

#define GLOBALSIZE    1000	/* global history size */
#define LOCALSIZE      100	/* local history size */
#define INPUTSIZE      100	/* command history buffer size */

typedef struct History {	/* circular list of lines, and logfile */
    CQueue cq;
    TFILE *logfile;
    const char *logname;
} History;

static int      next_hist_opt(const char **ptr, int *offsetp, History **histp,
		    void *u);
static void     save_to_hist(History *hist, conString *line);
static void     save_to_log(History *hist, const conString *str);
static void     hold_input(const conString *str);
static void     listlog(World *world);
static void     stoplog(World *world);
static int      do_watch(const char *args, int id, int *wlines, int *wmatch);


static struct History input[1];
static int wnmatch = 4, wnlines = 5, wdmatch = 2, wdlines = 5;

struct History globalhist_buf, localhist_buf;
struct History * const globalhist = &globalhist_buf;
struct History * const localhist = &localhist_buf;
int log_count = 0;
int nohistory = 0;	/* supress history (but not log) recording */
int nolog = 0;		/* supress log (but not history) recording */

#define histline(hist, i) \
    ((String*)(hist)->cq.data[nmod(i, (hist)->cq.maxsize)])

static void free_hist_datum(void *datum, const char *file, int line)
{
    Stringfree_fl(datum, file, line);
}

struct History *init_history(History *hist, int maxsize)
{
    if (!hist) hist = (History*)XMALLOC(sizeof(History));
    hist->logfile = NULL;
    init_cqueue(&hist->cq, maxsize, free_hist_datum);
    return hist;
}

inline void sync_input_hist(void)
{
    input->cq.index = input->cq.last;
}

void init_histories(void)
{
    init_history(input, INPUTSIZE);
    init_history(globalhist, GLOBALSIZE);
    init_history(localhist, LOCALSIZE);
    save_to_hist(input, blankline);
    sync_input_hist();
}

#if USE_DMALLOC
void free_histories(void)
{
    free_history(input);
    free_history(globalhist);
    free_history(localhist);
}
#endif

void free_history(History *hist)
{
    free_cqueue(&hist->cq);
    if (hist->logfile) {
	tfclose(hist->logfile);
	--log_count;
	update_status_field(NULL, STAT_LOGGING);
    }
}

static void save_to_hist(History *hist, conString *line)
{
    if (line->time.tv_sec < 0) gettime(&line->time);
    if (!hist->cq.data)
	hist->cq.maxsize = histsize;
    encqueue(&hist->cq, line);
    line->links++;
}

static void save_to_log(History *hist, const conString *str)
{
    if (wraplog) {
        /* ugly, but some people want it */
	const char *p = str->data;
        int i = 0, first = TRUE, len, remaining = str->len;
        do { /* must loop at least once, to handle empty string case */
            if (!first && wrapflag)
                for (i = wrapspace; i; i--) tfputc(' ', hist->logfile);
            len = wraplen(p, remaining, !first);
	    tfnputs(p, len, hist->logfile);
            first = FALSE;
	    p += len;
	    remaining -= len;
        } while (remaining);
    } else {
        tfputs(str->data, hist->logfile);
    }
    tfflush(hist->logfile);
}

void recordline(History *hist, conString *line)
{
    if (!(line->attrs & F_NOHISTORY) && !nohistory)
	save_to_hist(hist, line);
    if (hist->logfile && !nolog && !(line->attrs & F_NOLOG))
	save_to_log(hist, line);
}

static void hold_input(const conString *instr)
{
    String *str = Stringnew(instr->data, -1, sockecho() ? 0 : F_GAG);
    str->links++;
    gettime(&str->time);
    cqueue_replace(&input->cq, str, input->cq.last);
}

void record_input(const conString *str)
{
    int is_duplicate = 0;

    sync_input_hist();

    if (!str->data) return;
    if (input->cq.size > 1) {
        const String *prev_line = histline(input, input->cq.last-1);
        is_duplicate = (strcmp(str->data, prev_line->data) == 0);
    }

    if (!is_duplicate) {
        hold_input(str);
        save_to_hist(input, blankline);
        sync_input_hist();
    }

    if (input->logfile && !nolog) save_to_log(input, str);
}

/* recall_input() parameter combinations:
 *
 * mode=0: step n times
 * mode=1: search n times
 * mode=2: go to end with sign(n)
 */

String *recall_input(int n, int mode)
{
    int i, stop, dir;
    String *str, *pat = NULL;
    CQueue *cq = &input->cq;

    if (cq->index == cq->last) hold_input(CS(keybuf));

    stop = (n < 0) ? cq->first : cq->last;
    if (cq->index == stop) return NULL;
    dir = (n < 0) ? -1 : 1;
    if (mode == 2) {
	i = stop;
    } else {
        i = nmod(cq->index + dir, cq->maxsize);
        pat = (mode==1) ? cq->data[cq->last] : NULL;
    }
    if (n < 0) n = -n;

    /* Search until we find a non-gagged match. */
#define match(s, p) (s->len > p->len && strncmp(s->data, p->data, p->len) == 0)
    while (1) {
	str = (String*)cq->data[i];
	if ((!(str->attrs & F_GAG) && (!pat || match(str, pat))))
	    if (!--n) break;
	if (i == stop) return NULL;
	i = nmod(i + dir, cq->maxsize);
    }
#undef match

    cq->index = i;
    return str;
}

struct Value *handle_recall_command(String *args, int offset)
{
    return newint(do_recall(args, offset));
}

int do_recall(String *args, int offset)
{
    int hist_start, n0, n1, i, j, want, numbers;
    int count = 0, mflag = matching, quiet = 0, truth = !0, jump = 256;
    long ival;
    int before = 0, after = 0, out_of_range = 0;
    int lastprinted, incontext;
    Value val[1];
    struct timeval tv0, tv1, *tvp0, *tvp1;
    const char *ptr;
    AUTO_BUFFER(recall_time_format);
    attr_t attrs = 0, tmpattrs;
    char opt;
    Pattern pat;
    World *world = xworld();
    History *hist = NULL;
    String *line;
    static List stack[1] = {{ NULL, NULL }};
    String *buffer = NULL;
    STATIC_STRING(startmsg, "================ Recall start ================",0);
    STATIC_STRING(endmsg,   "================= Recall end =================",0);
    STATIC_STRING(divider, "--", 0);
#if DEVELOPMENT
    int locality;
    String *nextline = NULL;
#endif

    init_pattern_str(&pat, NULL);
    startopt(CS(args), "ligw:a:f:t:m:vqA#B#C#");
    while ((opt = next_hist_opt(&ptr, &offset, &hist, &ival))) {
        switch (opt) {
        case 'a': case 'f':
            if (!parse_attrs(ptr, &tmpattrs, 0))
                goto do_recall_exit;
            attrs |= tmpattrs;
            break;
        case 't':
	    Stringcpy(recall_time_format, ptr);
            break;
        case 'm':
            if ((mflag = enum2int(ptr, 0, enum_match, "-m")) < 0)
                goto do_recall_exit;
            break;
        case 'v':
            truth = 0;
            break;
        case 'q':
            quiet = 1;
            break;
        case 'A':
	    after = ival;
            break;
        case 'B':
	    before = ival;
            break;
        case 'C':
	    before = after = ival;
            break;
        default: goto do_recall_exit;
        }
    }
    if (!hist) hist = world ? world->history : globalhist;
    ptr = args->data + offset;
#if DEVELOPMENT
    if ((locality = (ptr && *ptr == '?'))) ptr++;
#endif
    if ((numbers = (ptr && *ptr == '#'))) ptr++;
    while (is_space(*ptr)) ptr++;

    tvp0 = tvp1 = NULL;
    n0 = 0;
    n1 = hist->cq.total - 1;
    want = hist->cq.size;

    if (!ptr || !*ptr) {
        eprintf("missing arguments");
        goto do_recall_exit;

    } else if (*ptr == '-') {                                 /*  -y */
        ++ptr;
	if (!parsenumber(ptr, &ptr, TYPE_DTIME | TYPE_INT, val)) {
            eprintf("syntax error in recall range");
            goto do_recall_exit;
        }
        if (val->type & TYPE_DTIME) {
	    tvp1 = &tv1;
	    tv1 = val->u.tval;
	    if (val->type & TYPE_HMS)
		abstime(&tv1);
        } else /* if (val->type & TYPE_INT) */ {
            n0 = n1 = hist->cq.total - val->u.ival;
        }

    } else if (*ptr == '/') {                                 /*  /x */
        ++ptr;
        want = strtoint(ptr, &ptr);

    } else if (is_digit(*ptr)) {                              /* x... */
	if (!parsenumber(ptr, &ptr, TYPE_DTIME | TYPE_INT, val)) {
            eprintf("syntax error in recall range");
            goto do_recall_exit;
        }
        if (val->type & TYPE_DTIME) {
            tvp0 = &tv0;
	    tv0 = val->u.tval;
        } else /* if (val->type & TYPE_INT) */ {
            n0 = val->u.ival;
        }
        if (*ptr != '-') {                                    /* x   */
            if (val->type & TYPE_DTIME) {
                struct timeval now;
                gettime(&now);
                tvsub(&tv0, &now, &tv0);
            } else {
                n0 = hist->cq.total - n0;
            }
        } else if (is_digit(*++ptr)) {                        /* x-y */
            if (val->type & TYPE_INT) n0 = n0 - 1;
            else if (val->type & TYPE_HMS) abstime(&tv0);

	    if (!parsenumber(ptr, &ptr, TYPE_DTIME | TYPE_INT, val)) {
		eprintf("syntax error in recall range");
		goto do_recall_exit;
	    }
            if (val->type & TYPE_DTIME) {
		tvp1 = &tv1;
		tv1 = val->u.tval;
                if (val->type & TYPE_HMS)
                    abstime(&tv1);
            } else /* if (type & TYPE_INT) */ {
                n1 = val->u.ival - 1;
            }
        } else {                                               /* x-  */
            if (val->type & TYPE_INT) n0 = n0 - 1;
            else if (val->type & TYPE_HMS) abstime(&tv0);
        }
    }
    if (*ptr && !is_space(*ptr)) {
        eprintf("extra characters after recall range: %s", ptr);
        goto do_recall_exit;
    }
    while (is_space(*ptr)) ++ptr;
    if (*ptr && !init_pattern(&pat, ptr, mflag))
        goto do_recall_exit;

    if (hist->cq.size == 0)
        goto do_recall_exit;            /* (after parsing, before searching) */

    if (!quiet && tfout == tfscreen) {
        nohistory++;                    /* don't save this output in history */
        oputline(startmsg);
        oflush();			/* in case this takes a while */
    }

    hist_start = hist->cq.total - hist->cq.size;
    if (n0 < hist_start) n0 = hist_start;
    if (n1 >= hist->cq.total) n1 = hist->cq.total - 1;
    if (n0 <= n1 && (!tvp0 || !tvp1 || tvcmp(tvp0, tvp1) <= 0)) {
        attrs = ~attrs;

	lastprinted = n1 + 1;
	incontext = 0;
        if (hist == input) hold_input(CS(keybuf));
        for (i = n1; i >= hist_start; i--) {
	    if (i < n0 || want <= 0) {
		if (incontext) out_of_range = 1;
		else break;
	    }

            line = histline(hist, i);
            if (interrupted()) {
		(buffer = Stringnew(NULL, 32, 0))->links++;
		tftime(buffer, blankline, &line->time);
                eprintf("history scan interrupted at #%d, %S", i, buffer);
		Stringfree(buffer);
		buffer = NULL;
                break;
            }

            if (tvp1 && tvcmp(&line->time, tvp1) > 0) {
		/* globalhist isn't chronological, but we can optimize others */
		if (hist == globalhist || !jump)
		    continue;

		/* take large steps backward searching for something < tv1 */
		for (i -= jump; i >= n0; i -= jump) {
		    line = histline(hist, i);
		    if (tvcmp(&line->time, tvp1) <= 0)
			break;
		}
		i += jump;
		jump = 0;	/* don't do this again */
		continue;
	    }
            /* globalhist isn't chronological, but we can optimize others */
            if (tvp0 && tvcmp(&line->time, tvp0) < 0) {
		if (incontext) {
		    out_of_range = 1;
		} else {
		    if (hist == globalhist) continue;
		    break;
		}
	    }

            if (gag && (line->attrs & F_GAG & attrs)) continue;

            if (!out_of_range && !!patmatch(&pat, CS(line), NULL) == truth) {
		want--;
		j = i + after;
		if (j >= lastprinted - 1) {
		    j = lastprinted - 1;
		} else if ((before || after) && stack->head) {
		    inlist((void*)divider, stack, NULL);
		}
		incontext = before;
	    } else if (incontext) {
		incontext--;
		j = i;
	    } else {
		continue;
	    }

	    for ( ; j >= i; j--) {
		line = histline(hist, j);
		if (numbers) {
		    if (!buffer)
			buffer= Stringnew(NULL, line->len + 8, 0);
		    Sappendf(buffer, "%d: ", j+1);
		}
		if (recall_time_format->data) {
		    if (!buffer)
			buffer= Stringnew(NULL, line->len + 20, 0);
		    if (!*recall_time_format->data) {
			Stringadd(buffer, '[');
			tftime(buffer, time_format, &line->time);
			Stringadd(buffer, ']');
		    } else {
			tftime(buffer, CS(recall_time_format), &line->time);
		    }
		    Stringadd(buffer, ' ');
		}

#if DEVELOPMENT
		if (locality) {
		    char sign = '+';
		    long diff = (char*)nextline - (char*)line;
		    if (nextline > line) diff -= sizeof(String) + line->size;
		    else diff += nextline ? (sizeof(String)+nextline->size) : 0;
		    if (diff < 0) { sign = '-'; diff = -diff; }
		    if (!buffer)
			buffer = Stringnew(NULL, 40, 0);
		    Sprintf(buffer, "%d (%010p): %c%lx", j, line, sign, diff);
		    nextline = line;
		    line = buffer;
		    buffer = NULL;
		} else
#endif
		/* share line if possible: copy only if different */
		if (buffer) {
		    line = SStringcat(buffer, CS(line));
		    line->attrs &= attrs & F_ATTR;
		    buffer = NULL;
		} else if (line->attrs & ~attrs & F_ATTR) {
		    line = Stringnew(line->data, line->len, line->attrs & attrs);
		}

		inlist((void*)line, stack, NULL);
		lastprinted = j;
		count++;
	    }
        }
    }

    while (stack->head)
        oputline(CS((String *)unlist(stack->head, stack)));

    if (!quiet && tfout == tfscreen) {
        oputline(endmsg);
        nohistory--;
    }

do_recall_exit:
    free_pattern(&pat);
    Stringfree(recall_time_format);

    return count;
}

static int do_watch(const char *args, int id, int *wlines, int *wmatch)
{
    int out_of, match;

    if (!*args) {
        oprintf("%% %s %sabled.", special_var[id].val.name,
            getintvar(id) ? "en" : "dis");
        return 1;
    } else if (cstrcmp(args, "off") == 0) {
        set_var_by_id(id, 0);
        oprintf("%% %s disabled.", special_var[id].val.name);
        return 1;
    } else if (cstrcmp(args, "on") == 0) {
        /* do nothing */
    } else {
        if ((match = numarg(&args)) < 0) return 0;
        if ((out_of = numarg(&args)) < 0) return 0;
        *wmatch = match;
        *wlines = out_of;
    }
    set_var_by_id(id, 1);
    oprintf("%% %s enabled, searching for %d out of %d lines",
        special_var[id].val.name, *wmatch, *wlines);
    return 1;
}

struct Value *handle_watchdog_command(String *args, int offset)
{
    return newint(do_watch(args->data + offset, VAR_watchdog,
        &wdlines, &wdmatch));
}

struct Value *handle_watchname_command(String *args, int offset)
{
    return newint(do_watch(args->data + offset, VAR_watchname,
        &wnlines, &wnmatch));
}

int is_watchname(History *hist, String *line)
{
    int nmatches = 1, i;
    const char *old, *end;
    STATIC_BUFFER(buf);

    if (!watchname || !gag || line->attrs & F_GAG) return 0;
    if (is_space(*line->data)) return 0;
    for (end = line->data; *end && !is_space(*end); ++end);
    for (i = ((wnlines >= hist->cq.size) ? hist->cq.size - 1 : wnlines);
	i > 0; i--)
    {
        old = histline(hist, hist->cq.last - i)->data;
        if (strncmp(old, line->data, end - line->data) != 0) continue;
        if (++nmatches == wnmatch) break;
    }
    if (nmatches < wnmatch) return 0;
    Sprintf(buf, "{%.*s}*", end - line->data, line->data);
    oprintf("%% Watchname: gagging \"%S\"", buf);
    return add_new_macro(buf->data, "", NULL, NULL, "", gpri, 100, F_GAG,
	0, MATCH_GLOB);
}

int is_watchdog(History *hist, String *line)
{
    int nmatches = 0, i;
    const char *old;

    if (!watchdog || !gag || line->attrs & F_GAG) return 0;
    for (i = ((wdlines >= hist->cq.size) ? hist->cq.size - 1 : wdlines);
	i > 0; i--)
    {
        old = histline(hist, hist->cq.last - i)->data;
        if (cstrcmp(old, line->data) == 0 && (++nmatches == wdmatch)) return 1;
    }
    return 0;
}

String *history_sub(String *line)
{
    STATIC_BUFFER(pattern);
    STATIC_BUFFER(buffer);
    char *replacement, *loc = NULL;
    String *src = NULL;
    int i;

    pattern->data = line->data + 1;
    if (!(replacement = strchr(pattern->data, '^'))) return NULL;
    *replacement = '\0';
    pattern->len = replacement - pattern->data;
    for (i = 1; i < input->cq.size; i++) {
	src = histline(input, input->cq.last - i);
	loc = strstr(src->data, pattern->data);
	if (loc) break;
    }
    *(replacement++) = '^';
    if (!loc) return NULL;
    Stringtrunc(buffer, 0);
    SStringncat(buffer, CS(src), loc - src->data);
    SStringocat(buffer, CS(line), replacement - line->data);
    SStringocat(buffer, CS(src), loc - src->data + pattern->len);
    return buffer;
}

static void stoplog(World *world)
{
    if (world->history->logfile) tfclose(world->history->logfile);
    world->history->logfile = NULL;
}

static void listlog(World *world)
{
    if (world->history->logfile)
        oprintf("%% Logging world %s output to %s",
          world->name, world->history->logfile->name);
}

/* Parse "ligw:" history options.  If another option is found, it is returned,
 * so the caller can parse it.  If end of options is reached, 0 is returned.
 * '?' is returned for error.  *histp will contain a pointer to the history
 * selected by the "ligw:" options.  *histp will be unchanged if no relavant
 * options are given; the caller should assign a default before calling.
 */
static int next_hist_opt(const char **ptr, int *offsetp, History **histp,
    void *u)
{
    World *world;
    char c;
    const char *p;
    int selected = 0;

    if (!ptr) ptr = &p;
    while ((c = nextopt(ptr, u, NULL, offsetp))) {
        switch (c) {
        case 'l':
	    if (selected++) goto multiple_error;
            *histp = localhist;
            break;
        case 'i':
	    if (selected++) goto multiple_error;
            *histp = input;
            break;
        case 'g':
	    if (selected++) goto multiple_error;
            *histp = globalhist;
            break;
        case 'w':
	    if (selected++) goto multiple_error;
            if (!(world = named_or_current_world(*ptr)))
                return '?';
	    *histp = world->history;
            break;
        default:
            return c;                /* let caller handle it */
        }
    }
    return c;

multiple_error:
    eprintf("only one of the -ligw options may be used.");
    return '?';
}

struct Value *handle_recordline_command(String *args, int offset)
{
    History *history = globalhist;
    char opt;
    struct timeval tv, *tvp = NULL;
    conString *line = NULL;
    int attrflag = 0;
    attr_t attrs = 0, tmpattrs;
    const char *ptr;

    startopt(CS(args), "lgiw:t@a:p");
    while ((opt = next_hist_opt(&ptr, &offset, &history, &tv))) {
	switch (opt) {
        case 't': tvp = &tv; break;
        case 'p': attrflag = 1; break;
        case 'a':
            if (!parse_attrs(ptr, &tmpattrs, 0))
		return shareval(val_zero);
            attrs |= tmpattrs;
            break;
	default:
	    return shareval(val_zero);
	}
    }

    if (attrflag) {
	line = CS(decode_attr(CS(args), attrs, offset));
	/* if encoding was invalid, just copy without decoding */
    }
    if (!line) {
	line = CS(Stringodup(CS(args), offset));
	line->attrs = adj_attr(line->attrs, attrs);
    }
    line->links++;
    if (tvp)
	line->time = tv;
    nolog++;
    if (history == input)
	record_input(line);
    else
	recordline(history, line);
    nolog--;
    conStringfree(line);
    return shareval(val_one);
}

struct Value *handle_log_command(String *args, int offset)
{
    History *history;
    History dummy;
    TFILE *logfile = NULL;
    const char *name;

    if (restriction >= RESTRICT_FILE) {
        eprintf("restricted");
        return shareval(val_zero);
    }

    history = &dummy;
    startopt(CS(args), "lgiw:");
    if (next_hist_opt(NULL, &offset, &history, NULL))
        return shareval(val_zero);

    if (history == &dummy && !(args->len - offset)) {
	/* "/log" */
        if (log_count) {
            if (input->logfile)
                oprintf("%% Logging input to %s", input->logfile->name);
            if (localhist->logfile)
                oprintf("%% Logging local output to %s",
                    localhist->logfile->name);
            if (globalhist->logfile)
                oprintf("%% Logging global output to %s",
                    globalhist->logfile->name);
            mapworld(listlog);
        } else {
            oputs("% Logging disabled.");
        }
        return shareval(val_one);
    } else if (cstrcmp(args->data + offset, "OFF") == 0) {
	/* "/log [options] OFF" */
        if (history == &dummy) {
            if (log_count) {
                if (input->logfile) tfclose(input->logfile);
                input->logfile = NULL;
                if (localhist->logfile) tfclose(localhist->logfile);
                localhist->logfile = NULL;
                if (globalhist->logfile) tfclose(globalhist->logfile);
                globalhist->logfile = NULL;
                mapworld(stoplog);
                log_count = 0;
                update_status_field(NULL, STAT_LOGGING);
            }
        } else if (history->logfile) {
            tfclose(history->logfile);
            history->logfile = NULL;
            --log_count;
            update_status_field(NULL, STAT_LOGGING);
        }
        return shareval(val_one);
    } else if (cstrcmp(args->data+offset, "ON") == 0 || !(args->len - offset)) {
	/* "/log [options] [ON]" */
        if (!(name = tfname(NULL, "LOGFILE")))
            return shareval(val_zero);
        logfile = tfopen(name, "a");
    } else {
	/* "/log [options] <filename>" */
        name = expand_filename(args->data + offset);
        logfile = tfopen(name, "a");
    }
    if (!logfile) {
        operror(name);
        return shareval(val_zero);
    }
    if (history == &dummy) history = globalhist;
    if (history->logfile) {
        tfclose(history->logfile);
        history->logfile = NULL;
        log_count--;
    }
    do_hook(H_LOG, "%% Logging to file %s", "%s", logfile->name);
    history->logfile = logfile;
    log_count++;
    update_status_field(NULL, STAT_LOGGING);
    return shareval(val_one);
}

#define histname(hist) \
        (hist == globalhist ? "global" : (hist == localhist ? "local" : \
        (hist == input ? "input" : "world")))

struct Value *handle_histsize_command(String *args, int offset)
{
    History *hist;
    int maxsize = 0, size;
    const char *ptr;

    hist = globalhist;
    startopt(CS(args), "lgiw:");
    if (next_hist_opt(NULL, &offset, &hist, NULL))
        return shareval(val_zero);
    if (args->len - offset) {
        ptr = args->data + offset;
        if ((maxsize = numarg(&ptr)) <= 0) return shareval(val_zero);
	if (!resize_cqueue(&hist->cq, maxsize)) {
	    eprintf("not enough memory for %d lines.", maxsize);
	    maxsize = 0;
	}
	/* XXX resize corresponding screen */
    }
    size = hist->cq.maxsize ? hist->cq.maxsize : histsize;
    oprintf("%% %s history capacity %s %ld lines.",
        histname(hist), maxsize ? "changed to" : "is",
        size);
    hist->cq.index = hist->cq.last;
    return newint(size);
}

long hist_getsize(const struct History *hist)
{
    return (hist && hist->cq.maxsize) ? hist->cq.maxsize : histsize;
}

#endif /* NO_HISTORY */
