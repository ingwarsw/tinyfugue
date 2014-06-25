/*************************************************************************
 *  TinyFugue - programmable mud client
 *  Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2002, 2003, 2004, 2005, 2006-2007 Ken Keys
 *
 *  TinyFugue (aka "tf") is protected under the terms of the GNU
 *  General Public License.  See the file "COPYING" for details.
 ************************************************************************/
/* $Id: parse.h,v 35004.51 2007/01/13 23:12:39 kkeys Exp $ */

#ifndef PARSE_H
#define PARSE_H

/* keywords: must be sorted and numbered sequentially */
typedef enum {
    BREAK = 0200, DO, DONE, ELSE, ELSEIF, ENDIF, EXIT, IF,
    LET, RESULT, RETURN, SET, SETENV, TEST, THEN, WHILE
} keyword_id_t;

#define OPNUM_MASK	0x007F

/* opcode type */
#define OPT_MASK	0x7000
#define OPT_EXPR	0x0000	/* must be zero, for ascii operators */
#define OPT_SUB		0x1000
#define OPT_JUMP	0x2000
#define OPT_CTRL	0x3000

/* opcode flag */
#define OPF_MASK	0x0080
#define OPF_0		0x0000
#define OPF_APP		0x0080	/* append (SUB & PSUB) */
#define OPF_NEG		0x0080	/* negate result (CTRL); negate cond (JUMP) */
#define OPF_SIDE	0x0080	/* has side effect (EXPR) */

/* opcode arg type */
#define OPA_MASK	0x0700
#define OPA_INT		0x0000	/* must be zero, for ascii operators */
#define OPA_NONE	0x0100
#define OPA_STRP	0x0200
#define OPA_CHAR	0x0300
#define OPA_VALP	0x0400
#define OPA_CMDP	0x0500

#define OPLABEL_MASK	(OPNUM_MASK | OPF_MASK)

#define op_type(op)			((op) & OPT_MASK)
#define op_type_is(op, type)		(op_type(op) == OPT_##type)
#define op_arg_type(op)			((op) & OPA_MASK)
#define op_arg_type_is(op, type)	(op_arg_type(op) == OPA_##type)
#define op_is_push(op)			(((op) & OPF_MASK) != OPF_APP)
#define op_is_append(op)		(((op) & OPF_MASK) == OPF_APP)
#define op_has_sideeffect(op)		(((op) & OPF_MASK) == OPF_SIDE)
#define opnum(op)			((op) & OPNUM_MASK)
#define opnum_eq(op1, op2)		(opnum(op1) == opnum(op2))

typedef enum {
#define defopcode(name, num, optype, argtype, flag) \
    OP_##name = (num | OPT_##optype | OPA_##argtype | OPF_##flag),
#include "opcodes.h"
    OP_ENDOFLIST = 0xffff
} opcode_t;


typedef union InstructionArg {
    int i;
    char c;
    conString *str;
    Value *val;
    struct BuiltinCmd *cmd;
} InstructionArg;

typedef struct Instruction {
    opcode_t op;
    union InstructionArg arg;
    const char *start, *end;	/* start/end points in source code, for mecho */
    int comefroms;		/* number of insts that jump to this one */
} Instruction;

struct Program {
    conString *src;	/* source code */
    int srcstart;	/* offset of start in src */
    const char *sip;	/* pointer into src->data, for compiling */
    Instruction *code;	/* compiled code */
    int len;		/* length of compiled code */
    int size;		/* size of code array */
    const char *mark;	/* pointer into source code, for mecho */
    int optimize;	/* opimization level */
};

typedef struct Arg {
    int start, end;
} Arg;

extern void        parse_error(Program *prog, const char *type,
		    const char *expect);
extern void        parse_error_suggest(Program *prog, const char *type,
		    const char *expect, const char *suggestion);
extern int         varsub(Program *prog, int sub_warn, int in_expr);
extern int         exprsub(Program *prog, int in_expr);
extern int         dollarsub(Program *prog, String **destp);
extern conString  *valstr(Value *val);
extern const char *valstd(Value *val);
extern long        valint(const Value *val);
extern int         valtime(struct timeval *tv, const Value *val);
extern int         valbool(const Value *val);
#if !NO_FLOAT
extern double      valfloat(const Value *val);
#endif /* NO_FLOAT */
extern void       *valptr(Value *val);
extern int         pushval(Value *val);
extern void        freeval_fl(Value *val, const char *file, int line);
extern Value      *expr_value(const char *expression);
extern Value      *expr_value_safe(Program *prog);
extern void        code_add(Program *prog, opcode_t op, ...);
extern int         reduce(opcode_t op, int n);
extern const char *oplabel(opcode_t op);

extern struct Value *newptr_fl(void *ptr, const char *file, int line);

#define popval() \
    ((Value*)stack[--stacktop])

/* get Nth operand from stack (counting backwards from top) */
#define opd(N)          (stack[stacktop-(N)])
#define opdfloat(N)     valfloat(opd(N))	/* float value of opd(N) */
#define opdint(N)       valint(opd(N))		/* int value of opd(N) */
#define opdbool(N)      valbool(opd(N))		/* boolean value of opd(N) */
#define opdstr(N)       valstr(opd(N))		/* String value of opd(N) */
#define opdstd(N)       valstd(opd(N))		/* String data of opd(N) */
#define opdtime(tv, N)  valtime(tv, opd(N))	/* copy timeval of opd(N) */

#define freeval(val)	freeval_fl((val), __FILE__, __LINE__)

#define comefrom(prog, from, to) \
    do { \
	if (from >= 0) { \
	    (prog)->code[(from)].arg.i = (to); \
	    (prog)->code[(to)].comefroms++; \
	} \
    } while (0)

#define ip  (prog->sip)	/* XXX */

extern Value *stack[];			/* expression stack */
extern int stacktop;			/* first free position on stack */
extern Arg *tf_argv;			/* shifted command argument vector */
extern int tf_argc;			/* shifted command/function arg count */
extern int argtop;			/* top of function argument stack */
extern const conString *argstring;	/* command argument text */
extern keyword_id_t block;		/* type of current expansion block */
extern int condition;			/* checked by /if and /while */
extern int evalflag;			/* flag: should we evaluate? */

#endif /* PARSE_H */
