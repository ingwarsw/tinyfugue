/*************************************************************************
 *  TinyFugue - programmable mud client
 *  Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2002, 2003, 2004, 2005, 2006-2007 Ken Keys
 *
 *  TinyFugue (aka "tf") is protected under the terms of the GNU
 *  General Public License.  See the file "COPYING" for details.
 ************************************************************************/
static const char RCSid[] = "$Id: process.c,v 35004.71 2007/01/13 23:12:39 kkeys Exp $";

/************************
 * Fugue processes.     *
 ************************/

#include "tfconfig.h"
#include <sys/types.h>
#include "port.h"
#include "tf.h"
#include "util.h"
#include "pattern.h"	/* for tfio.h */
#include "search.h"	/* for tfio.h */
#include "tfio.h"
#include "history.h"
#include "world.h"
#include "process.h"
#include "socket.h"
#include "expand.h"
#include "cmdlist.h"
#include "output.h"	/* oflush() */
#include "signals.h"	/* interrupted() */

const int feature_process = !(NO_PROCESS - 0);
#if !NO_PROCESS

#define P_REPEAT     'R'
#define P_QFILE      '\''
#define P_QSHELL     '!'
#define P_QRECALL    '#'
#define P_QLOCAL     '`'

#define PROC_DEAD     0
#define PROC_RUNNING  1

conString enum_disp[] = {
    STRING_LITERAL("echo"), STRING_LITERAL("send"), STRING_LITERAL("exec"),
    STRING_NULL };
enum { DISP_ECHO, DISP_SEND, DISP_EXEC };

#define PTIME_VAR	-1
#define PTIME_SYNC	-2
#define PTIME_PROMPT	-3

typedef struct Proc {
    int pid;
    char type;
    char state;
    int disp;           	/* disposition: what to do w/ generated text */
    int count;
    int (*func)(struct Proc *proc);
    struct timeval ptime;	/* delay */
    struct timeval timer;	/* time of next execution */
    char *pre;			/* prefix string */
    char *suf;			/* suffix string */
    TFILE *input;		/* source of quote input */
    struct World *world;	/* where to send output */
    conString *cmd;		/* command or file name */
    Stringp buffer;		/* buffer for prefix+cmd+suffix */
    struct Program *prog;	/* compiled repeat command */
    struct Proc *next, *prev;
} Proc;

static struct Value *killproc(Proc *proc, int needresult);

static void format_approx_interval(char *buf, struct timeval *tv);
static void nukeproc(Proc *proc);
static int  runproc(Proc *proc);
static int  do_repeat(Proc *proc);
static int  do_quote(Proc *proc);
static void strip_escapes(char *src);

struct timeval proctime = { 0, 0 };	/* when next process should be run */

static int runall_depth = 0;
static Proc *proclist = NULL;		/* head of process list */
static Proc *proctail = NULL;		/* tail of process list */


static void format_approx_interval(char *buf, struct timeval *tvp)
{
    if (tvp->tv_sec >= 60*60*100)
        sprintf(buf, "%6ld h", (long)tvp->tv_sec/3600);
    else if (tvp->tv_sec >= 60)
        sprintf(buf, "%2ld:%02ld:%02ld", (long)tvp->tv_sec/3600,
            (long)(tvp->tv_sec/60) % 60, (long)tvp->tv_sec % 60);
    else
        sprintf(buf, "%4ld.%03ld",
            (long)tvp->tv_sec, (long)tvp->tv_usec / 1000);
}

struct Value *handle_ps_command(String *args, int offset)
{
    Proc *p;
    char obuf[18], nbuf[10];
    const char *ptr;
    struct timeval now, next;
    int opt, pid = 0, shortflag = FALSE, repeatflag = FALSE, quoteflag = FALSE;
    struct World *world = NULL;

    startopt(CS(args), "srqw:");
    while ((opt = nextopt(&ptr, NULL, NULL, &offset))) {
        switch(opt) {
        case 's': shortflag = TRUE; break;
        case 'r': repeatflag = TRUE; break;
        case 'q': quoteflag = TRUE; break;
        case 'w':
            if (!(world = named_or_current_world(ptr)))
                return shareval(val_zero);
            break;
        default:  return shareval(val_zero);
        }
    }

    ptr = args->data + offset;
    if (*ptr)
        pid = numarg(&ptr);

    gettime(&now);
    if (!repeatflag && !quoteflag)
        repeatflag = quoteflag = TRUE;
    if (!shortflag)
        oprintf("  PID     NEXT T D WORLD       PTIME COUNT COMMAND");

    for (p = proclist; p; p = p->next) {
        if (p->state == PROC_DEAD) continue;
        if (!repeatflag && p->type == P_REPEAT) continue;
        if (!quoteflag && p->type != P_REPEAT) continue;
        if (world && p->world != world) continue;
        if (pid && pid != p->pid) continue;

        if (shortflag) {
            oprintf("%d", p->pid);
            continue;
        }

        if (p->ptime.tv_sec == PTIME_SYNC) {
            strcpy(nbuf, "       S");
        } else if (lpquote || p->ptime.tv_sec == PTIME_PROMPT) {
            strcpy(nbuf, "       P");
        } else {
            tvsub(&next, &p->timer, &now);
            if (next.tv_sec < 0 || next.tv_usec < 0)
                sprintf(nbuf, "%8s", "pending");
            else if (next.tv_sec >= 0)
                format_approx_interval(nbuf, &next);
        }
        sprintf(obuf, "%-8.8s ", p->world ? p->world->name : "");
        if (p->ptime.tv_sec == PTIME_SYNC) {
            strcpy(obuf+9, "       S");
        } else if (p->ptime.tv_sec == PTIME_PROMPT) {
            strcpy(obuf+9, "       P");
        } else if (p->ptime.tv_sec < 0) {
            strcpy(obuf+9, "        ");
        } else {
            format_approx_interval(obuf+9, &p->ptime);
        }
        if (p->type == P_REPEAT) {
            char cbuf[32];
            if (p->count < 0)
                sprintf(cbuf, "%5s", "i");
            else
                sprintf(cbuf, "%5d", p->count);
            oprintf("%5d %s r   %s %5s %S",
                p->pid, nbuf, obuf, cbuf, p->cmd);
        } else {
	    char disp;
	    switch (p->disp) {
	    case DISP_ECHO: disp = 'e'; break;
	    case DISP_SEND: disp = 's'; break;
	    case DISP_EXEC: disp = 'x'; break;
	    }
            oprintf("%5d %s q %c %s       %s%c\"%S\"%s",
                p->pid, nbuf, disp, obuf, p->pre, p->type,
                p->cmd, p->suf);
        }
    }
    return shareval(val_one);
}

static struct Value *newproc(int type, int (*func)(Proc *proc), int count,
    const char *pre, const char *suf, TFILE *input, struct World *world,
    String *cmd, struct timeval *ptime, int disp, int delay)
{
    Proc *proc;
    static int hipid = 0;

    cmd->links++;

    if (!(proc = (Proc *) MALLOC(sizeof(Proc)))) {
        eprintf("not enough memory for new process");
        Stringfree(cmd);
        return shareval(val_zero);
    }

    if (type == P_REPEAT) {
	if (!(proc->prog = compile_tf(CS(cmd), 0, SUB_MACRO, 0, 0))) {
	    Stringfree(cmd);
	    return shareval(val_zero);
	}
    } else {
	proc->prog = NULL;
    }

    proc->disp = disp;
    proc->count = count;
    proc->func = func;
    proc->type = type;
    proc->ptime = *ptime;

    if (ptime->tv_sec == PTIME_VAR) {
        gettime(&proc->timer);
	if (delay) tvadd(&proc->timer, &proc->timer, &process_time);
    } else if (ptime->tv_sec >= 0) {
        gettime(&proc->timer);
        if (delay) tvadd(&proc->timer, &proc->timer, &proc->ptime);
    }

    proc->pre = STRDUP(pre);
    proc->suf = suf ? STRDUP(suf) : NULL;
    proc->cmd = CS(cmd);  /* already did links++ */
    proc->pid = ++hipid;
    proc->input = input;
    proc->world = world;
    Stringzero(proc->buffer);

    *(proctail ? &proctail->next : &proclist) = proc;
    proc->prev = proctail;
    proc->next = NULL;
    proctail = proc;

    proc->state = PROC_RUNNING;
    do_hook(H_PROCESS, NULL, "%d", proc->pid);
    if (ptime->tv_sec == PTIME_SYNC) {  /* synch */
        oflush();  /* flush now, process might take a while */
        while (runproc(proc)) {
	    if (interrupted()) {
		eprintf("synchronous process interrupted.");
		break;
	    }
	}
        return killproc(proc, 1);  /* no nuke! */
    }
    if (lpquote || proc->ptime.tv_sec == PTIME_PROMPT) {
	runproc(proc);	/* XXX should do this asynchronously, in main loop */
    } else if (tvcmp(&proctime, &tvzero) == 0 ||
	tvcmp(&proc->timer, &proctime) < 0)
    {
        proctime = proc->timer;
    }
    return newint(proc->pid);
}

static struct Value *killproc(Proc *proc, int needresult)
{
    int result = 1;

    proc->state = PROC_DEAD;
    do_hook(H_KILL, NULL, "%d", proc->pid);

    if (proc->type == P_QSHELL) readers_clear(fileno(proc->input->u.fp));
    if (proc->input) result = tfclose(proc->input);

    if (proc->type == P_QFILE) result = result + 1;

    if (!needresult) return NULL;
    if (proc->type == P_QLOCAL || proc->type == P_REPEAT) return_user_result();
    else return newint(result);
}

static void nukeproc(Proc *proc)
{
    *(proc->next ? &proc->next->prev : &proctail) = proc->prev;
    *(proc->prev ? &proc->prev->next : &proclist) = proc->next;

    if (proc->prog)
	prog_free(proc->prog);
    FREE(proc->pre);
    conStringfree(proc->cmd);
    if (proc->suf) FREE(proc->suf);
    Stringfree(proc->buffer);
    FREE(proc);
}

void nuke_dead_procs(void)
{
    Proc *proc, *next;

    for (proc = proclist; proc; proc = next) {
        next = proc->next;
        if (proc->state == PROC_DEAD)
            nukeproc(proc);
    }
}

void kill_procs(void)
{
    while (proclist) {
        if (proclist->state != PROC_DEAD) {
            if (proclist->type == P_QSHELL)
                readers_clear(fileno(proclist->input->u.fp));
            if (proclist->input)
                tfclose(proclist->input);
        }
        nukeproc(proclist);
    }

    proctime = tvzero;
}

void kill_procs_by_world(struct World *world)
{
    Proc *proc;

    for (proc = proclist; proc; proc = proc->next) {
        if (proc->world == world) killproc(proc, 0);
    }
}

struct Value *handle_kill_command(String *args, int offset)
{
    Proc *proc;
    int pid, error = 0;
    const char *ptr = args->data + offset;

    while (*ptr) {
        if ((pid = numarg(&ptr)) < 0) return shareval(val_zero);
        for (proc = proclist; proc && (proc->pid != pid); proc=proc->next);
        if (!proc || proc->state == PROC_DEAD) {
            eprintf("no process %d", pid);
            error++;
        } else {
            killproc(proc, 0);
        }
    }
    return newint(!error);
}

int ch_lpquote(Var *var)
{
    runall(lpquote, NULL);
    return 1;
}

/* Run all processes that should be run.
 * If prompted, run promptable procs;
 * if !prompted, run timed procs;
 * and, always run procs that were already marked runnable pending a shell
 * line if that shell line has become available.
 * If !prompted, set proctime to the time of the next earliest process.
 */
void runall(int prompted, World *world)
{
    Proc *proc;
    struct timeval now;
    int resched;	/* consider this process in proctime calculation? */
    int promptable;	/* proc should be run iff prompted */

    gettime(&now);
    runall_depth++;
    if (!prompted)
	proctime = tvzero;
    for (proc = proclist; proc; proc = proc->next) {
        if (proc->state == PROC_DEAD) continue;
	promptable = (lpquote || proc->ptime.tv_sec == PTIME_PROMPT);
        if (proc->type == P_QSHELL) {
            if (is_active(fileno(proc->input->u.fp))) {
                if (!(resched = runproc(proc)))
                    killproc(proc, 0);  /* no nuke! */
	    } else if (promptable != prompted) {
		continue;
	    } else if (prompted && proc->world && world && proc->world != world)
	    {
		/* prompt wasn't from the right world */
		continue;
            } else if (promptable || tvcmp(&proc->timer, &now) <= 0) {
                resched = FALSE;
                readers_set(fileno(proc->input->u.fp));
            } else {
		resched = TRUE;
	    }
	} else if (promptable != prompted) {
	    continue;
	} else if (promptable || tvcmp(&proc->timer, &now) <= 0) {
            if (!(resched = runproc(proc)))
                killproc(proc, 0);  /* no nuke! */
        } else resched = TRUE;

        if (resched && !prompted &&
           (tvcmp(&proctime,&tvzero) == 0 || tvcmp(&proc->timer,&proctime) < 0))
        {
            proctime = proc->timer;
        }
    }
    runall_depth--;
}

static int runproc(Proc *p)
{
    int done;
    struct Sock *oldsock;
    struct timeval now;

    oldsock = xsock;
    if (p->world) xsock = p->world->sock;
    done = !(*p->func)(p);
    xsock = oldsock;
    if (!done && p->ptime.tv_sec >= PTIME_VAR) {   /* timed */
        tvadd(&p->timer, &p->timer,
            (p->ptime.tv_sec < 0) ? &process_time : &p->ptime);
        gettime(&now);
        if (tvcmp(&p->timer, &now) < 0) {
            /* We missed 2 or more appointments, presumably because tf was
             * suspended or blocked.  To prevent multiple execution,
             * schedule based on now instead of previous schedule.
             */
            p->timer = now;
            tvadd(&p->timer, &p->timer,
                (p->ptime.tv_sec < 0) ? &process_time : &p->ptime);
        }
    }
    return !done;
}

/* do_repeat
 * Returns 0 if proc is done, nonzero otherwise.
 */
static int do_repeat(Proc *proc)
{
    prog_run(proc->prog, NULL, 0, "\bREPEAT", 0);
    if (proc->count > 0) --proc->count;
    return proc->count;
}

/* do_quote
 * Returns 0 if proc is done, nonzero otherwise.
 */
static int do_quote(Proc *proc)
{
    STATIC_BUFFER(line);
    String *buffer;

    (buffer = Stringnew(NULL, -1, 0))->links++;
    if (proc->pre && *proc->pre) {
	if (!tfgetS(line, proc->input)) {
	    Stringfree(buffer);
	    return 0;
	}
	Stringcpy(buffer, proc->pre);
	SStringcat(buffer, CS(line));
    } else {
	if (!tfgetS(buffer, proc->input)) {
	    Stringfree(buffer);
	    return 0;
	}
    }
    if (proc->suf) Stringcat(buffer, proc->suf);
    if (proc->type == P_QSHELL) readers_clear(fileno(proc->input->u.fp));
    if (qecho)
	tfprintf(tferr, "%S%S%A", qprefix, buffer, getattrvar(VAR_qecho_attr));
    switch (proc->disp) {
    case DISP_ECHO:
        oputline(CS(buffer));
        break;
    case DISP_SEND:
        macro_run(CS(buffer), 0, NULL, 0, SUB_LITERAL, "\bQUOTE");
        break;
    case DISP_EXEC:
        macro_run(CS(buffer), 0, NULL, 0, SUB_KEYWORD, "\bQUOTE");
        break;
    }
    Stringfree(buffer);
    return TRUE;
}

static void strip_escapes(char *src)
{
    char *dest;

    if (!*src) return;
    for (dest = src; *src; *dest++ = *src++) {
        if (*src == '\\') src++;
    }
    *dest = '\0';
}

static int procopt(const char *opts, String *args, int *offsetp,
    struct timeval *ptime, struct World **world, int *disp, int *subflag,
    int *delay)
{
    char opt;
    const char *ptr;
    ValueUnion uval;

    *world = NULL;
    ptime->tv_sec = PTIME_VAR;
    startopt(CS(args), opts);
    while ((opt = nextopt(&ptr, &uval, NULL, offsetp))) {
        switch(opt) {
        case 'w':
            if (!(*world = named_or_current_world(ptr)))
                return FALSE;
            break;
        case 's':
            if ((*subflag = enum2int(ptr, 0, enum_sub, "-s")) < 0)
                return FALSE;
            break;
        case 'S':
            ptime->tv_sec = PTIME_SYNC;
            break;
        case 'P':
            ptime->tv_sec = PTIME_PROMPT;
            break;
        case 'd':
            if ((*disp = enum2int(ptr, 0, enum_disp, "-d")) < 0)
                return FALSE;
            break;
        case 'n':
            *delay = FALSE;
            break;
        case '-':
            *ptime = uval.tval;
            break;
        default:  return FALSE;
        }
    }
    return TRUE;
}

struct Value *handle_quote_command(String *args, int offset)
{
    char *ptr, *pre, *cmd, *suf = NULL;
    STATIC_BUFFER(newcmd);
    TFILE *input, *oldout, *olderr;
    int type, result;
    struct timeval ptime;
    int disp = -1, subflag = SUB_MACRO;
    struct World *world;

    if (!procopt("-@PSw:d:s:", args, &offset, &ptime, &world, &disp, &subflag,
	NULL))
	    return shareval(val_zero);

    ptr = args->data + offset;
    pre = ptr;
    while (*ptr != '\'' && *ptr != '!' && *ptr != '#' && *ptr != '`') {
        if (*ptr == '\\') ptr++;
        if (!*ptr) {
            eprintf("missing command character");
            return shareval(val_zero);
        }
        ptr++;
    }
    type = *ptr;
    *ptr = '\0';
    strip_escapes(pre);
    if (*++ptr == '"') {
        cmd = ++ptr;
        if ((ptr = estrchr(ptr, '"', '\\'))) {
            *ptr = '\0';
            suf = ptr + 1;
            strip_escapes(suf);
        }
        strip_escapes(cmd);
    } else {
        cmd = ptr;
    }

    switch (type) {
    case P_QFILE:
        if (restriction >= RESTRICT_FILE) {
            eprintf("files restricted");
            return shareval(val_zero);
        }
        cmd = expand_filename(cmd);
        if ((input = tfopen(cmd, "r")) == NULL) {
            operror(cmd);
            return shareval(val_zero);
        }
        break;
    case P_QSHELL:
        /* null input, and capture stderr */
#ifdef PLATFORM_UNIX
        Sprintf(newcmd, "{ %s; } </dev/null 2>&1", cmd);
#endif
#ifdef PLATFORM_OS2
        Sprintf(newcmd, "( %s ) <nul 2>&1", cmd);
#endif
	/* RESTRICT_SHELL is checked by tfopen() */
        if ((input = tfopen(newcmd->data, "p")) == NULL) {
            operror(cmd);
            return shareval(val_zero);
        }
        break;
#if !NO_HISTORY
    case P_QRECALL:
        oldout = tfout;
        olderr = tferr;
        tfout = input = tfopen(NULL, "q");
        /* tferr = input; */
        result = do_recall(args, cmd - args->data);
        tferr = olderr;
        tfout = oldout;
        if (!result) {
            tfclose(input);
            return shareval(val_zero);
        }
        break;
#endif
    case P_QLOCAL:
        oldout = tfout;
        olderr = tferr;
        tfout = input = tfopen(NULL, "q");
        /* tferr = input; */
        macro_run(CS(args), cmd - args->data, NULL, 0, subflag, "\bQUOTE");
        tferr = olderr;
        tfout = oldout;
        break;
    default:    /* impossible */
        return shareval(val_zero);
    }
    return newproc(type, do_quote, -1, pre, suf, input, world,
        Stringnew(cmd, -1, 0), &ptime,
	(disp >= 0) ? disp : (*pre ? DISP_EXEC : DISP_SEND),
	TRUE);
}

struct Value *handle_repeat_command(String *args, int offset)
{
    int count, delay = TRUE;
    struct timeval ptime;
    struct World *world;
    const char *ptr;

    if (!procopt("-@PSw:n", args, &offset, &ptime, &world, NULL, NULL, &delay))
        return shareval(val_zero);
    ptr = args->data + offset;
    if (tolower(*ptr) == 'i') {
	if (ptime.tv_sec == PTIME_SYNC) {
	    eprintf("-S i would cause an infinite busy loop.");
	    return shareval(val_zero);
	}
        count = -1;
	ptr++;
    } else if (is_digit(*ptr)) {
	/* If we used numarg(), we couldn't tell "3x" from "3 x" */
        count = strtoint(ptr, &ptr);
	if (count <= 0) {
	    eprintf("invalid repeat count (%d)", count);
	    return shareval(val_zero);
	}
    } else {
	eprintf("invalid repeat count (%.5s)", ptr);
	return shareval(val_zero);
    }
    if (*ptr && !is_space(*ptr)) {
	eprintf("repeat count followed by garbage (%.5s)", ptr);
        return shareval(val_zero);
    }
    while(is_space(*ptr)) ptr++;
    if (!*ptr) {
	eprintf("missing command");
        return shareval(val_zero);
    }
    return newproc(P_REPEAT, do_repeat, count, "", "", NULL, world,
        Stringnew(ptr, -1, 0), &ptime, DISP_ECHO, delay);
}

#endif /* NO_PROCESS */
