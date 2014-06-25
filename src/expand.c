/*************************************************************************
 *  TinyFugue - programmable mud client
 *  Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2002, 2003, 2004, 2005, 2006-2007 Ken Keys
 *
 *  TinyFugue (aka "tf") is protected under the terms of the GNU
 *  General Public License.  See the file "COPYING" for details.
 ************************************************************************/
static const char RCSid[] = "$Id: expand.c,v 35004.232 2007/01/13 23:12:39 kkeys Exp $";


/********************************************************************
 * Fugue macro text interpreter
 *
 * Written by Ken Keys
 * Interprets macro statements, performing substitutions for positional
 * parameters, variables, macro bodies, and expressions.
 ********************************************************************/

#include "tfconfig.h"
#include <math.h>
#include "port.h"
#include "tf.h"
#include "util.h"
#include "pattern.h"
#include "search.h"
#include "tfio.h"
#include "macro.h"
#include "signals.h"	/* interrupted() */
#include "socket.h"	/* send_line() */
#include "keyboard.h"	/* pending_line, handle_input_line() */
#include "parse.h"
#include "expand.h"
#include "expr.h"
#include "cmdlist.h"
#include "command.h"
#include "variable.h"

typedef struct { void *ptr; opcode_t op; } opcmd_t;

Value *user_result = NULL;		/* result of last user command */
int recur_count = 0;			/* expansion nesting count */
const char *current_command = NULL;
const conString *argstring = NULL;	/* command argument string */
Arg *tf_argv = NULL;			/* shifted command argument vector */
int tf_argc = 0;			/* shifted command/function arg count */
int argtop = 0;				/* top of function argument stack */
keyword_id_t block = 0;			/* type of current block */
Value *val_zero = NULL;
Value *val_one = NULL;
Value *val_blank = NULL;

static const char *oplabel_table[256];
static int cmdsub_count = 0;		/* cmdsub nesting count */


#define KEYWORD_LENGTH	6	/* length of longest keyword */
static const char *keyword_table[] = {
    "BREAK", "DO", "DONE", "ELSE", "ELSEIF", "ENDIF", "EXIT", "IF",
    "LET", "RESULT", "RETURN", "SET", "SETENV", "TEST", "THEN", "WHILE"
};


/* A JUMP with a negative arg is a placeholder:  ENDIF_PLACEHOLDER is for
 * jumping to ENDIF, all other negative values jump to the DONE of the
 * (-arg)th enclosing WHILE loop.  Any negative args left at runtime must
 * be because of a BREAK outside of a WHILE loop, so they will jump to
 * end-of-program.
 */
#define ENDIF_PLACEHOLDER -999999

static keyword_id_t keyword_parse(Program *prog);
static int list(Program *prog, int subs);
static int statement(Program *prog, int subs);
static int slashsub(Program *prog, String *dest);
static int backsub(Program *prog, String *dest);
static const char *error_text(Program *prog);
static int macsub(Program *prog, int in_expr);
static int cmdsub(Program *prog, int in_expr);
static int percentsub(Program *prog, int subs, String **destp);
static int expand(Program *prog, String *dest, int subs, opcmd_t *opcmdp);

#define is_end_of_statement(p) ((p)[0] == '%' && is_statend((p)[1]))
#define is_end_of_cmdsub(p) (cmdsub_count && *(p) == ')')

void init_expand(void)
{
    val_zero = newint(0);
    val_one = newint(1);
    val_blank = newSstr(blankline);

#define defopcode(name, num, optype, argtype, flag) \
    oplabel_table[(num)|(OPF_##flag)] = #name;
#include "opcodes.h"
}

static void prog_free_tail(Program *prog, int start)
{
    int i;

    for (i = start; i < prog->len; i++) {
	switch (op_arg_type(prog->code[i].op)) {
	case OPA_STRP:
	    if (prog->code[i].arg.str)
		conStringfree(prog->code[i].arg.str);
	    break;
	case OPA_VALP:
	    if (prog->code[i].arg.val)
		freeval(prog->code[i].arg.val);
	    break;
	}
    }
    prog->len = start;
}

void prog_free(Program *prog)
{
    prog_free_tail(prog, 0);
    if (prog->code) FREE(prog->code);
    conStringfree(prog->src);
    FREE(prog);
}

const char *oplabel(opcode_t op)
{
    static char buf[2] = "?";
    if (op >= 0x20 && op < 0x7F) {
	buf[0] = op;
	return buf;
    }
    return oplabel_table[op & OPLABEL_MASK];
}

static void inst_dump(const Program *prog, int i, char prefix)
{
    String *buf;
    opcode_t op;
    Value *val;
    conString *constr;
    BuiltinCmd *cmd;

    (buf = Stringnew(NULL, 0, 0))->links++;
    Sprintf(buf, "%c%5d: ", prefix, i);
    op = prog->code[i].op;
    Sappendf(buf, "%04X ", op);
    if (!oplabel(op)) {
	Sappendf(buf, "%8s ", "");
    } else {
	Sappendf(buf, "%-8s ", oplabel(op));
    }
    switch(op_arg_type(op)) {
    case OPA_INT:
	Sappendf(buf, "%d", prog->code[i].arg.i);
	break;
    case OPA_CHAR:
	Sappendf(buf, "%c", prog->code[i].arg.c);
	break;
    case OPA_STRP:
	constr = prog->code[i].arg.str;
	Sappendf(buf, constr ? "\"%S\"" : "NULL", constr);
	break;
    case OPA_CMDP:
	cmd = prog->code[i].arg.cmd;
	Sappendf(buf, "%s", cmd->name);
	break;
    case OPA_VALP:
	if (!(val = prog->code[i].arg.val)) {
	    Stringcat(buf, "NULL");
	    break;
	}
	switch (val->type & TYPES_BASIC) {
	case TYPE_ID:      Sappendf(buf, "ID %s", val->name); break;
	case TYPE_FUNC:    Sappendf(buf, "FUNC %s", val->name); break;
	case TYPE_CMD:     Sappendf(buf, "CMD %s", val->name); break;
	case TYPE_STR:     Sappendf(buf, "STR \"%S\"", valstr(val));
			   if (val->type & TYPE_REGEX)
			       Stringcat(buf, " (RE)");
			   if (val->type & TYPE_EXPR)
			       Stringcat(buf, " (EXPR)");
			   if (val->type & TYPE_ATTR)
			       Stringcat(buf, " (ATTR)");
			   break;
	case TYPE_ENUM:    Sappendf(buf, "ENUM \"%S\"", valstr(val)); break;
	case TYPE_POS:     Sappendf(buf, "POS %S", valstr(val)); break;
	case TYPE_INT:     Sappendf(buf, "INT %S", valstr(val)); break;
	case TYPE_DECIMAL: Sappendf(buf, "DECIMAL %S", valstr(val)); break;
	case TYPE_DTIME:   Sappendf(buf, "DTIME %S", valstr(val)); break;
	case TYPE_ATIME:   Sappendf(buf, "ATIME %S", valstr(val)); break;
	case TYPE_FLOAT:   Sappendf(buf, "FLOAT %S", valstr(val)); break;
	default:           Sappendf(buf, "? %S", valstr(val)); break;
	}
	break;
    }
    buf->attrs = getattrvar(prefix == 'c' ? VAR_cecho_attr : VAR_iecho_attr);
    tfputline(CS(buf), tferr);
    Stringfree(buf);
}

static void prog_dump(Program *prog)
{
    int i;

    for (i = 0; i < prog->len; i++)
	inst_dump(prog, i, 'c');
}


struct Value *handle_eval_command(String *args, int offset)
{
    int c, subflag = SUB_MACRO;
    const char *ptr;

    startopt(CS(args), "s:");
    while ((c = nextopt(&ptr, NULL, NULL, &offset))) {
        switch (c) {
        case 's':
            if ((subflag = enum2int(ptr, 0, enum_sub, "-s")) < 0)
                return shareval(val_zero);
            break;
        default:
            return shareval(val_zero);
        }
    }
    if (!macro_run(CS(args), offset, NULL, 0, subflag, "\bEVAL"))
        return shareval(val_zero);
    return_user_result();
}

/* A command with the same name as a builtin was called; execute the macro if
 * there is one and builtin was not explicitly required, otherwise execute
 * the builtin. */
static int execute_command(BuiltinCmd *cmd, String *args, int offset,
    int builtin_only)
{
    if (!builtin_only && cmd->macro) {
	current_command = cmd->name;
	return do_macro(cmd->macro, args, offset, USED_NAME, 0);
    } else if (cmd->func) {
	current_command = cmd->name;
	set_user_result((*cmd->func)(args, offset));
	return 1;
    }
    /* eprefix depends on current_command still referring to caller */
    do_hook(H_NOMACRO, "!/%s is disabled in this copy of tf", "%s", cmd->name);
    return 0;
}

/* A command with a name not matching a builtin was called.  Call the macro. */
static int execute_macro(const char *name, unsigned int hash, String *args,
    int offset)
{
    Macro *macro;
    if (!(macro = find_hashed_macro(name, hash))) {
        do_hook(H_NOMACRO, "!%s: no such command or macro", "%s", name);
        return 0;
    }
    current_command = name;
    return do_macro(macro, args, offset, USED_NAME, 0);
}

static String *execute_start(const conString *args, const char **old_cmdp)
{
    String *str;

    (str = Stringdup(args))->links++;
    *old_cmdp = current_command;
    return str;
}

static int execute_end(const char *old_command, int truth, String *args)
{
    int error = 0;

    if (pending_line && !read_depth) {  /* "/dokey newline" and not in read() */
	current_command = NULL;
        error += !handle_input_line();
    }
    current_command = old_command;
    if (!truth) {
        truth = !valbool(user_result);
        set_user_result(newint(truth));
    }
    Stringfree(args);
    return !error;
}

/* handle_command
 * Execute a single command line that has already been expanded.
 * orig_cmd_line will not be written into.
 */
int handle_command(const conString *orig_cmd_line)
{
    String *cmd_line;
    const char *old_command, *name;
    BuiltinCmd *cmd = NULL;
    int error = 0, truth = 1;
    int offset, end;

    offset = 0;
    if (!orig_cmd_line->data[offset] || is_space(orig_cmd_line->data[offset]))
        return 0;
    cmd_line = execute_start(orig_cmd_line, &old_command);
    name = cmd_line->data + offset;
    while (cmd_line->data[offset] && !is_space(cmd_line->data[offset]))
        offset++;
    if (cmd_line->data[offset]) cmd_line->data[offset++] = '\0';
    while (is_space(cmd_line->data[offset]))
        offset++;
    for (end = cmd_line->len - 1; is_space(cmd_line->data[end]); end--);
    Stringtrunc(cmd_line, end+1);
    while (*name == '!') {
        truth = !truth;
        name++;
    }
    if (*name == '@') {
	name++;
        if (!(cmd = find_builtin_cmd(name))) {
            eprintf("%s: not a builtin command", name);
            error++;
        } else {
	    error += !execute_command(cmd, cmd_line, offset, TRUE);
	}
    } else {
        if ((cmd = find_builtin_cmd(name))) {
	    error += !execute_command(cmd, cmd_line, offset, FALSE);
        } else {
	    error += !execute_macro(name, macro_hash(name), cmd_line, offset);
	}
    }

    error += !execute_end(old_command, truth, cmd_line);
    return !error;
}

/* stringvec
 * Allocates *<vector> and fills it with locations of start and end of each
 * word in <str>, starting at <offset>.  <vecsize> is a guess of the number of
 * words there will be, or -1.  Returns number of words found, or -1 for error.
 * Freeing *<vector> is the caller's responsibility.
 */
static int stringvec(const String *str, int offset, Arg **vector, int vecsize)
{
    int count = 0;
    char *start, *next;
    const char *end;

    if (vecsize <= 0) vecsize = 10;
    if (!(*vector = (Arg *)MALLOC(vecsize * sizeof(Arg)))) {
	eprintf("Not enough memory for %d word vector", vecsize);
	return -1;
    }

    for (next = str->data + offset; *next; count++) {
	if (count == vecsize) {
	    *vector = (Arg*)XREALLOC((char*)*vector, sizeof(Arg)*(vecsize+=10));
	    if (!*vector) {
		FREE(*vector);
		*vector = NULL;
		eprintf("Not enough memory for %d word vector", vecsize);
		return -1;
	    }
	}
	start = stringarg(&next, &end);
	(*vector)[count].start = start - str->data;
	(*vector)[count].end = end - str->data;
    }

    return count;
}

Program *compile_tf(conString *src, int srcstart, int subs, int is_expr,
    int optimize)
{
    Program *prog;
    prog = MALLOC(sizeof(Program));
    if (!prog) {
	eprintf("out of memory");
	return NULL;
    }
    prog->len = 0;
    prog->size = 0;
    prog->code = NULL;
    (prog->src = src)->links++;
    prog->srcstart = srcstart;
    prog->mark = src->data + srcstart;
    prog->optimize = optimize_user ? optimize : 0;
    ip = src->data + srcstart;
    if (is_expr) {
	if (expr(prog)) {
	    if (!*ip) {
		if (cecho > invis_flag) prog_dump(prog);
		return prog;
	    }
	    parse_error(prog, "expression", "end of expression");
	}
    } else {
	if (list(prog, subs)) {
	    if (!*ip) {
		if (cecho > invis_flag) prog_dump(prog);
		return prog;
	    }
	    parse_error(prog, "macro", "end of statement");
	}
    }
    prog_free(prog);
    return NULL;
}

int macro_run(conString *body, int bodystart, String *args, int offset,
    int subs, const char *name)
{
    Program *prog;
    int result;

    if (!(prog = compile_tf(body, bodystart, subs, 0, 0))) return 0;
    result = prog_run(prog, args, offset, name, 0);
    prog_free(prog);
    return result;
}

static int do_parmsub(String *dest, int first, int last, int *emptyp)
{
    int from_stack = argtop, to_stack = !dest, result = 1;
    int orig_len, i;
    Value *val = NULL;

    if (first < 0 || last < first || last >= tf_argc) {
	*emptyp = 1;
    } else if (from_stack) {
	if (to_stack && first == last) {
	    (val = stack[argtop - tf_argc + first])->count++;
	    *emptyp = (val->type & TYPE_STR) && val->sval->len <= 0;
	} else {
	    if (to_stack)
		(dest = Stringnew(NULL, 0, 0))->links++;
	    orig_len = dest->len;
	    for (i = first; ; i++) {
		SStringcat(dest, valstr(stack[argtop-tf_argc+i]));
		if (i == last) break;
		Stringadd(dest, ' ');
	    }
	    *emptyp = dest->len <= orig_len;
	}
    } else {
	if (to_stack)
	    (dest = Stringnew(NULL, 0, 0))->links++;
	orig_len = dest->len;
	SStringoncat(dest, argstring, tf_argv[first].start,
	    tf_argv[last].end - tf_argv[first].start);
	*emptyp = dest->len <= orig_len;
    }

    if (to_stack) {
	if (!val)
	    val = dest && dest->len ? newSstr(CS(dest)) : shareval(val_blank);
	result = pushval(val);
	if (dest) Stringfree(dest);
    }

    return result;
}

static void do_mecho(const Program *prog, int i)
{
    /* XXX is invis_flag set correctly at runtime? */
    if (prog->code[i].start && prog->code[i].end) {
	tfprintf(tferr, "%S%.*s%A", do_mprefix(),
	    prog->code[i].end - prog->code[i].start, prog->code[i].start,
	    mecho_attr);
    }
}

Value *prog_interpret(const Program *prog, int in_expr)
{
    Value *val, *val2, *result = NULL;
    String *str, *tbuf;
    conString *constr;
    int cip, stackbot, no_arg;
    int empty;	/* for varsub default */
    opcode_t op;
    int first, last, n;
    int instruction_count = 0;
    String *buf;
    const char *cstr, *old_cmd;
    struct tf_frame {
	TFILE *orig_tfin, *orig_tfout;	/* restored when done */
	TFILE *local_tfin, *local_tfout;/* restored after pipes */
	TFILE *inpipe, *outpipe;	/* pipes between commands */
    } first_frame, *frame;
#if 0
    TFILE **filep;
#define which_tfile_p(c) \
    (c=='i' ? &tfin : c=='o' ? &tfout : c=='e' ? &tferr : NULL)
#endif

    frame = &first_frame;
    frame->local_tfin = frame->orig_tfin = tfin;
    frame->local_tfout = frame->orig_tfout = tfout;
    frame->inpipe = frame->outpipe = NULL;

    (buf = Stringnew(NULL, 0, 0))->links++;
    stackbot = stacktop;

    for (cip = 0; cip < prog->len; cip++) {
	if (exiting) break;
	if (interrupted()) {
	    eprintf("Macro execution interrupted.");
	    goto prog_interpret_exit;
	}
	if (max_instr > 0 && instruction_count > max_instr) {
	    eprintf("instruction count exceeded %max_instr (%d).", max_instr);
	    goto prog_interpret_exit;
	}
	instruction_count++;
	op = prog->code[cip].op;
	if (mecho > invis_flag) do_mecho(prog, cip);
	if (iecho > invis_flag) inst_dump(prog, cip, 'i');

	if (op_type_is(op, EXPR)) {
	    if (!reduce(op, prog->code[cip].arg.i))
		goto prog_interpret_exit;
	    continue;
	}

#define setup_next_io() \
    do { \
	if (!frame->inpipe) { \
	    frame->local_tfin = tfin; /* save any change */ \
	} else { \
	    tfclose(frame->inpipe); /* close inpipe */ \
	} \
	if (!frame->outpipe) { \
	    frame->local_tfout = tfout; /* save any change */ \
	    tfin = frame->local_tfin; /* no pipe into next cmd */ \
	    frame->inpipe = NULL; /* no pipe into next cmd */ \
	} else { \
	    tfin = frame->inpipe = frame->outpipe; /* pipe into next cmd */ \
	    frame->outpipe = NULL; \
	    tfout = frame->local_tfout; /* no pipe out of next cmd */ \
	} \
    } while (0)

	switch (op) {
	case OP_PIPE:
	    tfout = frame->outpipe = tfopen(NULL, "q");
	    break;

	case OP_EXECUTE:
	    constr = prog->code[cip].arg.str;
	    if ((no_arg = !constr)) constr = CS(buf);
	    handle_command(constr);
	    if (no_arg) Stringtrunc(buf, 0);
	    setup_next_io();
	    break;

	case OP_ARG:
	    constr = prog->code[cip].arg.str;
	    if ((no_arg = !constr)) constr = CS(buf);
	    /* no_arg and constr will be used by BUILTIN, MACRO, SET, ... */
	    break;

	case OP_LET:
	case OP_SET:
	case OP_SETENV:
	    /* no_arg and constr were set by OP_ARG */
	    do_set(prog->code[cip].arg.val->name,
		prog->code[cip].arg.val->u.hash,
		constr, 0, op == OP_SETENV, op == OP_LET);
	    if (no_arg) Stringtrunc(buf, 0);
	    break;

	case OP_BUILTIN: case OP_NBUILTIN:
	case OP_COMMAND: case OP_NCOMMAND:
	    /* no_arg and constr were set by OP_ARG */
	    str = execute_start(constr, &old_cmd); /*XXX optimize: don't dup buf */
	    execute_command(prog->code[cip].arg.cmd, str, 0,
		opnum_eq(op, OP_BUILTIN));
	    execute_end(old_cmd, !(op & OPF_NEG), str);
	    if (no_arg) Stringtrunc(buf, 0);
	    setup_next_io();
	    break;

	case OP_MACRO: case OP_NMACRO:
	    /* no_arg and constr were set by OP_ARG */
	    str = execute_start(constr, &old_cmd); /*XXX optimize: don't dup buf */
	    execute_macro(prog->code[cip].arg.val->name,
		prog->code[cip].arg.val->u.hash, str, 0);
	    execute_end(old_cmd, !(op & OPF_NEG), str);
	    if (no_arg) Stringtrunc(buf, 0);
	    setup_next_io();
	    break;

	case OP_SEND:
	    constr = prog->code[cip].arg.str;
	    if ((no_arg = !constr)) constr = CS(buf);
	    if (constr->len || !snarf) {
#if 0
		if (/*(subs == SUB_MACRO) &&*//*XXX*/ (mecho > invis_flag))
		    tfprintf(tferr, "%S%s%S%A", do_mprefix(), "SEND: ", constr,
			mecho_attr);
#endif
		if (!do_hook(H_SEND, NULL, "%S", constr)) {
		    set_user_result(newint(send_line(constr->data, constr->len,
			TRUE)));
		}
	    }
	    if (no_arg) Stringtrunc(buf, 0);
	    setup_next_io();
	    break;

	case OP_APPEND:
	    constr = prog->code[cip].arg.str;
	    if (!constr) {
		SStringcat(buf, valstr(popval()));
		freeval(opd(0));
	    } else {
		SStringcat(buf, constr);
	    }
	    break;

#define jumpaddr(prog) \
	((prog->code[cip].arg.i < 0) ? prog->len : prog->code[cip].arg.i - 1)

	case OP_JUMP:
	    cip = jumpaddr(prog);
	    break;
	case OP_JZ:
	    if (!valbool(popval()))
		cip = jumpaddr(prog);
	    freeval(opd(0));
	    break;
	case OP_JNZ:
	    if (valbool(popval()))
		cip = jumpaddr(prog);
	    freeval(opd(0));
	    break;
	case OP_JRZ:
	    if (!valbool(user_result))
		cip = jumpaddr(prog);
	    break;
	case OP_JRNZ:
	    if (valbool(user_result))
		cip = jumpaddr(prog);
	    break;
	case OP_JNEMPTY:
	    /* empty was set by one of the varsub operators */
	    if (!empty)
		cip = jumpaddr(prog);
	    break;

	case OP_EXPR:
	    /* Evaulate the expression contained in buf and push its value */
	    val = expr_value(buf->data);
	    Stringtrunc(buf, 0);
	    val = val ? valval(val) : shareval(val_zero);
	    if (!pushval(val)) goto prog_interpret_exit;
	    break;

	case OP_RETURN:
	case OP_RESULT:
	    if ((val = prog->code[cip].arg.val))
		val->count++;
	    else
		val = popval();
	    set_user_result(valval(val));
	    if (op == OP_RESULT && !argtop) {
		constr = valstr(val);
		oputline(constr ? constr : blankline);
	    }
	    cip = prog->len - 1;
	    setup_next_io();
	    break;
	case OP_TEST:
	    if ((val = prog->code[cip].arg.val))
		val->count++;
	    else
		val = popval();
	    set_user_result(valval(val));
	    setup_next_io();
	    break;
	case OP_PUSHBUF:
	    if (!pushval(newptr(buf))) /* XXX optimize */
		goto prog_interpret_exit;
	    (buf = Stringnew(NULL, 0, 0))->links++;
	    break;
	case OP_POPBUF:
	    Stringfree(buf);
	    buf = valptr(popval()); /* XXX optimize */
	    freeval(opd(0));
	    break;
	case OP_CMDSUB:
	    if (!pushval(newptr(frame))) /* XXX optimize */
		goto prog_interpret_exit;
	    frame = XMALLOC(sizeof(*frame));
	    frame->local_tfout = tfout = tfopen(NULL, "q");
	    frame->local_tfin = tfin;
	    frame->inpipe = frame->outpipe = NULL;
	    break;
#if 0
	case OP_POPFILE:
	    filep = which_tfile_p(prog->code[cip].arg.c);
	    tfclose(*filep);
	    *filep = (TFILE*)valptr(popval()); /* XXX optimize */
	    freeval(opd(0));
	    break;
#endif
	case OP_ACMDSUB:
	case OP_PCMDSUB:
	    if (op_is_push(op))
		(tbuf = Stringnew(NULL, 0, 0))->links++;
	    else
		tbuf = buf;
	    first = 1;
	    while ((constr = dequeue((tfout)->u.queue))) {
		if (!((constr->attrs & F_GAG) && gag)) {
		    if (!first) {
			Stringadd(tbuf, ' ');
			if (tbuf->charattrs) tbuf->charattrs[tbuf->len] = 0;
		    }
		    first = 0;
		    SStringcat(tbuf, constr);
		}
		conStringfree(constr);
	    }
	    tfclose(tfout);
	    FREE(frame);
	    frame = valptr(popval()); /* XXX optimize */
	    tfout = frame->outpipe ? frame->outpipe : frame->local_tfout;
	    tfin = frame->inpipe ? frame->inpipe : frame->local_tfin;
	    freeval(opd(0));
	    if (op_is_push(op)) {
		if (!pushval(newSstr(CS(tbuf))))
		    goto prog_interpret_exit;
		Stringfree(tbuf);
	    }
	    break;
	case OP_PBUF:
	    constr = CS(buf);
	    buf = valptr(popval()); /* XXX optimize */
	    freeval(opd(0));
	    if (!pushval(newSstr(constr)))
		goto prog_interpret_exit;
	    conStringfree(constr);
	    break;
	case OP_AMAC:
	case OP_PMAC:
	    if ((constr = prog->code[cip].arg.str)) {
		constr->links++;
	    } else {
		constr = CS(buf);
		buf = valptr(popval()); /* XXX optimize */
		freeval(opd(0));
	    }
	    if (!(cstr = macro_body(constr->data))) {
		tfprintf(tferr, "%% macro not defined: %S", constr);
	    }
	    if (op_is_push(op)) {
		if (!pushval(newstr(cstr ? cstr : "", -1)))
		    goto prog_interpret_exit;
	    } else if (cstr) {
		Stringcat(buf, cstr);
	    }
	    if (mecho > invis_flag)
		tfprintf(tferr, "%S$%S --> %s%A", do_mprefix(), constr, cstr,
		    mecho_attr);
	    conStringfree(constr);
	    break;
	case OP_AVAR:
	    (val = (prog->code[cip].arg.val))->count++;
	    if (!(val2 = hgetnearestvarval(val))) {
		constr = blankline;
		if (patmatch(&looks_like_special_sub_ic, NULL, val->name)) {
		    char upper[64];
		    int i;
		    for (i = 0; i < sizeof(upper) && val->name[i]; i++)
			upper[i] = ucase(val->name[i]);
		    wprintf("\"%%{%s}\" is a variable substitution, "
			"and is not the same as special substitution "
			"\"%%{%.*s}\".", val->name, i, upper);
		}
	    } else {
		constr = valstr(val2);
	    }
	    empty = !constr->len;
	    SStringcat(buf, constr);
	    freeval(val);
	    break;
	case OP_PVAR:
	    (val = (prog->code[cip].arg.val))->count++;
	    (val = valval(val))->count++;
	    if (!pushval(val))
		goto prog_interpret_exit;
	    empty = (val->type & TYPE_STR && !val->sval->len);
	    freeval(val);
	    break;
	case OP_AREG:
	    empty = (regsubstr(buf, prog->code[cip].arg.i) <= 0);
	    break;
	case OP_PREG:
	    str = Stringnew(NULL, 0, 0);
	    empty = (regsubstr(str, prog->code[cip].arg.i) <= 0);
	    if (empty) Stringcat(str, "");
	    if (!pushval(newSstr(CS(str))))
		goto prog_interpret_exit;
	    break;
	case OP_APARM:
	    first = prog->code[cip].arg.i - 1;
	    if (!do_parmsub(buf, first, first, &empty))
		goto prog_interpret_exit;
	    break;
	case OP_PPARM:
	    first = prog->code[cip].arg.i - 1;
	    if (!do_parmsub(NULL, first, first, &empty))
		goto prog_interpret_exit;
	    break;
	case OP_AXPARM:
	    if (!do_parmsub(buf, prog->code[cip].arg.i, tf_argc - 1, &empty))
		goto prog_interpret_exit;
	    break;
	case OP_PXPARM:
	    if (!do_parmsub(NULL, prog->code[cip].arg.i, tf_argc - 1, &empty))
		goto prog_interpret_exit;
	    break;
	case OP_ALPARM:
	    first = tf_argc - prog->code[cip].arg.i;
	    if (!do_parmsub(buf, first, first, &empty))
		goto prog_interpret_exit;
	    break;
	case OP_PLPARM:
	    first = tf_argc - prog->code[cip].arg.i;
	    if (!do_parmsub(NULL, first, first, &empty))
		goto prog_interpret_exit;
	    break;
	case OP_ALXPARM:
	    last = tf_argc - prog->code[cip].arg.i - 1;
	    if (!do_parmsub(buf, 0, last, &empty))
		goto prog_interpret_exit;
	    break;
	case OP_PLXPARM:
	    last = tf_argc - prog->code[cip].arg.i - 1;
	    if (!do_parmsub(NULL, 0, last, &empty))
		goto prog_interpret_exit;
	    break;
	case OP_APARM_CNT:
	    empty = 0;
	    Sappendf(buf, "%d", tf_argc);
	    break;
	case OP_PPARM_CNT:
	    empty = 0;
	    if (!pushval(newint(tf_argc)))
		goto prog_interpret_exit;
	    break;
	case OP_ARESULT:
	    empty = 0;
	    SStringcat(buf, valstr(user_result));
	    break;
	case OP_PRESULT:
	    empty = 0;
	    if (!pushval(user_result))
		goto prog_interpret_exit;
	    user_result->count++;
	    break;
	case OP_ACMDNAME:
	    if (!(empty = !(current_command && *current_command != '\b')))
		Stringcat(buf, current_command);
	    break;
	case OP_PCMDNAME:
	    if ((empty = !(current_command && *current_command != '\b')))
		val = shareval(val_blank);
	    else
		val = newstr(current_command, -1);
	    if (!pushval(val))
		goto prog_interpret_exit;
	    break;
	case OP_APARM_ALL:
	    if (!do_parmsub(buf, 0, tf_argc - 1, &empty))
		goto prog_interpret_exit;
	    break;
	case OP_PPARM_ALL:
	    if (!do_parmsub(NULL, 0, tf_argc - 1, &empty))
		goto prog_interpret_exit;
	    break;
	case OP_APARM_RND:
	    n = tf_argc ? RRAND(0, tf_argc - 1) : -1;
	    if (!do_parmsub(buf, n, n, &empty))
		goto prog_interpret_exit;
	    break;
	case OP_PPARM_RND:
	    n = tf_argc ? RRAND(0, tf_argc - 1) : -1;
	    if (!do_parmsub(NULL, n, n, &empty))
		goto prog_interpret_exit;
	    break;
	case OP_DUP:	/* duplicate the ARGth item from the top of the stack */
	    (val = opd(prog->code[cip].arg.i))->count++;
	    if (!pushval(val)) goto prog_interpret_exit;
	    break;
	case OP_POP:	/* argument is ignored */
	    freeval(popval());
	    break;
	case OP_PUSH:	/* push (Value*)ARG onto stack */
	    (val = (prog->code[cip].arg.val))->count++;
	    if (!pushval(val)) goto prog_interpret_exit;
	    break;
	case OP_ENDIF:
	case OP_DONE:
	case OP_NOP:
	    /* no op: place holders for mecho pointers */
	    break;
	default:
	    internal_error(__FILE__, __LINE__, "invalid opcode 0x%04X at %d",
		op, cip);
	    goto prog_interpret_exit;
	}
    }

    if (stacktop == stackbot + in_expr) {
	if (in_expr) {
	    result = popval();
	} else {
	    /* Note: a macro prog may never set user_result, but have no
	     * errors either; in that case we must set user_result here. */
	    if (!user_result) user_result = shareval(val_blank);
	    result = user_result;
	}
    } else {
	eprintf("expression stack underflow or dirty (%+d)",
	    stacktop - (stackbot + in_expr));
    }

    if (frame != &first_frame)
	internal_error(__FILE__, __LINE__, "invalid frame");

prog_interpret_exit:
    frame = &first_frame; /* if loop aborted, frame may be wrong */
    tfin = frame->orig_tfin;
    tfout = frame->orig_tfout;

    while (stacktop > stackbot)
	freeval(popval());
    Stringfree(buf);
    return result;
}

int prog_run(const Program *prog, const String *args, int offset,
    const char *name, int kbnumlocal)
{
    Arg *true_argv = NULL;		/* unshifted argument vector */
    int saved_cmdsub, saved_argc, saved_argtop;
    Arg *saved_argv;
    const conString *saved_argstring;
    const char *saved_command;
    TFILE *saved_tfin, *saved_tfout, *saved_tferr;
    List scope[1];

    if (++recur_count > max_recur && max_recur) {
        eprintf("recursion count exceeded %max_recur (%d)", max_recur);
        recur_count--;
        return 0;
    }
    saved_command = current_command;
    saved_cmdsub = cmdsub_count;
    saved_argstring = argstring;
    saved_argc = tf_argc;
    saved_argv = tf_argv;
    saved_argtop = argtop;
    saved_tfin = tfin;
    saved_tfout = tfout;
    saved_tferr = tferr;

    if (name) current_command = name;
    cmdsub_count = 0;

    pushvarscope(scope);
    if (kbnumlocal) {
	/* TODO: make this local %kbnum const */
	setlocalintvar("kbnum", kbnumlocal);
    }

    if (args) {
        argstring = (const conString*)args;
	tf_argc = stringvec(args, offset, &tf_argv, 20);
        true_argv = tf_argv;
        argtop = 0;
    }
       /* else, leave argstring, tf_argv, and tv_argc alone, so /eval body
        * inherits positional parameters */

    if (tf_argc >= 0) {
        if (!prog_interpret(prog, !name)) set_user_result(NULL);
    }

    if (true_argv) FREE(true_argv);
    popvarscope();

    tfin = saved_tfin;
    tfout = saved_tfout;
    tferr = saved_tferr;
    cmdsub_count = saved_cmdsub;
    tf_argc = saved_argc;
    tf_argv = saved_argv;
    argstring = saved_argstring;
    argtop = saved_argtop;
    current_command = saved_command;
    recur_count--;
    return !!user_result;
}

String *do_mprefix(void)
{
    STATIC_BUFFER(buffer);
    int i;

    Stringtrunc(buffer, 0);
    for (i = 0; i < recur_count + cmdsub_count; i++)
        SStringcat(buffer, mprefix);
    Stringadd(buffer, ' ');
    if (current_command) {
        if (*current_command == '\b') {
            Sappendf(buffer, "%s: ", current_command+1);
        } else {
            Sappendf(buffer, "/%s: ", current_command);
        }
    }
    return buffer;
}

#define keyword_mprefix() \
{ \
  if (mecho > invis_flag) \
    tfprintf(tferr, "%S/%s%A", do_mprefix(), keyword_label(block), mecho_attr);\
}

static void code_mark(Program *prog, const char *mark)
{
    int i;

    prog->mark = mark;
    for (i = prog->len - 1; i >= 0; i--) {
	if (prog->code[i].start && !prog->code[i].end) {
	    prog->code[i].end = mark;
	    break;
	}
    }
}

static void vcode_add(Program *prog, opcode_t op, int use_mark, va_list ap)
{
    Instruction *inst;

    /* The -1 in the condition, and zeroing of instructions, are to allow
     * the use of comefrom() on code that hasn't been emitted yet. */
    if (prog->len >= prog->size - 1) {
	prog->size += 20;
	prog->code = XREALLOC(prog->code, prog->size * sizeof(Instruction));
	memset(&prog->code[prog->size - 20], 0, 20 * sizeof(Instruction));
    }

    inst = &prog->code[prog->len];
    prog->len++;
    inst->start = inst->end = NULL;
    inst->op = op;

    switch (op_arg_type(op)) {
	case OPA_INT:  inst->arg.i    = va_arg(ap, int);		break;
	case OPA_CHAR: inst->arg.c    = (char)va_arg(ap, int);		break;
	case OPA_STRP: inst->arg.str  = va_arg(ap, conString *);	break;
	case OPA_VALP: inst->arg.val  = va_arg(ap, Value *);		break;
	case OPA_CMDP: inst->arg.cmd  = va_arg(ap, BuiltinCmd *);	break;
	default:       inst->arg.i    = 0;
    }

    if (prog->mark && use_mark) {
	inst->start = prog->mark;
	prog->mark = NULL;
    }

    if (!prog->optimize)
	return;

#if 1 /* optimizer */
#define inst_is_const(inst) \
    ((inst)->op == OP_PUSH && (inst)->arg.val->type != TYPE_ID)

    while (1) {
	if (inst->comefroms) {
	    /* Something jumps to this instruction, so we can't consolidate it
	     * with any instruction before it. */
	    return;
	}

	switch (op_type(inst->op)) {
	case OPT_EXPR:
	    /* An expression operator with no side effects and compile-time
	     * constant operands can be reduced at compile time.
	     */
	    /* e.g. {PUSH 3; PUSH 4; +;} to {PUSH 7} */
	    if (!op_has_sideeffect(inst->op)) {
		int i, n;
		int old_stacktop = stacktop;
		n = inst->arg.i;
		for (i = prog->len - n - 1; i < prog->len - 1; i++) {
		    if (!inst_is_const(prog->code+i) || prog->code[i+1].comefroms)
			return;
		}
		for (i = prog->len - n - 1; i < prog->len - 1; i++) {
		    if (!pushval(prog->code[i].arg.val))
			goto const_expr_error;
		    prog->code[i].arg.val = NULL;
		}
		if (!reduce(inst->op, n))
		    goto const_expr_error;
		prog->len -= n;
		inst = &prog->code[prog->len - 1];
		/* inst->op = OP_PUSH; */ /* already true */
		inst->arg.val = popval();
		continue;
	    const_expr_error:
		while (stacktop > old_stacktop)
		    freeval(popval());
		return /* 0 */; /* XXX */
	    }
	    return;

	case OPT_CTRL:
	    if (prog->len <= 1)
		return;

	    if (inst->op == OP_APPEND && inst->arg.str) {
		/* {APPEND "x";} */

		if (inst->arg.str->len == 0) {
		    /* {APPEND "";} to {} */
		    prog->len--;
		    inst--;
		    continue;
		} else if (inst[-1].op == OP_APPEND && inst[-1].arg.str) {
		    /* e.g. {APPEND "x"; APPEND "y";} to {APPEND "xy";} */
		    String *str;
		    prog->len--;
		    inst--;
		    /* XXX optimize - don't need to dup inst->arg.str */
		    str = SStringcat(Stringdup(inst->arg.str), inst[1].arg.str);
		    conStringfree(inst[0].arg.str);
		    conStringfree(inst[1].arg.str);
		    (inst[0].arg.str = CS(str))->links++;
		    /* inst->op = OP_APPEND; */ /* already true */
		    continue;
		}

	    } else if (inst->op == OP_APPEND && !inst->arg.str) {
		/* {APPEND NULL;} */

		if (op_type_is(inst[-1].op, SUB) && op_is_push(inst[-1].op)) {
		    /* e.g. {PPARM 1; APPEND NULL;} to {APARM 1;} */
		    prog->len--;
		    inst--;
		    inst->op |= OPF_APP;
		    continue;
		} else if (inst_is_const(inst-1)) {
		    /* e.g. {PUSH "x"; APPEND NULL;} to {APPEND "x";} */
		    Value *val = inst[-1].arg.val;
		    prog->len--;
		    inst--;
		    inst->op = inst[1].op;
		    (inst->arg.str = valstr(val))->links++;
		    freeval(val);
		    continue;
		}

	    } else if (op_arg_type_is(inst->op, STRP) && !inst->arg.str &&
		inst[-1].op == OP_APPEND && inst[-1].arg.str)
	    {
		/* e.g. {APPEND string; MACRO NULL;} to {MACRO string;} */
		/* but only if not preceeded by other append operators */
		if (prog->len > 2 &&
		   (inst[-2].op == OP_APPEND /* could be {APPEND NULL;} */ ||
		   (op_type_is(inst[-2].op, SUB) && op_is_append(inst[-2].op))))
		{
		    return;
		}
		prog->len--;
		inst--;
		inst->op = inst[1].op;
		/* keep inst->arg.str */
		continue;

	    } else if (op_arg_type_is(inst->op, VALP) && !inst->arg.val &&
		inst_is_const(inst-1))
	    {
		/* e.g., {PUSH val; TEST NULL;} to {TEST val;} */
		prog->len--;
		inst--;
		inst->op = inst[1].op;
		/* inst->arg.val = inst[1].arg.val; */ /* already true */
		continue;
	    }
	    return;

	case OPT_JUMP:
	    if (prog->len <= 1)
		return;
	    if (opnum_eq(inst->op, OP_JZ)) {
		if (inst[-1].op == '!') {
		    /* e.g., {! 1; JZ x;} to {JNZ x;} */
		    prog->len--;
		    inst--;
		    inst->op = inst[1].op ^ OPF_NEG;
		    inst->arg = inst[1].arg;
		    continue;
		} else if (inst_is_const(inst-1)) {
		    /* e.g., {PUSH INT 0; JZ x;} to {JUMP x;} */
		    /* e.g., {PUSH INT 1; JZ x;} to {} */
		    int flag;
		    prog->len--;
		    inst--;
		    flag = valbool(inst->arg.val);
		    freeval(inst->arg.val);
		    if (!flag == !(inst[1].op & OPF_NEG)) {
			inst->op = OP_JUMP;
			inst->arg.i = inst[1].arg.i;
		    } else {
			prog->len--;
			inst--;
		    }
		    continue;
		}
	    }
	    return;

	default:
	    /* sub of a macro whose name is compile-time constant */
	    if (opnum_eq(inst->op, OP_PMAC) &&
		!inst->arg.str && prog->len > 2 && inst[-2].op == OP_PUSHBUF &&
		inst[-1].op == OP_APPEND)
	    {
		/* e.g. {PUSHBUF; APPEND name; PMAC NULL} to {PMAC name} */
		inst[-2].op = inst->op;
		inst[-2].arg.str = inst[-1].arg.str;
		prog->len -= 2;
		continue;
	    }

	    if (inst->op == OP_PBUF && !inst->arg.str && prog->len > 2 &&
		inst[-2].op == OP_PUSHBUF && inst[-1].op == OP_APPEND)
	    {
		/* {PUSHBUF; APPEND text; PBUF NULL} to {PUSH text} */
		inst[-2].op = OP_PUSH;
		inst[-2].arg.val = newSstr(inst[-1].arg.str);
		prog->len -= 2;
		continue;
	    }

	    return;
	}
    }
#endif /* optimizer */
}

void code_add(Program *prog, opcode_t op, ...)
{
    va_list ap;
    va_start(ap, op);
    vcode_add(prog, op, 1, ap);
    va_end(ap);
}

static void code_add_nomecho(Program *prog, opcode_t op, ...)
{
    va_list ap;
    va_start(ap, op);
    vcode_add(prog, op, 0, ap);
    va_end(ap);
}

void eat_newline(Program *prog)
{
    while (ip[0] == '\\' && ip[1] == '\n') {
	loadstart++;
	for (ip += 2; is_space(*ip); ip++);
    }
}

void eat_space(Program *prog)
{
    while (is_space(*ip)) ip++;
    eat_newline(prog);
}

static inline const char *keyword_label(keyword_id_t id)
{
    return keyword_table[id - BREAK];
}

static void unexpected(keyword_id_t innerblock, keyword_id_t outerblock)
{
    /* THEN and DO blocks don't start with a /then or /do if they belong to a
     * /if () or /while (), so "THEN block" and "DO block" would be confusing
     * to user. */
    switch (outerblock) {
    case THEN:
	eprintf("unexpected /%s in IF body", keyword_label(innerblock));
	break;
    case DO:
	eprintf("unexpected /%s in WHILE body", keyword_label(innerblock));
	break;
    default:
	eprintf("unexpected /%s in %s block", keyword_label(innerblock),
	    outerblock ? keyword_label(outerblock) : "outer");
    }
}

static int list(Program *prog, int subs)
{
    keyword_id_t oldblock;
    int is_a_command, is_a_condition, is_special;
    int failed = 0, result = 0;
    const char *stmtstart;
    int is_pipe = 0;
    int block_start, jump_point, oldlen;

    /* Do NOT strip leading space here.  This allows user to type and send
     * lines with leading spaces (but completely empty lines are handled
     * by handle_input_line()).  During expansion, spaces AFTER a "%;"
     * or keyword will be skipped.
     */

    if (block == WHILE || block == IF) {
	block_start = prog->len;
    }

    do /* while (*ip) */ {
        if (subs >= SUB_NEWLINE) {
            while (is_space(*ip) || (ip[0] == '\\' && is_space(ip[1])))
                ++ip;
	    eat_newline(prog);
	}

        is_special = is_a_command = is_a_condition = FALSE;

        /* Lines begining with one "/" are tf commands.  Lines beginning
         * with multiple "/"s have the first removed, and are sent to server.
         */

        if ((subs > SUB_LITERAL) && (*ip == '/') && (*++ip != '/')) {
            is_a_command = TRUE;
            oldblock = block;
            if (subs >= SUB_KEYWORD) {
                stmtstart = ip;
                is_special = block = keyword_parse(prog);
            }

        } else if ((subs > SUB_LITERAL) &&
            (block == IF || block == ELSEIF || block == WHILE))
        {
            if (*ip == '(') {
                is_a_condition = TRUE;
                oldblock = block;
                ip++; /* skip '(' */
                if (!expr(prog)) goto list_exit;
                if (*ip != ')') {
                    parse_error(prog, "condition", "operator or ')'");
                    goto list_exit;
                }
		++ip; /* skip ')' */
		eat_space(prog);
		if (is_end_of_statement(ip)) {
		    wprintf("\"%2.2s\" following \"/%s (...)\" "
			"sends blank line to server, "
			"which is probably not what was intended.",
			ip, keyword_label(block));
		}
                block = (block == WHILE) ? DO : THEN;
            } else if (*ip) {
                wprintf("statement starting with %s in /%s "
                    "condition sends text to server, "
                    "which is probably not what was intended.",
                    error_text(prog), keyword_label(block));
            }
        }

        if (is_a_command || is_a_condition) {
            switch(block) {
            case WHILE:
                if (subs == SUB_KEYWORD)
		    subs = SUB_NEWLINE;
                if (!list(prog, subs)) failed = 1;
                else if (block == WHILE) {
                    parse_error(prog, "macro", "/do");
                    failed = 1;
                } else if (block == DO) {
                    parse_error(prog, "macro", "/done");
                    failed = 1;
                }
                block = oldblock;
                if (failed) goto list_exit;
                break;

            case DO:
                if (oldblock != WHILE) {
                    unexpected(block, oldblock);
                    block = oldblock;
                    goto list_exit;
                }
		oldlen = prog->len;
		code_add(prog, is_a_condition ? OP_JZ : OP_JRZ, -1);
		code_mark(prog, ip);
		if (prog->len > oldlen) /* code was emitted */
		    jump_point = prog->len - 1;
		else /* code was completely optimized away */
		    jump_point = - 1;
                continue;

            case BREAK:
	    {
		int levels =
		    (!*ip || is_end_of_statement(ip) || is_end_of_cmdsub(ip)) ?
		    1 : strtoint(ip, &ip);
                if (levels <= 0) {
		    parse_error(prog, "/BREAK", "positive integer");
                    block = oldblock;
                    goto list_exit;
                }
		code_add_nomecho(prog, OP_JUMP, -levels);
		block = oldblock;
		if (is_end_of_statement(ip)) {
		    ip += 2;
		    eat_space(prog);
		}
		code_mark(prog, ip); /* XXX ??? */
                continue;
	    }

            case DONE:
                if (oldblock != DO) {
                    unexpected(block, oldblock);
                    block = oldblock;
                    goto list_exit;
                }
		code_add_nomecho(prog, OP_JUMP, block_start);
		/* fill in the jump address for each BREAK */
		{
		    int i;
		    for (i = block_start; i < prog->len; i++) {
			if (op_type_is(prog->code[i].op, JUMP) &&
			    prog->code[i].arg.i < 0 &&
			    prog->code[i].arg.i != ENDIF_PLACEHOLDER)
			{
			    if (prog->code[i].arg.i == -1)
				prog->code[i].arg.i = prog->len;
			    else
				prog->code[i].arg.i++;
			}
		    }
		}
		code_add(prog, OP_DONE, 0);
		comefrom(prog, jump_point, prog->len - 1);
		eat_space(prog);
		code_mark(prog, ip);
		result = 1;  goto list_exit;

            case IF:
                if (subs == SUB_KEYWORD)
		    subs = SUB_NEWLINE;
                if (!list(prog, subs)) {
                    failed = 1;
                } else if (block == IF || block == ELSEIF) {
                    parse_error(prog, "macro", "/then");
                    failed = 1;
                } else if (block == THEN || block == ELSE) {
                    parse_error(prog, "macro", "/endif");
                    failed = 1;
                }
                block = oldblock;
                if (failed) goto list_exit;
		code_mark(prog, ip);
                break;

            case THEN:
                if (oldblock != IF && oldblock != ELSEIF) {
                    unexpected(block, oldblock);
                    block = oldblock;
                    goto list_exit;
                }
		code_add_nomecho(prog, is_a_condition ? OP_JZ : OP_JRZ,
		    ENDIF_PLACEHOLDER);
		code_mark(prog, ip);
		if (prog->len > 0 &&
		    op_type_is(prog->code[prog->len - 1].op, JUMP))
		{
		    jump_point = prog->len - 1;
		} else { /* jump was completely optimized away */
		    jump_point = -1;
		}
                continue;

            case ELSEIF:
            case ELSE:
                if (oldblock != THEN) {
                    unexpected(block, oldblock);
                    block = oldblock;
                    goto list_exit;
                }
		code_add_nomecho(prog, OP_JUMP, ENDIF_PLACEHOLDER);
		comefrom(prog, jump_point, prog->len);
		eat_space(prog);
		if (block == ELSE && is_end_of_statement(ip)) {
		    wprintf("\"%2.2s\" following \"/%s\" "
			"sends blank line to server, "
			"which is probably not what was intended.",
			ip, keyword_label(block));
		}
                continue;

            case ENDIF:
                if (oldblock != THEN && oldblock != ELSE) {
                    unexpected(block, oldblock);
                    block = oldblock;
                    goto list_exit;
                }
		eat_space(prog);
		/* fill in the jump address after each THEN block */
		{
		    int i;
		    for (i = block_start; i < prog->len; i++) {
			if (op_type_is(prog->code[i].op, JUMP) &&
			    prog->code[i].arg.i == ENDIF_PLACEHOLDER)
			{
			    prog->code[i].arg.i = prog->len;
			}
		    }
		}
		code_add(prog, OP_ENDIF);   /* no-op, to hold mecho pointers */
                result = 1;  goto list_exit;

	    case LET:
	    case SET:
	    case SETENV:
	      {
		String *dest;
		Value *val;
		const char *start = ip, *end;
		while (is_space(*start)) start++;
		end = spanvar(start);
		ip = end;
		if (setdelim(&ip) < 1) {
		    /* "invalid" name may expand to valid name at runtime */
		    goto noncontrol;
		}
		(dest = Stringnew(NULL, 1, 0))->links++;
		if (!(result = expand(prog, dest, subs, NULL)))
		    goto list_exit;
		val = newid(start, end - start);
		code_add(prog, OP_ARG, result == 1 ? dest : NULL);
		switch (block) {
		    case LET:    code_add(prog, OP_LET,    val); break;
		    case SET:    code_add(prog, OP_SET,    val); break;
		    case SETENV: code_add(prog, OP_SETENV, val); break;
		    default:     /* impossible */ break;
		}
		block = oldblock;
		break;
	      }

	    case RETURN:
	    case RESULT:
		if (cmdsub_count) {
		    eprintf("/%s may be called only directly from a macro, "
			"not in $() command substitution.",
			keyword_label(block));
                    goto list_exit;
		}
		/* FALL THROUGH */
	    case TEST:
	      {
		Value *val = NULL;
		String *dest;
		/* len=1 forces data allocation (expected by prog_interpret) */
		(dest = Stringnew(NULL, 1, 0))->links++;
		result = expand(prog, dest, subs, NULL);
		if (!result)
		    goto list_exit;
		if (result == 2) {
		    /* expr is generated at runtime, must defer compilation */
		    code_add(prog, OP_EXPR, NULL);

		} else {
		    /* expression is fixed, can compile it now */
		    const char *saved_ip = ip;
		    int exprstart = prog->len; /* start of expr code in prog */
		    ip = dest->data;
		    eat_space(prog);
		    if (!*ip) {
			val = shareval(val_zero);
		    } else if (!expr(prog)) {
			prog_free_tail(prog, exprstart); /* rm bad expr code */
			val = shareval(val_zero);
		    } else if (*ip) {
			parse_error(prog, "expression", "end of expression");
			prog_free_tail(prog, exprstart); /* rm bad expr code */
			val = shareval(val_zero);
		    }
		    ip = saved_ip; /* XXX need to restore line number too */
		    /* XXX is this safe? prog may have debug ptrs into dest. */
		    Stringfree(dest);
		}
		switch (block) {
		    case TEST:   code_add(prog, OP_TEST, val);   break;
		    case RETURN: code_add(prog, OP_RETURN, val); break;
		    case RESULT: code_add(prog, OP_RESULT, val); break;
		    default: /* impossible */ break;
		}
		block = oldblock;
                break;
	      }

            default:
            noncontrol:
                /* not a control statement */
                ip = stmtstart - 1;
                is_special = 0;
                block = oldblock;
                if (!statement(prog, subs)) goto list_exit;
                break;
            }

        } else /* !(is_a_command || is_a_condition) */ {
            if (is_pipe) {
                eprintf("Piping input to a server command is not allowed.");
                goto list_exit;
            }
            if (!statement(prog, subs))
                goto list_exit;
        }

        is_pipe = (ip[0] == '%' && ip[1] == '|'); /* this stmnt pipes to next */
        if (is_pipe) {
	    Instruction tmp;
            if (!is_a_command) {
                eprintf("Piping output of a server command is not allowed.");
                goto list_exit;
            } else if (is_special) {
                eprintf("Piping output of a special command is not allowed.");
                goto list_exit;
            }
	    /* OP_PIPE needs to go BEFORE the exec operator, so we use
	     * code_add() to add it, then swap the last two instructions. */
            code_add(prog, OP_PIPE);
	    tmp = prog->code[prog->len - 1];
	    prog->code[prog->len - 1] = prog->code[prog->len - 2];
	    prog->code[prog->len - 2] = tmp;
        }

        if (is_end_of_cmdsub(ip)) {
            break;
        } else if (is_end_of_statement(ip)) {
            ip += 2;
	    eat_space(prog);
	    code_mark(prog, ip);
        } else if (*ip) {
	    const char *expect;
	    switch (is_special) {
		case IF:    expect = "end of statement after /endif";  break;
		case WHILE: expect = "end of statement after /done";   break;
		default:    expect = "end of statement";               break;
	    }
	    parse_error_suggest(prog, "macro", expect, "Possibly missing '%;'");
            goto list_exit;
        }

    } while (*ip);

    code_mark(prog, ip);
    result = 1;

    if (is_pipe) {
        eprintf("'%|' must be followed by another command.");
        result = 0;
    }

list_exit:
    return !!result;
}

const char **keyword(const char *id)
{
    return (const char **)bsearch((void*)id, (void*)keyword_table,
        sizeof(keyword_table)/sizeof(char*), sizeof(char*), cstrstructcmp);
}

static keyword_id_t keyword_parse(Program *prog)
{
    const char **result, *start, *end;
    char buf[KEYWORD_LENGTH+1];

    start = (*ip == '@') ? ip + 1 : ip;
    if (!is_keystart(*start)) return 0;		/* fast heuristic */

    end = start + 1;
    while (*end && !is_space(*end) && *end != '%' && *end != ')') {
	if (++end - start > KEYWORD_LENGTH)
	    return 0; /* too long to be keyword */
    }

    /* XXX stupid copy */
    strncpy(buf, start, end - start);
    buf[end - start] = '\0';
    if (!(result = keyword(buf)))
        return 0;
    for (ip = end; is_space(*ip); ip++);
    return BREAK + (result - keyword_table);
}

/* percentsub() and dollarsub() append to the compile-time buffer *destp if
 * source is compile-time constant; otherwise, it emits {APPEND *destp},
 * creates a new compile-time buffer, and emits code for the sub.
 */
static int percentsub(Program *prog, int subs, String **destp)
{
    int result = 1;

    if (*ip == '%') {
	while (*ip == '%') Stringadd(*destp, *ip++);
    } else if (!*ip || is_space(*ip)) {
	/* "% " and "%" at end of line are left alone */
	Stringadd(*destp, '%');
    } else if (subs >= SUB_FULL) {
	if ((*destp)->len) {
	    code_add(prog, OP_APPEND, *destp);
	    (*destp = Stringnew(NULL, 0, 0))->links++;
	}
	result = varsub(prog, 0, 0);
    } else {
	Stringadd(*destp, '%');
    }
    return result;
}

int dollarsub(Program *prog, String **destp)
{
    int result = 1;

    if (*ip == '$' && destp) {
	while (*ip == '$') Stringadd(*destp, *ip++);
    } else {
	if (destp && (*destp)->len) {
	    code_add(prog, OP_APPEND, *destp);
	    (*destp = Stringnew(NULL, 0, 0))->links++;
	}
	result = ((*ip == '[') ? exprsub(prog, !destp) :
	    (*ip == '(') ? cmdsub(prog, !destp) :
	    macsub(prog, !destp));
    }
    return result;
}

/* returns: 0=error, 1=static, 2=dynamic */
static int expand(Program *prog, String *dest, int subs, opcmd_t *opcmdp)
{
    const char *start;
    int error = 0;
    int orig_len = prog->len;

    while (*ip) {
	eat_newline(prog);
	/* XXX The '\\' test used to be >= SUB_NEWLINE when this code was
	 * part of statement(), but when test/return/result were implemented
	 * as real keywords, things like
	 *   /test echo('\\.')
	 * gave the wrong results.  Changing it to SUB_FULL fixed that problem,
	 * but it fails to generate a 'legal escapes' warning for
	 *   /test echo('\.')
	 * and may have caused other problems I haven't discovered yet.
	 */
        if (*ip == '\\' && subs >= SUB_FULL) {
            ++ip;
            if (!backsub(prog, dest)) { error++; break; }
        } else if (*ip == '/' && subs >= SUB_FULL) {
            ++ip;
            if (!slashsub(prog, dest)) { error++; break; }
        } else if (*ip == '%' && subs >= SUB_NEWLINE) {
            if (is_end_of_statement(ip)) {
                while (dest->len && is_space(dest->data[dest->len-1]))
                    Stringtrunc(dest, dest->len-1);  /* nuke spaces before %; */
                break;
            }
            ++ip;
	    if (!percentsub(prog, subs, &dest)) { error++; break; }
	    if (prog->len != orig_len) opcmdp = NULL;
        } else if (*ip == '$' && subs >= SUB_FULL) {
            ++ip;
	    if (!dollarsub(prog, &dest)) { error++; break; }
	    if (prog->len != orig_len) opcmdp = NULL;
        } else if (subs >= SUB_FULL && is_end_of_cmdsub(ip)) {
            break;
        } else {
            /* is_statmeta() is much faster than all those if statements. */
            start = ip++;
            while (*ip && !is_statmeta(*ip) && !(is_space(*ip) && opcmdp))
		ip++;
            SStringoncat(dest, prog->src, start - prog->src->data, ip - start);
	    if (opcmdp && (!*ip || is_space(*ip) || is_end_of_statement(ip))) {
		/* command name is constant, can be resolved at compile time */
		int i, truth = 1;
		for (i = 0; dest->data[i] == '!'; i++)
		    truth = !truth;
		opcmdp->op = (dest->data[i] == '@') ? (++i, OP_BUILTIN) :
		    OP_COMMAND;
		if ((opcmdp->ptr = find_builtin_cmd(dest->data + i))) {
		    /* BUILTIN or COMMAND: ptr is CMD builtin */
		} else if (opcmdp->op == OP_BUILTIN) {
		    eprintf("%s: not a builtin command", dest->data + i);
		    error++;
		    break;
		} else {
		    /* MACRO: ptr is ID name */
		    Value *val = newid(dest->data + i, dest->len - i);
		    if (dest->data[i] == '#') /* macro_hash() */
			val->u.hash = atoi(dest->data + i + 1);
		    opcmdp->ptr = val;
		    opcmdp->op = OP_MACRO;
		}
		Stringtrunc(dest, 0);
		while (is_space(*ip)) { ip++; }
		if (!truth)
		    opcmdp->op |= OPF_NEG;
		opcmdp = NULL;
	    }
        }
    }

    if (error) {
	Stringfree(dest);
	return 0; /* error */

    } else if (prog->len == orig_len) {
	return 1; /* static */

    } else {
	/* text is generated at runtime */
	if (dest->len) {
	    code_add(prog, OP_APPEND, dest);
	} else {
	    Stringfree(dest);
	}
	dest = NULL;
	return 2; /* dynamic */
    }
}

static int statement(Program *prog, int subs)
{
    String *dest;
    int result;
    opcmd_t opcmd, *opcmdp = NULL;

    opcmd.ptr = NULL;

    if (ip[0] != '/' || subs <= SUB_LITERAL) {
	opcmd.op = OP_SEND;
    } else if (ip[1] == '/') {
	ip++;
	opcmd.op = OP_SEND;
    } else {
	ip++;
	opcmd.op = OP_EXECUTE;
	opcmdp = &opcmd;
    }

    /* len=1 forces dest->data to be allocated (expected by prog_interpret) */
    (dest = Stringnew(NULL, 1, 0))->links++;

    result = expand(prog, dest, subs, opcmdp);

    if (!result)
	return 0;

    if (opcmd.ptr) {
	code_add(prog, OP_ARG, result == 1 ? dest : NULL);
	code_add(prog, opcmd.op, opcmd.ptr);
    } else {
	code_add(prog, opcmd.op, result == 1 ? dest : NULL);
    }
    return result;
}

static int slashsub(Program *prog, String *dest)
{
    if (*ip == '/' && oldslash)
        while (*ip == '/') Stringadd(dest, *ip++);
    else
        Stringadd(dest, '/');
    return 1;
}

static const char *error_text(Program *prog)
{
    STATIC_BUFFER(buf);

    if (*ip) {
        const char *end = ip + 1;
        if (is_alnum(*ip) || is_quote(*ip) || *ip == '/') {
            while (is_alnum(*end)) end++;
        }
        Sprintf(buf, "'%.*s'", end - ip, ip);
        return buf->data;
    } else {
        return "end of body";
    }
}

void parse_error(Program *prog, const char *type, const char *expect)
{
    eprintf("%s syntax error: expected %s, found %s.",
        type, expect, error_text(prog));
}

void parse_error_suggest(Program *prog, const char *type, const char *expect,
    const char *suggestion)
{
    eprintf("%s syntax error: expected %s, found %s.  (%s)",
        type, expect, error_text(prog), suggestion);
}


int exprsub(Program *prog, int in_expr)
{
    int result = 0;

    ip++; /* skip '[' */
    eat_space(prog);
    if (!expr(prog)) return 0;
    if (!*ip || is_end_of_statement(ip)) {
        eprintf("unmatched $[");
    } else if (*ip != ']') {
        parse_error(prog, "expression", "operator or ']'");
    } else {
	if (!in_expr)
	    code_add(prog, OP_APPEND, NULL);
        ++ip;
        result = 1;
    }
    return result;
}


static int cmdsub(Program *prog, int in_expr)
{
    int result;
    const char *saved_mark;

    code_add(prog, OP_CMDSUB);
    code_add(prog, OP_PUSHBUF, 0);
    cmdsub_count++;

    ip++; /* skip '(' */
    saved_mark = prog->mark;
    prog->mark = ip;
    result = list(prog, SUB_MACRO);
    prog->mark = saved_mark;

    cmdsub_count--;
    code_add(prog, OP_POPBUF, 0);
    code_add(prog, in_expr ? OP_PCMDSUB : OP_ACMDSUB);

    if (*ip != ')') {
        eprintf("unmatched (");
        return 0;
    }

    ip++;
    return result;
}

static int macsub(Program *prog, int in_expr)
{
    const char *s;
    int bracket;
    String *name;

    code_add(prog, OP_PUSHBUF, 0);

    (name = Stringnew(NULL, 0, 0))->links++;
    if ((bracket = (*ip == '{'))) ip++;
    while (*ip) {
        if (*ip == '\\') {
            ++ip;
            if (!backsub(prog, name)) goto macsub_err;
        } else if (is_end_of_statement(ip) || is_end_of_cmdsub(ip)) {
            break;
        } else if (*ip == '/') {
            ++ip;
            if (!slashsub(prog, name)) goto macsub_err;
        } else if (*ip == '}') {
            /* note: in case of "%{var-$mac}", we break even if !bracket. */
            /* Users shouldn't use '}' in macro names anyway. */
            break;
        } else if (!bracket && is_space(*ip)) {
            break;
        } else if (*ip == '$') {
            if (ip[1] == '$') {
                while(*++ip == '$') Stringadd(name, *ip);
            } else {
                if (!bracket) break;
                else Stringadd(name, *ip++);
            }
        } else if (*ip == '%') {
            ++ip;
            if (!percentsub(prog, SUB_FULL, &name)) goto macsub_err;
        } else {
            for (s = ip++; *ip && !is_punct(*ip) && !is_space(*ip); ip++);
            SStringoncat(name, prog->src, s - prog->src->data, ip - s);
        }
    }
    if (bracket) {
        if (*ip != '}') {
            eprintf("unmatched ${");
            goto macsub_err;
        } else ip++;
    } else if (*ip == '$') {
        ip++;
    }

    code_add(prog, OP_APPEND, name);
    code_add(prog, in_expr ? OP_PMAC : OP_AMAC, NULL);

    return 1;

macsub_err:
    Stringfree(name);
    return 0;
}

static int backsub(Program *prog, String *dest)
{
    if (is_digit(*ip)) {
        char c = strtochr(ip, &ip);
        Stringadd(dest, mapchar(c));
    } else if (!backslash) {
        Stringadd(dest, '\\');
    } else if (*ip) {
        Stringadd(dest, *ip++);
    }
    return 1;
}

int varsub(Program *prog, int sub_warn, int in_expr)
{
    int result = 0;
    const char *start, *contents;
    int bracket, except = FALSE, ell = FALSE, pee = FALSE, star = FALSE, n = -1;
    String *selector;
    String *dest = NULL;
    static int sub_warned = 0;

    (selector = Stringnew(NULL, 0, 0))->links++;

    contents = ip;
    if ((bracket = (*ip == '{'))) ip++;

    if (ip[0] == '#' && (!bracket || ip[1] == '}')) {
        ++ip;
	code_add(prog, in_expr ? OP_PPARM_CNT : OP_APARM_CNT);

    } else if (ip[0] == '?' && (!bracket || ip[1] == '}')) {
        ++ip;
	code_add(prog, in_expr ? OP_PRESULT : OP_ARESULT);

    } else {
        if (is_digit(*ip)) {
            start = ip;
            n = strtoint(ip, &ip);
        } else {
            if ((except = (*ip == '-'))) {
                ++ip;
            }
            start = ip;
            if ((ell = (*ip == 'L')) || (pee = (*ip == 'P')) ||
                (star = (*ip == '*')))
            {
                ++ip;
            }
            if (!star && is_digit(*ip)) {
                n = strtoint(ip, &ip);
            }
        }

        /* This is strange, for backward compatibility.  Some examples:
         * "%{1}x"  == parameter "1" followed by literal "x".
         * "%1x"    == parameter "1" followed by literal "x".
         * "%{1x}"  == bad substitution.
         * "%{L}x"  == parameter "L" followed by literal "x".
         * "%Lx"    == variable "Lx".
         * "%{Lx}"  == variable "Lx".
         * "%{L1}x" == parameter "L1" followed by literal "x".
         * "%L1x"   == parameter "L1" followed by literal "x".
         * "%{L1x}" == variable "L1x".
         */
        Stringtrunc(selector, 0);
        if ((n < 0 && !star) || (bracket && (ell || pee))) {
            /* is non-special, or could be non-special if followed by alnum_ */
            if (is_alnum(*ip) || (*ip == '_')) {
                ell = pee = FALSE;
                n = -1;
                do ip++; while (is_alnum(*ip) || *ip == '_');
                Stringncpy(selector, start, ip - start);
            }
        }

	if (star) {
	    code_add(prog, in_expr ? OP_PPARM_ALL : OP_APARM_ALL);
	} else if (pee) {
	    if (n < 0) n = 1;
	    code_add(prog, in_expr ? OP_PREG : OP_AREG, n);
	    n = -1;
	} else if (ell) {
	    if (n < 0) n = 1;
	    if (except)
		code_add(prog, in_expr ? OP_PLXPARM : OP_ALXPARM, n);
	    else
		code_add(prog, in_expr ? OP_PLPARM : OP_ALPARM, n);
	    except = 0; /* handled */
	} else if (n > 0) {
	    if (except)
		code_add(prog, in_expr ? OP_PXPARM : OP_AXPARM, n);
	    else
		code_add(prog, in_expr ? OP_PPARM : OP_APARM, n);
	    except = 0; /* handled */
	} else if (n == 0) {
	    code_add(prog, in_expr ? OP_PCMDNAME : OP_ACMDNAME);
	} else if (strcmp(selector->data, "R") == 0) {
	    code_add(prog, in_expr ? OP_PPARM_RND : OP_APARM_RND);
	} else if (strcmp(selector->data, "PL") == 0) {
	    code_add(prog, in_expr ? OP_PREG : OP_AREG, -1);
	} else if (strcmp(selector->data, "PR") == 0) {
	    code_add(prog, in_expr ? OP_PREG : OP_AREG, -2);
	} else {
	    Value *val = newid(selector->data, selector->len);
	    if (in_expr) {
		code_add(prog, *ip == '-' ? OP_PVAR : OP_PUSH, val);
	    } else {
		code_add(prog, OP_AVAR, val);
	    }
	}
	if (except) { /* unhandled leading '-' */
            eprintf("illegal character '-' in substitution");
            goto varsub_exit;
	}
    }

    if (*ip == '-') {
	int jump_point;
        ++ip;
	code_add(prog, OP_JNEMPTY, -1);
	jump_point = prog->len - 1;
	if (in_expr) {
	    code_add(prog, OP_POP);
	    code_add(prog, OP_PUSHBUF, NULL);
	}
	(dest = Stringnew(NULL, 0, 0))->links++;
        while (*ip) {
            if (is_end_of_statement(ip) || is_end_of_cmdsub(ip)) {
                break;
            } else if (bracket && *ip == '}') {
                break;
            } else if (!bracket && is_space(*ip)) {
                break;
            } else if (*ip == '%') {
                ++ip;
                if (!percentsub(prog, SUB_FULL, &dest)) goto varsub_exit;
            } else if (*ip == '$') {
                ++ip;
                if (!dollarsub(prog, &dest)) goto varsub_exit;
            } else if (*ip == '/') {
                ++ip;
                if (!slashsub(prog, dest)) goto varsub_exit;
            } else if (*ip == '\\') {
		if (ip[1] == '\n') {
		    eat_newline(prog);
		} else {
		    ++ip;
		    if (!backsub(prog, dest)) goto varsub_exit;
		}
            } else {
                for (start = ip++; *ip && is_alnum(*ip); ip++);
		SStringoncat(dest, prog->src, start - prog->src->data,
		    ip - start);
            }
        }
	if (dest->len) {
	    code_add(prog, OP_APPEND, dest);
	    dest = NULL;
	}
	if (in_expr) {
	    code_add(prog, OP_PBUF, NULL);
	}
	comefrom(prog, jump_point, prog->len);
    }

    if (bracket) {
        if (*ip != '}') {
            if (!*ip) eprintf("unmatched { or bad substitution");
            else eprintf("unmatched { or illegal character '%c'", *ip);
            goto varsub_exit;
        } else ip++;
    }

    result = 1;
    if (sub_warn & (!sub_warned || pedantic)) {
        sub_warned = 1;
        wprintf("\"%%%.*s\" substitution in expression is legal, "
	    "but can be confusing.  Try using \"{%.*s}\" instead.",
            ip-contents, contents,
            (ip-bracket)-(contents+bracket), contents+bracket);
    }
varsub_exit:
    if (dest) Stringfree(dest);
    Stringfree(selector);
    return result;
}

struct Value *handle_shift_command(String *args, int offset)
{
    int count;
    int error;

    count = (args->len - offset) ? atoi(args->data + offset) : 1;
    if (count < 0) return shareval(val_zero);
    if ((error = (count > tf_argc))) count = tf_argc;
    tf_argc -= count;
    if (tf_argv) {  /* true if macro was called as command, not as function */
        tf_argv += count;
    }
    return newint(!error);
}

#if USE_DMALLOC
void free_expand()
{
    freeval(user_result);
    freeval(val_blank);
    freeval(val_one);
    freeval(val_zero);
}
#endif

