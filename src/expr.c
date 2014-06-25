/*************************************************************************
 *  TinyFugue - programmable mud client
 *  Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2002, 2003, 2004, 2005, 2006-2007 Ken Keys
 *
 *  TinyFugue (aka "tf") is protected under the terms of the GNU
 *  General Public License.  See the file "COPYING" for details.
 ************************************************************************/
static const char RCSid[] = "$Id: expr.c,v 35004.179 2007/01/13 23:12:39 kkeys Exp $";


/********************************************************************
 * Fugue expression interpreter
 *
 * Written by Ken Keys
 * Parses and evaluates expressions.
 ********************************************************************/

#include "tfconfig.h"
#include <math.h>
#include <limits.h>
#include "port.h"
#include "tf.h"
#include "util.h"
#include "pattern.h"
#include "search.h"
#include "tfio.h"
#include "macro.h"
#include "signals.h"	/* interrupted() */
#include "socket.h"	/* socktime() */
#include "output.h"	/* igoto() */
#include "attr.h"
#include "keyboard.h"	/* do_kb*() */
#include "parse.h"
#include "expand.h"
#include "expr.h"
#include "cmdlist.h"
#include "command.h"
#include "variable.h"
#include "tty.h"	/* no_tty */
#include "history.h"	/* log_count */
#include "world.h"	/* new_world() */


#define STACKSIZE 512

int stacktop = 0;
Value *stack[STACKSIZE];
const int feature_float = !(NO_FLOAT-0);

static Value *valpool = NULL;		/* freelist */


/* dup string operand */
#define opdstrdup(N) Stringdup(opdstr(N))

typedef struct ExprFunc {
    const char *name;		/* name invoked by user */
    unsigned min, max;		/* allowable argument counts */
} ExprFunc;

static ExprFunc functab[] = {
#define funccode(name, pure, min, max)  { #name, min, max }
#include "funclist.h"
#undef funccode
};

enum func_id {
#define funccode(name, pure, min, max)  FN_##name
#include "funclist.h"
#undef funccode
};


static int comma_expr(Program *prog);
static int assignment_expr(Program *prog);
static int conditional_expr(Program *prog);
static int or_expr(Program *prog);
static int and_expr(Program *prog);
static int relational_expr(Program *prog);
static int additive_expr(Program *prog);
static int multiplicative_expr(Program *prog);
static int unary_expr(Program *prog, int could_be_div_or_macro);
static int primary_expr(Program *prog, int could_be_div_or_macro);
static int reduce_arithmetic(opcode_t op, const Value *val0, int n, Value *res);
static Value *function_switch(const ExprFunc *func, int n, const char *parent);
static Value *do_function(int n);


int expr(Program *prog)
{
    return comma_expr(prog);
}

/* Returns the value of expression.  Caller must freeval() the value. */
Value *expr_value(const char *expression)
{
    Program *prog;
    Value *result;
    conString *str;

    if (!expression) return shareval(val_blank);
    str = CS(Stringnew(expression, -1, 0)); /* XXX String should be a param */
    if (!(prog = compile_tf(str, 0, -1, 1, 0))) return NULL;
    result = prog_interpret(prog, 1);
    prog_free(prog);
    return result;
}

Value *expr_value_safe(Program *prog)
{
    return prog_interpret(prog, 1);
}


#if !NO_FLOAT
Value *newfloat_fl(double f, const char *file, int line)
{
    Value *val;

    palloc(val, Value, valpool, u.next, file, line);
    val->count = 1;
    val->type = TYPE_FLOAT;
    val->u.fval = f;
    val->name = NULL;
    val->sval = NULL;
    return val;
}
#endif /* NO_FLOAT */

inline Value *newval_fl(const char *file, int line)
{
    Value *val;

    palloc(val, Value, valpool, u.next, file, line);
    val->count = 1;
    val->name = NULL;
    val->sval = NULL;
    return val;
}

Value *newint_fl(long i, const char *file, int line)
{
    Value *val;

    val = newval_fl(file, line);
    val->type = TYPE_INT;
    val->u.ival = i;
    return val;
}

Value *newtime_fl(long s, long u, type_t type, const char *file, int line)
{
    Value *val;

    val = newval_fl(file, line);
    val->type = type;
    val->u.tval.tv_sec = s;
    val->u.tval.tv_usec = u;
    return val;
}

Value *newstr_fl(const char *data, int len, const char *file, int line)
{
    Value *val;

    val = newval_fl(file, line);
    val->type = TYPE_STR;
    (val->sval = CS(Stringnew(data, len, 0)))->links++;
    val->u.p = NULL;
    return val;
}

/* newSstr shares argument; caller must Stringdup() if needed. */
Value *newSstr_fl(conString *str, const char *file, int line)
{
    Value *val;

    val = newval_fl(file, line);
    val->type = TYPE_STR;
    (val->sval = str)->links++;
    val->u.p = NULL;
    return val;
}

Value *newid_fl(const char *id, int len, const char *file, int line)
{
    Value *val;
    char *new;

    val = newval_fl(file, line);
    val->type = TYPE_ID;
    new = strncpy((char *)xmalloc(NULL, len + 1, file, line), id, len);
    new[len] = '\0';
    val->name = new;
    val->u.hash = hash_string(new);	/* cache hashkey to speed lookup */
    return val;
}

/* Push a void pointer. */
Value *newptr_fl(void *ptr, const char *file, int line)
{
    Value *val;

    val = newval_fl(file, line);
    val->type = TYPE_FILE;
    val->u.p = ptr;
    return val;
}

/* If val is an ID, create a copy of its value, free the ID, return the copy;
 * otherwise, just return val.
 */
Value *valval_fl(Value *val, const char *file, int line)
{
    Value *result;
    struct timeval tv;

    if (val->type != TYPE_ID)
	return val;
    if (!(result = hgetnearestvarval(val))) {
	result = newSstr_fl(blankline, file, line);
    } else {
	switch (result->type & TYPES_BASIC) {
	case TYPE_ENUM:
	case TYPE_POS:
	case TYPE_INT:
	    result = newint_fl(result->u.ival, file, line); break;
	case TYPE_DECIMAL:
	case TYPE_DTIME:
	case TYPE_ATIME:
	    if (valtime(&tv, result))
		result = newtime_fl(tv.tv_sec, tv.tv_usec, result->type,
		    file, line);
	    else
		result = NULL;
	    break;
	case TYPE_FLOAT:
	    result = newfloat_fl(valfloat(result), file, line); break;
	default:
	    result = newSstr_fl(valstr(result), file, line); break;
	}
    }
    freeval(val);
    return result;
}

/* free value of val (but not its name) */
void clearval_fl(Value *val, const char *file, int line)
{
    if (val->sval) {
        conStringfree_fl(val->sval, file, line);
	val->sval = NULL;
    }

    if (val->type & TYPE_REGEX) {
	tf_reg_free(val->u.ri);
    }
    if (val->type & TYPE_EXPR) {
	prog_free(val->u.prog);
    }
    val->type &= TYPES_BASIC;

    val->u.ival = 0;
}

void freeval_fl(Value *val, const char *file, int line)
{
    if (!val) return;
    if (--val->count > 0) return;
    assert(val->count == 0); 
    clearval_fl(val, file, line);
    if (val->name) {
        xfree(NULL, (void*)val->name, file, line);
	val->name = NULL;
    }
    pfree_fl(val, valpool, u.next, file, line);
}

/* Return numeric value of val (converting ID or STR if needed) */
static const Value *valnum(const Value *val)
{
    static Value parsed[1]; /* NB: may return pointer to this */

    if (!val) return NULL;
    if (val->type == TYPE_ID)
        if (!(val = hgetnearestvarval(val)))
            return NULL;

    if (val->type & TYPE_STR) {
        if (!val->sval) return NULL;
        if (!parsenumber(val->sval->data, NULL, TYPE_NUM, parsed)) {
#if 0
            if (pedantic)
                wprintf("%s",
                    "non-numeric string value used in numeric context");
#endif
            return NULL;
        }
	val = parsed;
    }
    return val;
}

/* return boolean value of item */
int valbool(const Value *val)
{
    if (!(val = valnum(val)))
	return 0;

    if (val->type & (TYPE_INT | TYPE_POS | TYPE_ENUM))
        return !!val->u.ival;
    if (val->type & (TYPE_DECIMAL|TYPE_DTIME|TYPE_ATIME))
        return (val->u.tval.tv_sec || val->u.tval.tv_usec);
#if !NO_FLOAT
    else if (val->type & TYPE_FLOAT)
        return !!val->u.fval;
#endif
    return 0;
}

/* return integer value of item */
long valint(const Value *val)
{
    if (!(val = valnum(val)))
	return 0;

    if (val->type & (TYPE_INT | TYPE_POS | TYPE_ENUM))
        return val->u.ival;
    if (val->type & (TYPE_DECIMAL|TYPE_DTIME|TYPE_ATIME))
        return (long)val->u.tval.tv_sec;
#if !NO_FLOAT
    else if (val->type & TYPE_FLOAT) {
        double fival = val->u.fval < 0 ? ceil(val->u.fval) : floor(val->u.fval);
        if (fival != (long)val->u.fval) {
	    eprintf("real value too large to convert to integer");
	}
        return (long)val->u.fval;
    }
#endif
    return 0;
}

/* copy timeval of item */
int valtime(struct timeval *tvp, const Value *val)
{
    if (!(val = valnum(val)))
	goto valtime_error;

    /* *tvp and val->u might be aliases, so we can't write to *tvp if we still
     * need to read from val->u. */

    if (val->type & (TYPE_INT | TYPE_POS | TYPE_ENUM)) {
        tvp->tv_sec = val->u.ival;
        tvp->tv_usec = 0;
    } else if (val->type & (TYPE_DECIMAL|TYPE_DTIME|TYPE_ATIME)) {
        *tvp = val->u.tval;
#if !NO_FLOAT
    } else if (val->type & TYPE_FLOAT) {
        long ival = (long)val->u.fval;
        double fival = val->u.fval < 0 ? ceil(val->u.fval) : floor(val->u.fval);
        if (fival != ival) {
	    eprintf("real value too large to convert to time");
	    goto valtime_error;
	}
        tvp->tv_usec = (long)((val->u.fval - ival) * 1000000);
        tvp->tv_sec = ival;
#endif
    }
    return 1;

valtime_error:
    *tvp = tvzero;
    return 0;
}

#if !NO_FLOAT
/* return floating value of item */
double valfloat(const Value *val)
{
    if (!val) return 0.0;
    if (val->type == TYPE_ID) {
        if (!(val = hgetnearestvarval(val)))
            return 0.0;
    }
    errno = 0;
    if (val->type & TYPE_FLOAT)
        return val->u.fval;
    if (val->type & (TYPE_INT | TYPE_POS | TYPE_ENUM))
        return (double)val->u.ival;
    if (val->type & (TYPE_DECIMAL|TYPE_DTIME|TYPE_ATIME))
        return (double)val->u.tval.tv_sec + val->u.tval.tv_usec / 1000000.0;
    return val->sval ? strtod(val->sval->data, NULL) : 0.0;
}
#endif /* NO_FLOAT */

/* return String value of item (only valid for lifetime of val!) */
/* If C had "mutable", sval could be mutable, and val could be const */
conString *valstr(Value *val)
{
    const char *p;
    String *sval;
    long sec, usec;

    if (!val) return NULL;
    if (val->type == TYPE_ID) {
        if (!(val = hgetnearestvarval(val)))
            return blankline;
    }

    /* use existing sval (but floats may be affected by %sigfigs) */
    if (val->sval && val->type != TYPE_FLOAT)
        return val->sval;

    /* generate and cache new sval */
    switch (val->type & TYPES_BASIC) {
        case TYPE_STR:
        case TYPE_ENUM:
            val->sval = blankline;
            break;
        case TYPE_INT:
        case TYPE_POS:
            sval = Stringnew(NULL, 0, 0);
            Sprintf(sval, "%ld", val->u.ival);
            val->sval = CS(sval);
            break;
        case TYPE_DECIMAL:
        case TYPE_ATIME:
	    /* s.u format */
            sval = Stringnew(NULL, 0, 0);
	    tftime(sval, NULL, &val->u.tval);
            val->sval = CS(sval);
            break;
        case TYPE_DTIME:
            sval = Stringnew(NULL, 0, 0);
            sec = val->u.tval.tv_sec;
	    if (sec >= 60 || sec <= -60) {
		/* h:mm:ss.u format */
		usec = val->u.tval.tv_usec;
		if (sec < 0) {
		    Stringadd(sval, '-');
		    sec = -sec;
		    usec = -usec;
		}
		Sappendf(sval, "%ld:%02ld", sec/3600, (sec/60)%60);
		if (sec % 60 || usec)
		    Sappendf(sval, ":%02ld", sec % 60);
		if (usec)
		    append_usec(sval, usec, 1);
	    } else {
		/* s.u format */
		tftime(sval, NULL, &val->u.tval);
	    }
            val->sval = CS(sval);
            break;
#if !NO_FLOAT
        case TYPE_FLOAT:
	    if (val->sval) conStringfree(val->sval);
	    sval = Stringnew(NULL, 0, 0);
            Sprintf(sval, "%.*g", sigfigs, val->u.fval);
            /* note: appending ".0" could imply more precision than is proper */
            for (p = sval->data; is_digit(*p); p++) ;
            if (!*p)
                Stringadd(sval, '.');
            val->sval = CS(sval);
            break;
#endif
        default:
	    internal_error(__FILE__, __LINE__, "valstr: impossible type %d",
		val->type);
	    return NULL;
    }
    val->sval->links++;
    return val->sval;
}

/* return String data (char*) of item (only valid for lifetime of val!) */
const char *valstd(Value *val)
{
    return val ? valstr(val)->data : NULL;
}

/* Return void pointer value of item.  Internal, conversion not needed. */
void *valptr(Value *val)
{
    return val ? val->u.p : NULL;
}

int pushval(Value *val)
{
    if (stacktop == STACKSIZE) {
        eprintf("expression stack overflow");
        freeval(val);
        return 0;
    }
    stack[stacktop++] = val;
    return 1;
}

/* variable's name has already been looked up, so if var is NULL, it does
 * not exist, and assign() creates a new variable with name */
static Value *assign(Var *var, const Value *idval, Value *val)
{
    Value tmpvalue, *result;

    if (!var)
	var = newglobalvar(idval->name);
    if (!val) {
	tmpvalue.type = TYPE_STR;
	tmpvalue.sval = blankline;
	val = &tmpvalue;
    } else if (val->type & TYPE_STR) { /* must copy the String, not share it */
	tmpvalue.type = TYPE_STR;
	tmpvalue.sval = CS(Stringdup(val->sval));
	val = &tmpvalue;
    }
    setvar(var, val, 0);
    palloc(result, Value, valpool, u.next, __FILE__, __LINE__);
    result->name = NULL;
    result->count = 1;
    if (var) {
	if (val->type & TYPE_REGMATCH)
	    var->val.type |= TYPE_REGMATCH;
	result->type = var->val.type;
	if ((result->sval = var->val.sval))
	    result->sval->links++;
	result->u = var->val.u;
    } else {
	result->type = TYPE_STR;
	(result->sval = blankline)->links++;
	result->u.p = NULL;
    }
    return result;
}

/* Pop n operands, apply op to them, and push result */
int reduce(opcode_t op, int n)
{
    Value *val = NULL;
    const Value *val0;
    Value res; /* not a proper Value, just result space for reduce_arithmetic */
    Var *var;
    long i; /* scratch */

    if (stacktop < n) {
	if (oplabel(op))
	    internal_error(__FILE__, __LINE__,
		"stack underflow, op=%s, n=%d", oplabel(op), n);
	else
	    internal_error(__FILE__, __LINE__,
		"stack underflow, op=0x%04X, n=%d", op, n);
        return 0;
    }

    switch (op) {
    case '>':
    case '<':
    case OP_EQUAL:
    case OP_NOTEQ:
    case OP_GTE:
    case OP_LTE:
    case '+':
    case '-':
    case '*':
    case '/':
        if (!reduce_arithmetic(op, opd(n), n, &res))
	    break;
	switch (res.type) {
	case TYPE_FLOAT:
            if (res.u.fval == HUGE_VAL || res.u.fval == -HUGE_VAL) {
		eprintf("%s operator: arithmetic overflow", oplabel(op));
	    } else {
		val = newfloat(res.u.fval);
	    }
	    break;
	case TYPE_DECIMAL:
	case TYPE_ATIME:
	case TYPE_DTIME:
	    val = newtime(res.u.tval.tv_sec, res.u.tval.tv_usec, res.type);
	    break;
	case TYPE_INT:
	    val = newint(res.u.ival); break;
	default:
	    eprintf("impossible result type %d", res.type);
	}
        break;

    case OP_ADDA:
    case OP_SUBA:
    case OP_MULA:
    case OP_DIVA:
	if (opd(n)->type != TYPE_ID) goto reduce_bad_assignment;
	var = hfindnearestvar(opd(n));
	val0 = var ? getvarval(var) : val_zero;
        if (!reduce_arithmetic(op, val0, n, &res))
	    break;
	if (res.type == TYPE_FLOAT &&
            (res.u.fval == HUGE_VAL || res.u.fval == -HUGE_VAL))
	{
	    eprintf("%s operator: arithmetic overflow", oplabel(op));
	} else {
	    val = assign(var, opd(2), &res);
	}
	break;

    case OP_ASSIGN:
	if (opd(n)->type != TYPE_ID) goto reduce_bad_assignment;
	var = hfindnearestvar(opd(2));
	val = opd(1);
	if (val && val->type == TYPE_ID)
	    val = hgetnearestvarval(val);
	if (!(val = assign(var, opd(2), val)))
	    return 0;
	break;

    case OP_PREDEC: if (opd(n)->type != TYPE_ID) goto reduce_bad_assignment;
		    i = valint(getvarval(var = hfindnearestvar(opd(1)))) - 1;
		    if (i == LONG_MAX)
			eprintf("integer overflow in --%s", opd(1)->name);
		    var = setintvar(var ? var : newglobalvar(opd(1)->name),
			i, 0);
                    val = newint(var ? i : 0);
                    break;
    case OP_PREINC: if (opd(n)->type != TYPE_ID) goto reduce_bad_assignment;
		    i = valint(getvarval(var = hfindnearestvar(opd(1)))) + 1;
		    if (i == LONG_MIN)
			eprintf("integer overflow in ++%s", opd(1)->name);
		    var = setintvar(var ? var : newglobalvar(opd(1)->name),
			i, 0);
                    val = newint(var ? i : 0);
                    break;
    case OP_STREQ:  val = newint(strcmp(opdstd(2), opdstd(1)) == 0); break;
    case OP_STRNEQ: val = newint(strcmp(opdstd(2), opdstd(1)) != 0); break;
    case OP_MATCH:  val = newint(smatch_check(opdstd(1)) &&
                        smatch(opdstd(1),opdstd(2))==0);
                    break;
    case OP_NMATCH: val = newint(smatch_check(opdstd(1)) &&
                        smatch(opdstd(1),opdstd(2))!=0);
                    break;
#if 0 /* doesn't work right */
    case '.':	    if (opd(n-0)->count == 1 && (opd(n-0)->type & TYPE_STR))
			(val = opd(n-0))->count++;
		    else
			val = newSstr(opdstr(n-0));
		    SStringcat(val->sval, opdstr(n-1));
                    break;
#endif
    case OP_FUNC:   val = do_function(n);                            break;
    case '!':       val = newint(!opdbool(1));                       break;
    default:        internal_error(__FILE__, __LINE__,
			"internal error: reduce: bad op 0x%04X", op);
                    break;
    }

    goto reduce_exit;
reduce_bad_assignment:
    eprintf("illegal object of assignment");
reduce_exit:
    while (n--) freeval(popval());
    return val ? pushval(val) : 0;
}

/* Fills in *res with a TYPE_INT, TYPE_DECIMAL, TYPE_DTIME, TYPE_ATIME,
 * or TYPE_FLOAT value */
static int reduce_arithmetic(opcode_t op, const Value *val0, int n, Value *res)
{
    int i;
    int int0, int1, neg0, neg1, sum;
    double f;
    struct timeval t, t1;
    type_t type, promoted_type = 0;
    const Value *val[2];
    Value localval[2];

#define resint(i) \
    ((res->type = TYPE_INT), (res->u.ival = (i)), 1)
#define resfloat(f) \
    ((res->type = TYPE_FLOAT), (res->u.fval = (f)), 1)
#define restime(sec, usec, restype) \
    ((res->type = restype), (res->u.tval.tv_sec = (sec)), \
    (res->u.tval.tv_usec = (usec)), 1)

    for (i = 0; i < n; i++) {
	val[i] = (i == 0) ? val0 : opd(n-i);
	if (val[i]->type == TYPE_ID)
	    if (!(val[i] = hgetnearestvarval(val[i])))
		val[i] = val_zero;
	if (val[i]->type & TYPE_STR) {
	    parsenumber(val[i]->sval->data, NULL, TYPE_NUM, &localval[i]);
	    val[i] = &localval[i];
	}
	type = val[i]->type & TYPES_BASIC;
	if (type & (TYPE_ENUM | TYPE_POS))
	    type = TYPE_INT;
	if (type > promoted_type)
	    promoted_type = type;
    }
    if (n == 2 && val[0]->type & TYPE_ATIME && val[1]->type & TYPE_ATIME) {
        switch (op & ~OPF_SIDE) {
        case '+':
        case '*':
        case '/':
	    wprintf("invalid operation %s on absolute time values.",
		oplabel(op));
        default:   break;
        }
	if (op == '-') /* atime - atime => dtime */
	    promoted_type = TYPE_DTIME;
    } else if (n == 1 && val[0]->type == TYPE_ATIME && op == '-') {
	wprintf("invalid operation %s on absolute time value.", oplabel(op));
	promoted_type = TYPE_ATIME;
    }

    if ((op == OP_EQUAL || op == OP_NOTEQ) &&
	((val[0]->type & TYPE_REGMATCH) || (val[1]->type & TYPE_REGMATCH)))
    {
	if ((val[0]->type & TYPE_REGMATCH && valint(val[1]) == 1) ||
	    (val[1]->type & TYPE_REGMATCH && valint(val[0]) == 1) ||
	    ((val[0]->type & TYPE_REGMATCH) && (val[1]->type & TYPE_REGMATCH)))
	{
	    wprintf("regmatch() may return >= 1 for success.");
	}
    }

    if ((promoted_type & (TYPE_INT | TYPE_DECIMAL | TYPE_ATIME | TYPE_DTIME)) &&
	is_additive(op & ~OPF_SIDE))
    {
	/* additive overflow causes promotion to float */
	int0 = (n > 1) ? valint(val[0]) : 0;
	int1 = valint(val[n-1]);
	neg0 = int0 < 0;
	if ((op & ~OPF_SIDE) == '-') {
	    neg1 = int1 >= 0;
	    sum = (int0 - int1);
	} else {
	    neg1 = int1 < 0;
	    sum = (int0 + int1);
	}
	if (neg0 == neg1 && sum<0 != neg0) {
	    /* operands have same sign, but sum has different sign: overflow */
	    promoted_type = TYPE_FLOAT;
	}
    }

    switch (promoted_type) {
    case TYPE_INT:
        switch (op & ~OPF_SIDE) {
        case '>':       return resint(valint(val[0]) > valint(val[1]));
        case '<':       return resint(valint(val[0]) < valint(val[1]));
        case OP_EQUAL:  return resint(valint(val[0]) == valint(val[1]));
        case OP_NOTEQ:  return resint(valint(val[0]) != valint(val[1]));
        case OP_GTE:    return resint(valint(val[0]) >= valint(val[1]));
        case OP_LTE:    return resint(valint(val[0]) <= valint(val[1]));
        case '+':	/* fall thru to '-' */
        case '-':	return resint(sum);
        case '*':       i = valint(val[0]) * valint(val[1]);
			f = valfloat(val[0]) * valfloat(val[1]);
			return (i == f) ? resint(i) : resfloat(f);
        case '/':       if ((i = valint(val[1])) != 0)
                            return resint(valint(val[0]) / i);
                        eprintf("division by zero");
                        return 0;
        default:        return 0;
        }

    case TYPE_DECIMAL:
    case TYPE_ATIME:
    case TYPE_DTIME:
        if (!valtime(&t1, val[n-1])) return 0;
        if (n == 1)
            t = tvzero;
        else
            if (!valtime(&t, val[0])) return 0;

        switch (op & ~OPF_SIDE) {
        case '+':  tvadd(&t, &t, &t1);
		   return restime(t.tv_sec, t.tv_usec, promoted_type);
        case '*':  return resfloat(valfloat(val[0]) * valfloat(val[1]));
        case '/':  return resfloat(valfloat(val[0]) / valfloat(val[1]));
        default:   break;
        }

        tvsub(&t, &t, &t1);

        switch (op & ~OPF_SIDE) {
        case '>':      return resint(t.tv_sec ? t.tv_sec > 0 : t.tv_usec > 0);
        case '<':      return resint(t.tv_sec ? t.tv_sec < 0 : t.tv_usec < 0);
        case OP_EQUAL: return resint(!t.tv_sec && !t.tv_usec);
        case OP_NOTEQ: return resint(t.tv_sec || t.tv_usec);
        case OP_GTE:   return resint(t.tv_sec ? t.tv_sec >=0 : t.tv_usec >=0);
        case OP_LTE:   return resint(t.tv_sec ? t.tv_sec <=0 : t.tv_usec <=0);
        case '-':      return restime(t.tv_sec, t.tv_usec, promoted_type);
        default:       return 0;
        }

    case TYPE_FLOAT:
	f = valfloat(val[0]);
	switch (op & ~OPF_SIDE) {
	case '>':       return resint(f > valfloat(val[1]));
	case '<':       return resint(f < valfloat(val[1]));
	case OP_EQUAL:  return resint(f == valfloat(val[1]));
	case OP_NOTEQ:  return resint(f != valfloat(val[1]));
	case OP_GTE:    return resint(f >= valfloat(val[1]));
	case OP_LTE:    return resint(f <= valfloat(val[1]));
	case '+':       return resfloat((n>1) ? f + valfloat(val[1]) : +f);
	case '-':       return resfloat((n>1) ? f - valfloat(val[1]) : -f);
	case '*':       return resfloat(f * valfloat(val[1]));
	case '/':       return resfloat(f / valfloat(val[1]));
	default:        return 0;
	}

    default:
        internal_error(__FILE__, __LINE__,
            "impossible type %d in reduce_arithmetic", promoted_type);
	return 0;  /* impossible */
    }

#undef resint
#undef resfloat
#undef restime
}

static Value *function_switch(const ExprFunc *func, int n, const char *parent)
{
    int oldblock;
    long i, j;
    char c;
    const char *str, *ptr;
    String *Sstr, *Sstr2;
    conString *constr;
    FILE *file;
    TFILE *tfile;
    struct timeval tv;
    const struct timeval *then;
    ValueUnion uval;
    int type;

    int symbol = func - functab;

#ifdef __CYGWIN32__
    STATIC_STRING(systype, "cygwin32", 0);
#else
# ifdef PLATFORM_UNIX
    STATIC_STRING(systype, "unix", 0);
# else
#  ifdef PLATFORM_OS2
    STATIC_STRING(systype, "os/2", 0);
#  else
    STATIC_STRING(systype, "unknown", 0);
#  endif
# endif
#endif

        switch (symbol) {

        case FN_test:
	    return expr_value(opdstd(1));

        case FN_addworld:
    /* addworld(name, type, host, port, char, pass, file, flags, srchost) */
	  {
	    int flags = 0;

            if (restriction >= RESTRICT_WORLD) {
                eprintf("restricted");
                return shareval(val_zero);
            }

            if (n > 7) {
                for (str = opdstd(n-7); *str; str++) {
		    switch (*str) {
		    case 'x': flags |= WORLD_SSL; break;
		    case 'p': flags |= WORLD_NOPROXY; break;
		    case 'e': flags |= WORLD_ECHO; break;
		    /* compatibility with old use_proxy */
		    case 'f':
		    case '0': flags |= WORLD_NOPROXY; break;
		    case 'o':
		    case 'n':
		    case '1': /* ignore */; break;
		    default:
			eprintf("invalid flag %c", *str);
			return shareval(val_zero);
		    }
		}
            }

            return newint(!!new_world(
                opdstd(n-0),                /* name */
                opdstd(n-1),                /* type */
                n>2 ? opdstd(n-2) : "",     /* host */
                n>3 ? opdstd(n-3) : "",     /* port */
                n>4 ? opdstd(n-4) : "",     /* char */
                n>5 ? opdstd(n-5) : "",     /* pass */
                n>6 ? opdstd(n-6) : "",     /* mfile */
                flags,			    /* flags */
		n>8 ? opdstd(n-8) : ""));   /* srchost */
	  }

        case FN_columns:
            return newint(columns);

        case FN_lines:
            return newint(lines);

        case FN_winlines:
            return newint(winlines());

        case FN_encode_ansi:
	    return newSstr(CS(encode_ansi(opdstr(n-0), 0)));

        case FN_decode_ansi:
	    constr = CS(decode_ansi(opdstd(n), 0, EMUL_ANSI_ATTR, NULL));
	    return constr ? newSstr(constr) : shareval(val_blank);

        case FN_encode_attr:
	    return newSstr(CS(encode_attr(opdstr(n-0), 0)));

        case FN_decode_attr:
            {
		attr_t attr = 0;
		if (n > 1) {
		    if (!parse_attrs(opdstd(n-1), &attr, 0))
			return shareval(val_blank);
		}
		i = (n>2) ?
		    enum2int(opdstd(n-2), 0, enum_flag, "arg 3 (inline)") : 1;
		constr = CS(i ? decode_attr(opdstr(n), 0, 0) :
		    Stringdup(opdstr(n)));
		constr->attrs = adj_attr(constr->attrs, attr);
		return constr ? newSstr(constr) : shareval(val_blank);
            }

        case FN_strip_attr:
	    constr = opdstr(n-0);
	    return newstr(constr->data, constr->len);

        case FN_status_width:
	    return newint(handle_status_width_func(opdstd(n-0)));

        case FN_status_fields:
	    return newSstr(status_field_text(n>0 ? opdint(n-0) : 0));

        case FN_echo:
            i = (n>2) ?
		enum2int(opdstd(n-2), 0, enum_flag, "arg 3 (inline)") : 0;
            if (i < 0) return shareval(val_zero);
            return newint(handle_echo_func(opdstr(n),
                (n >= 2) ? opdstd(n-1) : "",
                i, (n >= 4) ? opdstd(n-3) : "o"));

        case FN_substitute:
            i = (n>2) ?
		enum2int(opdstd(n-2), 0, enum_flag, "arg 3 (inline)") : 0;
            if (i < 0) return shareval(val_zero);
            return newint(handle_substitute_func(opdstr(n),
                (n >= 2) ? opdstd(n-1) : "",  i));

        case FN_prompt:
            return newint(handle_prompt_func(opdstr(n)));

        case FN_eval:
	    i = SUB_MACRO;
	    if (n>1)
		if ((i = enum2int(opdstd(n-1), 0, enum_sub, "arg 2 (sub)")) < 0)
		    return shareval(val_zero);
	    if (!macro_run(opdstr(n-0), 0, NULL, 0, i, "\bEVAL"))
		return shareval(val_zero);
	    return_user_result();

        case FN_send:
            i = handle_send_function(opdstr(n), (n>1 ? opdstd(n-1) : NULL), 
		(n>2 ? opdstd(n-2) : ""));
            return newint(i);

        case FN_fake_recv:
            i = handle_fake_recv_function(opdstr(n),
		(n>1 ? opdstd(n-1) : NULL), (n>2 ? opdstd(n-2) : ""));
            return newint(i);

        case FN_fwrite:
            ptr = opdstd(2);
            file = fopen(expand_filename(ptr), "a");
            if (!file) {
                eprintf("%S: %s", opdstr(2), strerror(errno));
                return shareval(val_zero);
            }
            fputs(opdstd(1), file);
            fputc('\n', file);
            fclose(file);
            return shareval(val_one);

        case FN_tfopen:
            return newint(handle_tfopen_func(
                n<2 ? "" : opdstd(2), n<1 ? "q" : opdstd(1)));

        case FN_tfclose:
            str = opdstd(1);
            if (!str[1]) {
                switch(lcase(str[0])) {
                case 'i':  tfin = NULL;  return shareval(val_zero);
                case 'o':  tfout = NULL; return shareval(val_zero);
                case 'e':  eprintf("tferr can not be closed.");
                           return newint(-1);
                default:   break;
                }
            }
            tfile = find_tfile(str);
            return newint(tfile ? tfclose(tfile) : -1);

        case FN_tfwrite:
            tfile = (n > 1) ? find_usable_tfile(opdstd(2), S_IWUSR) : tfout;
            if (!tfile) return newint(-1);
            Sstr = opdstrdup(1); /* XXX optimize */
            tfputline(CS(Sstr), tfile);
            return shareval(val_one);

        case FN_tfreadable:
            tfile = find_usable_tfile(opdstd(1), S_IRUSR);
	    return newint(tfreadable(tfile));

        case FN_tfread:
            tfile = (n > 1) ? find_usable_tfile(opdstd(2), S_IRUSR) : tfin;
            if (!tfile) return newint(-1);
            if (opd(1)->type != TYPE_ID) {
                eprintf("arg %d: illegal object of assignment", n);
                return newint(-1);
            }
            oldblock = block;  /* condition and evalflag are already correct */
            block = 0;
            j = -1;
            (Sstr = Stringnew(NULL, -1, 0))->links++;
            if (tfgetS(Sstr, tfile)) {
                if (hsetnearestvar(opd(1), CS(Sstr)))
                    j = Sstr->len;
            }
            Stringfree(Sstr);
            block = oldblock;
            return newint(j);

        case FN_tfflush:
            tfile = find_usable_tfile(opdstd(n), S_IWUSR);
            if (!tfile) return newint(-1);
            if (n > 1) {
                if ((i = enum2int(opdstd(1), 0, enum_flag, "argument 2")) < 0)
                    return shareval(val_zero);
                tfile->autoflush = i;
            } else {
                tfflush(tfile);
            }
            return shareval(val_one);

        case FN_ascii:
            return newint((0x100 + unmapchar(*opdstd(1))) & 0xFF);

        case FN_char:
            c = mapchar(localize(opdint(1)));
            return newstr(&c, 1);

        case FN_keycode:
            constr = opdstr(1);
            ptr = get_keycode(constr->data);
            if (ptr) return newstr(ptr, -1);
            eprintf("unknown key name \"%S\"", constr);
            return shareval(val_blank);

        case FN_mod:
            if ((i = opdint(1)) == 0) {
                eprintf("division by zero");
                return NULL;
            }
            return newint(opdint(2) % i);

        case FN_morepaused:
	  {
	    World *world;
	    Screen *screen;
            world = (n>=1 && *opdstd(1)) ? find_world(opdstd(1)) : xworld();
	    screen = (virtscreen && world) ? world->screen : display_screen;
	    return newint(screen->paused);
	  }

        case FN_moresize:
	  {
	    int lim_only = 0, new_only = 0, include_A = 0;
	    int nnew, nback;
	    World *world;
	    Screen *screen;
	    if (n > 0 && (str = opdstd(n-0))) {
		for ( ; *str; str++) {
		    if (lcase(*str) == 'l') lim_only++;
		    else if (lcase(*str) == 'n') new_only++;
		    else if (lcase(*str) == 'a') include_A++;
		    else {
			eprintf("illegal flag '%c'", *str);
			return newint(0);
		    }
		}
	    }
            world = (n>=2 && *opdstd(n-1)) ? find_world(opdstd(n-1)) : xworld();
	    screen = (virtscreen && world) ? world->screen : display_screen;
	    if (!screen) /* screen hasn't been created yet */
		return newint(0);
	    nnew = lim_only ? screen->nnew_filtered : screen->nnew;
	    nback = lim_only ? screen->nback_filtered : screen->nback;
	    if (screen == display_screen || screen->active || include_A ||
		(!new_only && nback > nnew))
		    return newint(new_only ? nnew : nback);
	    else
		return newint(0);
	  }

        case FN_morescroll:
            return newint(clear_more(opdint(1)));

#if !NO_FLOAT
        case FN_sqrt:
            return newfloat(sqrt(opdfloat(1)));

        case FN_sin:
            return newfloat(sin(opdfloat(1)));

        case FN_cos:
            return newfloat(cos(opdfloat(1)));

        case FN_tan:
            return newfloat(tan(opdfloat(1)));

        case FN_asin:
            return newfloat(asin(opdfloat(1)));

        case FN_acos:
            return newfloat(acos(opdfloat(1)));

        case FN_atan:
            return newfloat(atan(opdfloat(1)));

        case FN_exp:
            return newfloat(exp(opdfloat(1)));

        case FN_ln:
            return newfloat(log(opdfloat(1)));

        case FN_log10:
            return newfloat(log10(opdfloat(1)));

        case FN_pow:
            return newfloat(pow(opdfloat(2), opdfloat(1)));

        case FN_trunc:
            return newint(opdint(1));

        case FN_abs:
	  {
	    const Value *val = valnum(opd(1));
	    return (!val) ? shareval(val_zero) : (val->type & TYPE_INT) ?
                newint(labs(valint(val))) : newfloat(fabs(valfloat(val)));
	  }
#else
        case FN_abs:
            return newint(abs(opdint(1)));
#endif /* NO_FLOAT */

        case FN_rand:
            if (n == 0) return newint(RAND());
            i = (n==1) ? 0 : opdint(2);
            if (i < 0) i = 0;
            j = opdint(1) - (n==1);
            return newint((j > i) ? RRAND(i, j) : i);

        case FN_isatty:
            return newint(!no_tty);

        case FN_ftime:
            Sstr = Stringnew(NULL, 0, 0);
            if (n < 2)
		gettime(&tv);
	    else if (!opdtime(&tv, n-1))
		return shareval(val_blank);
            tftime(Sstr, n>0 ? opdstr(n) : blankline, &tv);
            return newSstr(CS(Sstr));

        case FN_time:
            gettime(&tv);
            return newatime(tv.tv_sec, tv.tv_usec);

        case FN_cputime:
	    {
		clock_t t;
		t = clock();
		return t == -1 ? newint(-1) :
		    newfloat(t / (double)CLOCKS_PER_SEC);
	    }

        case FN_idle:
        case FN_sidle:
            if (symbol == FN_sidle)
                then = socktime(n > 0 ? opdstd(1) : "", SOCK_SEND);
            else if (n > 0)
                then = socktime(opdstd(1), SOCK_RECV);
            else
                then = &keyboard_time;
            if (!then) return newdtime(0, 0);
            gettime(&tv);
            tvsub(&tv, &tv, then);
            return newdtime(tv.tv_sec, tv.tv_usec);

        case FN_mktime:
	    {
		struct tm tm;
		time_t t;
		unsigned int usec = 0;
		tm.tm_sec  = 0;
		tm.tm_min  = 0;
		tm.tm_hour = 0;
		tm.tm_mday = 1;
		tm.tm_mon  = 0;
		tm.tm_year = 0;
		tm.tm_isdst = -1;
		switch (n) {
		    case 7: usec       = opdint(n-6);
		    case 6: tm.tm_sec  = opdint(n-5);
		    case 5: tm.tm_min  = opdint(n-4);
		    case 4: tm.tm_hour = opdint(n-3);
		    case 3: tm.tm_mday = opdint(n-2);
		    case 2: tm.tm_mon  = opdint(n-1) - 1;
		    case 1: tm.tm_year = opdint(n-0) - 1900;
		}
		t = mktime(&tm);
		if (t == -1 || usec < 0 || usec > 999999)
		    return newatime(-1, 0);
		if (t < 0 && usec > 0) {
		    t += 1;
		    usec -= 1000000;
		}
		return newatime(t, usec);
	    }

        case FN_filename:
            str = expand_filename(opdstd(1));
            return newstr(str, -1);

        case FN_fg_world:
            return ((str=fgname())) ? newstr(str, -1) : shareval(val_blank);

        case FN_world_info:
            ptr = n>=1 ? opdstd(1) : NULL;
            str = world_info(n>=2 ? opdstd(2) : NULL, ptr);
            if (!str) {
                str = "";
                eprintf("illegal field name '%s'", ptr);
            }
            return newstr(str, -1);

        case FN_is_connected:
            return newint(is_connected(n>0 ? opdstd(1) : ""));

        case FN_is_open:
            return newint(is_open(n>0 ? opdstd(1) : ""));

        case FN_getpid:
            return newint((long)getpid());

        case FN_regmatch:
	  {
	    Value *val;
            (Sstr = opdstrdup(1))->links++;	/* XXX not needed if no match */
	    /* test for type ==, not &; it must not have other extentions */
            i = regmatch_in_scope(opd(2)->type == TYPE_STR ? opd(2) : NULL,
		opdstd(2), Sstr);
	    Stringfree(Sstr);
	    val = newint(i);
	    val->type |= TYPE_REGMATCH;
            return val;
	  }

        case FN_strcat:
            Sstr = opdstrdup(n);
            for (n--; n; n--)
                SStringcat(Sstr, opdstr(n));
            return newSstr(CS(Sstr));

        case FN_strrep:
            constr = opdstr(2);
            i = opdint(1);
	    if (i < 0) i = 0;
            for (Sstr2 = Stringnew(NULL, constr->len * i, 0); i > 0; i--)
                SStringcat(Sstr2, constr);
            return newSstr(CS(Sstr2));

        case FN_pad:
            for (Sstr2 = Stringnew(NULL, 0, 0); n > 0; n -= 2) {
                constr = opdstr(n);
                i = (n > 1) ? opdint(n-1) : 0;
		if (i > constr->len) Stringnadd(Sstr2, ' ', i - constr->len);
		SStringcat(Sstr2, constr);
		if (-i > constr->len) Stringnadd(Sstr2, ' ', -i - constr->len);
            }
            return newSstr(CS(Sstr2));

        case FN_strcmp:
            return newint(strcmp(opdstd(2), opdstd(1)));

        case FN_strcmpattr:
            return newint(Stringcmp(opdstr(2), opdstr(1)));

        case FN_strncmp:
            return newint(strncmp(opdstd(3), opdstd(2), opdint(1)));

        case FN_strlen:
            constr = opdstr(1);
            return newint(constr->len);


#define bound_check(var, maxval) \
    do { \
	if ((var) > (maxval)) { \
	    (var) = (maxval); \
	} else if ((var) < 0) { \
	    (var) = (maxval) + (var); \
	    if ((var) < 0) (var) = 0; \
	} \
    } while (0)

#define optional_int_arg(var, argc, argnum, maxval, defaultval) \
    do { \
	if ((argc) >= argnum) { \
	    (var) = opdint((argc) - (argnum-1)); \
	    bound_check((var), (maxval)); \
	} else { \
	    (var) = (defaultval); \
	} \
    } while (0)

        case FN_substr:
            constr = opdstr(n);
            i = opdint(n - 1);
	    bound_check(i, constr->len);
	    optional_int_arg(j, n, 3, constr->len - i, constr->len - i);
            Sstr2 = Stringnew(NULL, j, 0);
            return newSstr(CS(SStringoncat(Sstr2, constr, i, j)));

        case FN_strstr:
            constr = opdstr(n);
	    optional_int_arg(j, n, 3, constr->len, 0);
            ptr = strstr(constr->data + j, opdstd(n-1));
            return newint(ptr ? (ptr - constr->data) : -1);

        case FN_strchr:
            constr = opdstr(n);
	    optional_int_arg(j, n, 3, constr->len, 0);
            i = strcspn(constr->data + j, opdstd(n-1));
            return newint(constr->data[i+j] ? i+j : -1);

        case FN_strrchr:
            constr = opdstr(n);
            ptr = opdstd(n-1);
	    optional_int_arg(i, n, 3, constr->len - 1, constr->len - 1);
            for ( ; i >= 0; i--)
		if (strchr(ptr, constr->data[i]))
		    return newint(i);
            return newint(-1);

        case FN_replace: {
	    conString *old, *new;
	    const char *start, *next;
	    old = opdstr(3);
	    new = opdstr(2);
	    constr = opdstr(1);
	    Sstr2 = Stringnew(NULL, -1, constr->attrs);
	    start = constr->data;
	    while (old->len > 0 && (next = strstr(start, old->data))) {
		SStringoncat(Sstr2, constr, start - constr->data, next - start);
		SStringcat(Sstr2, new);
		start = next + old->len;
	    }
	    SStringocat(Sstr2, constr, start - constr->data);
	    return newSstr(CS(Sstr2));
	  }

        case FN_tolower:
            Sstr2 = opdstrdup(n);
	    optional_int_arg(j, n, 2, Sstr2->len, Sstr2->len);
            for (i = 0; i < j; i++)
                Sstr2->data[i] = lcase(Sstr2->data[i]);
            return newSstr(CS(Sstr2));

        case FN_toupper:
            Sstr2 = opdstrdup(n);
	    optional_int_arg(j, n, 2, Sstr2->len, Sstr2->len);
            for (i = 0; i < j; i++)
                Sstr2->data[i] = ucase(Sstr2->data[i]);
            return newSstr(CS(Sstr2));

        case FN_kbhead:
            return newstr(keybuf->data, keyboard_pos);

        case FN_kbtail:
            return newstr(keybuf->data + keyboard_pos, keybuf->len - keyboard_pos);

        case FN_kbpoint:
            return newint(keyboard_pos);

        case FN_kbgoto:
            return newint(igoto(opdint(1)));

        case FN_kbdel:
            return (newint(do_kbdel(opdint(1))));

        case FN_kbmatch:
            return newint(do_kbmatch(n>0 ? opdint(1) : keyboard_pos));

        case FN_kbwordleft:
            return newint(do_kbword(n>0 ? opdint(1) : keyboard_pos, -1));

        case FN_kbwordright:
            return newint(do_kbword(n>0 ? opdint(1) : keyboard_pos, 1));

        case FN_kblen:
            return newint(keybuf->len);

        case FN_gethostname:
	{
	    char buf[1024] = "";
#ifdef HAVE_GETHOSTNAME
	    gethostname(buf, sizeof(buf) - 1);
#endif
	    buf[sizeof(buf)-1] = '\0'; /* gethostname() might not terminate */
	    return newstr(buf, -1);
	}

        case FN_getopts: {
            char name[] = "opt_?";
            int offset;
            current_command = parent;
            if (!tf_argv) {
                eprintf("getopts may only be called from a command.");
                return shareval(val_zero);
            }
            str = opdstd(n);

            if (n>1) {
                conString *init = opdstr(n-1);
                for (ptr = str; *ptr; ptr++) {
                    if (!is_alpha(*ptr)) {
                        eprintf("%s: invalid option specifier: %c",
                            func->name, *ptr);
                        return shareval(val_zero);
                    }
                    name[4] = *ptr;
                    setlocalstrvar(name, CS(Stringdup(init)));
                    if (ptr[1] == ':' || ptr[1] == '#' || ptr[1] == '@')
			ptr++;
                }
            }

            if (!tf_argc) return shareval(val_one);
            offset = tf_argv[0].start;
            startopt(argstring, str);
            while ((c = nextopt(&ptr, &uval, &type, &offset))) {
                if (!is_alpha(c))
		    return shareval(val_zero);
		name[4] = c;
		switch (type) {
		case TYPE_STR:
		    setlocalstrvar(name, CS(Stringnew(ptr, -1, 0)));
		    break;
		case TYPE_INT:
		    setlocalintvar(name, uval.ival);
		    break;
		case TYPE_DTIME:
		    setlocaldtimevar(name, &uval.tval);
		    break;
		default:
		    setlocalintvar(name, 1);
		    break;
		}
            }
            while (tf_argc > 0 && offset >= tf_argv[0].end) {
                tf_argv++;
                tf_argc--;
            }
            if (tf_argc) {
                tf_argv[0].start = offset;
            }
            return shareval(val_one);
          }

        case FN_read:
            wprintf("read() is deprecated.  Use tfread() instead.");
            oldblock = block;  /* condition and evalflag are already correct */
            block = 0;
            Sstr = Stringnew(NULL, -1, 0);
            if (!tfgetS(Sstr, tfin))
                Stringtrunc(Sstr, 0);
            block = oldblock;
            return newSstr(CS(Sstr));

        case FN_nread:
            return newint(read_depth);

        case FN_nactive:
            return newint(nactive(n ? opdstd(1) : NULL));

        case FN_nlog:
            return newint(log_count);

        case FN_nmail:
            return newint(mail_count);

        case FN_systype:
            return newSstr(systype);

        case FN_whatis: {
	    Value *val = opd(1);
            Sstr = Stringnew(NULL, 0, 0);
	    if (val->type == TYPE_ID) {
		Stringcat(Sstr, "id:");
		val = hgetnearestvarval(val);
	    }
	    if (val) {
		switch (val->type & TYPES_BASIC) {
		case TYPE_STR:	   Stringcat(Sstr, "string");	break;
		case TYPE_ENUM:	   Stringcat(Sstr, "enum");	break;
		case TYPE_INT:	   Stringcat(Sstr, "int");	break;
		case TYPE_POS:	   Stringcat(Sstr, "pos");	break;
		case TYPE_DECIMAL: Stringcat(Sstr, "decimal");	break;
		case TYPE_ATIME:   Stringcat(Sstr, "atime");	break;
		case TYPE_DTIME:   Stringcat(Sstr, "dtime");	break;
		case TYPE_FLOAT:   Stringcat(Sstr, "float");	break;
		default:	   Sappendf(Sstr, "%d", opd(1)->type);
		}
	    }
            return newSstr(CS(Sstr));
	  }

        default:
            eprintf("not supported");
            return NULL;
        }
}

static inline ExprFunc *find_builtin_func(const char *name) {
    return (ExprFunc *)bsearch((void*)name, (void*)functab,
	sizeof(functab)/sizeof(ExprFunc), sizeof(ExprFunc), strstructcmp);
}

static Value *do_function(int n /* number of operands (including func id) */)
{
    Value *val, *func_result;
    const ExprFunc *funcrec = NULL;
    Macro *macro = NULL;
    BuiltinCmd *cmd = NULL;
    const char *old_command;
    STATIC_BUFFER(scratch);
    int i;

    val = opd(n);
    n--;

    if (val->type == TYPE_FUNC) {
	funcrec = valptr(val);
        old_command = current_command;
        current_command = val->name;
        errno = 0;
        val = function_switch(funcrec, n, old_command);
#if !NO_FLOAT
	/* BUG: setting val=NULL aborts the macro.  Perhaps instead we should
	 * set val to a value with TYPE_ERROR.
	 */
        if (errno == EDOM) {
            eprintf("argument outside of domain");
            freeval(val);
            val = NULL;
        } else if (errno == ERANGE) {
            eprintf("result outside of range");
            freeval(val);
            val = NULL;
        }
#endif
        current_command = old_command;
        return val;
    }

    if (val->type == TYPE_CMD) {
	cmd = valptr(val);
	if (cmd->macro)
	    macro = (cmd->macro);
    } else if (!(macro = find_hashed_macro(val->name, val->u.hash))) {
	eprintf("%s: no such function", val->name);
	return NULL;
    }

    if (macro) {
	Value *saved_user_result;
	int saved_argtop, saved_argc;
	Arg *saved_argv;

	/* pass parameters by value, not by [pseudo]reference */
	for (i = 1; i <= n; i++) {
	    val = opd(i);
	    if (val->type == TYPE_ID) {
		opd(i) = newSstr(CS(Stringdup(valstr(val))));
		freeval(val);
	    }
	}

	saved_argtop = argtop;
	saved_argc = tf_argc;
	saved_argv = tf_argv;
	saved_user_result = user_result;

	argtop = stacktop;
	tf_argc = n;
	tf_argv = NULL;
	user_result = NULL; /* prevent macro from freeing it */

	func_result = do_macro(macro, NULL, 0, USED_NAME, 0) ?
	    user_result : NULL;

	argtop = saved_argtop;
	tf_argc = saved_argc;
	tf_argv = saved_argv;
	user_result = saved_user_result;

    } else /* if (cmd) */ {
	SStringcpy(scratch, n ? opdstr(1) : blankline);
	func_result = (*cmd->func)(scratch, 0);
    }

    return func_result;
}

static int comma_expr(Program *prog)
{
    if (!assignment_expr(prog)) return 0;
    while (*ip == ',') {
        ip++;
	code_add(prog, OP_POP, 1);	/* throw it away */
        if (!assignment_expr(prog)) return 0;
    }
    return 1;
}

static int assignment_expr(Program *prog)
{
    if (!conditional_expr(prog)) return 0;
    if (is_assignpfx(ip[0]) && ip[1] == '=') {
	opcode_t op = ip[0] | OPF_SIDE;
        ip += 2;
        if (!assignment_expr(prog)) return 0;
	code_add(prog, op, 2);
    }
    return 1;
}

static int conditional_expr(Program *prog)
{
    int jump_point;

    if (!or_expr(prog)) return 0;
    if (*ip == '?') {
        while (is_space(*++ip));
        if (*ip == ':') {
	    code_add(prog, OP_DUP, 1);	/* reuse condition val as true val */
	    code_add(prog, OP_JNZ, -1);	/* place holder */
	    jump_point = prog->len - 1;
	    code_add(prog, OP_POP, 1);	/* discard dup'd value */
        } else {
	    code_add(prog, OP_JZ, -1);	/* place holder */
	    jump_point = prog->len - 1;
            if (!comma_expr(prog)) return 0;
            if (*ip != ':') {
                parse_error(prog, "expression", "':' after '?...'");
                return 0;
            }
	    code_add(prog, OP_JUMP, -1);	/* place holder */
	    comefrom(prog, jump_point, prog->len);
	    jump_point = prog->len - 1;
        }
        ip++;
        if (!conditional_expr(prog)) return 0;
	comefrom(prog, jump_point, prog->len);
    }
    return 1;
}

static int or_expr(Program *prog)
{
    int jump_point;

    if (!and_expr(prog)) return 0;
    while (*ip == '|') {
        ip++;
	code_add(prog, OP_DUP, 1);
	code_add(prog, OP_JNZ, -1);	/* place holder */
	jump_point = prog->len - 1;
	code_add(prog, OP_POP, 1);	/* discard dup'd value */
        if (!and_expr(prog)) return 0;
        comefrom(prog, jump_point, prog->len);
    }
    return 1;
}

static int and_expr(Program *prog)
{
    int jump_point;

    if (!relational_expr(prog)) return 0;
    while (*ip == '&') {
        ip++;
	code_add(prog, OP_DUP, 1);
	code_add(prog, OP_JZ, -1);	/* place holder */
	jump_point = prog->len - 1;
	code_add(prog, OP_POP, 1);	/* discard dup'd value */
        if (!relational_expr(prog)) return 0;
        comefrom(prog, jump_point, prog->len);
    }
    /* XXX optimize: in a series of &'s, a zero should jump all the way to
     * the end, not to another { DUP; JZ }.
     */
    return 1;
}

static int relational_expr(Program *prog)
{
    opcode_t op;

    if (!additive_expr(prog)) return 0;
    while (1) {
        if (ip[0] == '=') {
	    ip++;
            if      (*ip == '~') op = OP_STREQ;
            else if (*ip == '/') op = OP_MATCH;
            else if (*ip == '=') op = OP_EQUAL;
            else {
                if (pedantic) eprintf("suggestion: use == instead of =");
                op = OP_EQUAL;
		ip--;
            }
        } else if (ip[0] == '!') {
            if      (ip[1] == '~') op = OP_STRNEQ;
            else if (ip[1] == '/') op = OP_NMATCH;
            else if (ip[1] == '=') op = OP_NOTEQ;
            else break;
	    ip++;
        }
        else if (ip[0] == '>') op = (ip[1] == '=') ? ++ip, OP_GTE : *ip;
        else if (ip[0] == '<') op = (ip[1] == '=') ? ++ip, OP_LTE : *ip;
        else break;

        ip++;
        if (!additive_expr(prog)) return 0;
	code_add(prog, op, 2);
    }
    return 1;
}

static int additive_expr(Program *prog)
{
    opcode_t op;
    if (!multiplicative_expr(prog)) return 0;
    while (is_additive(*ip) && ip[1] != '=') {
        op = *ip++;
        if (!multiplicative_expr(prog)) return 0;
	code_add(prog, op, 2);
    }
    return 1;
}

static int multiplicative_expr(Program *prog)
{
    opcode_t op;

    if (!unary_expr(prog, 0)) return 0;
    while (is_mult(*ip) && ip[1] != '=') {
        op = *ip++;
        if (!unary_expr(prog, op == '/')) return 0;
	code_add(prog, op, 2);
    }
    return 1;
}

static int unary_expr(Program *prog, int could_be_div_or_macro)
{
    opcode_t op;

    if (is_space(*ip)) could_be_div_or_macro = 0;
    while (is_space(*ip)) ip++;

    if (is_unary(*ip)) {
	if (ip[0] == '!') {
	    op = '!';
	} else if (ip[0] == '+') {
	    op = (ip[1] == '+') ? ++ip, OP_PREINC : '+';
	} else /* if (ip[0] == '-') */ {
	    op = (ip[1] == '-') ? ++ip, OP_PREDEC : '-';
	}
	ip++;
        if (!unary_expr(prog, 0)) return 0;
	code_add(prog, op, 1);
        return 1;

    } else {
        if (!primary_expr(prog, could_be_div_or_macro)) return 0;

        if (*ip == '(') {
            /* function call expression */
            int n = 1;
	    InstructionArg *arg;
	    ExprFunc *funcrec = NULL;
	    BuiltinCmd *cmd = NULL;

	    arg = &prog->code[prog->len-1].arg;
	    if (prog->code[prog->len-1].op != OP_PUSH ||
		arg->val->type != TYPE_ID)
	    {
		eprintf("function name must be an identifier.");
		return 0;
	    }
	    if ((funcrec = find_builtin_func(arg->val->name))) {
		arg->val->type = TYPE_FUNC;
		arg->val->u.p = funcrec;
	    } else if ((cmd = find_builtin_cmd(arg->val->name))) {
		arg->val->type = TYPE_CMD;
		arg->val->u.p = cmd;
		if (cmd->func == handle_exit_command) {
		    eprintf("%s: not a function", cmd->name);
		    return 0;
		}
	    }
	    ++ip;
	    eat_space(prog);
            if (*ip != ')') {
                while (1) {
                    if (!assignment_expr(prog)) return 0;
                    n++;
                    if (*ip == ')') break;
                    if (*ip != ',') {
                        parse_error(prog, "expression",
                            "',' or ')' after function argument");
                        return 0;
                    }
                    ++ip;
                }
            }
	    if (funcrec && (n-1 < funcrec->min || n-1 > funcrec->max)) {
		eprintf((funcrec->min == funcrec->max) ?
		    "%s: found %d arguments, expected %d" :
		    "%s: found %d arguments, expected between %d and %d",
		    funcrec->name, n-1, funcrec->min, funcrec->max);
		return 0;
	    }
	    if (cmd && n > 2) {
		eprintf("%s: command called as function must have 0 or 1 "
		    "argument", cmd->name);
		return 0;
	    }
	    ++ip;
	    eat_space(prog);
	    code_add(prog, OP_FUNC, n);
        }
        return 1;
    }
}

static int primary_expr(Program *prog, int could_be_div_or_macro)
{
    const char *end;
    Value *val;
    String *str;

    eat_space(prog);
    if (is_digit(*ip) || (ip[0] == '.' && is_digit(ip[1]))) {
        if (!(val = parsenumber(ip, &ip, TYPE_NUM, NULL)))
	    return 0;
	code_add(prog, OP_PUSH, val);
    } else if (is_quote(*ip)) {
	int error = 0;
        (str = Stringnew(NULL, -1, 0))->links++;
        if (!stringliteral(str, &ip)) {
            eprintf("%S in string literal", str);
            error++;
	} else {
	    val = newSstr(CS(str));
	}
        Stringfree(str);
        if (error) return 0;
	code_add(prog, OP_PUSH, val);
    } else if (is_alpha(*ip) || *ip == '_') {
        for (end = ip + 1; is_alnum(*end) || *end == '_'; end++);
        val = newid(ip, end - ip);
        ip = end;
	code_add(prog, OP_PUSH, val);
	if (could_be_div_or_macro &&
	    (keyword(val->name) || find_builtin_cmd(val->name) ||
	    find_macro(val->name)))
	{
	    wprintf("possibly missing '%%;' or ')' before /%s", val->name);
	}
    } else if (*ip == '$') {
        static int warned = 0;
        ++ip;
        if ((!warned || pedantic) && *ip == '[') {
            wprintf("$[...] substitution in expression is legal, but redundant.  Try using (...) instead.");
            warned = 1;
        }
        dollarsub(prog, NULL);
    } else if (*ip == '{') {
        if (!varsub(prog, 0, 1)) return 0;
    } else if (*ip == '%') {
        ++ip;
        if (!varsub(prog, 1, 1)) return 0;
    } else if (*ip == '(') {
        ++ip;
        if (!comma_expr(prog)) return 0;
        if (*ip != ')') {
            parse_error(prog, "expression", "')' after '(...'");
            return 0;
        }
        ++ip;
    } else {
        parse_error(prog, "expression", "operand");
        return 0;
    }
    
    eat_space(prog);
    return 1;
}


#if USE_DMALLOC
void free_expr(void)
{
    pfreepool(Value, valpool, u.next);
}
#endif

