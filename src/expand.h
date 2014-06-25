/*************************************************************************
 *  TinyFugue - programmable mud client
 *  Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2002, 2003, 2004, 2005, 2006-2007 Ken Keys
 *
 *  TinyFugue (aka "tf") is protected under the terms of the GNU
 *  General Public License.  See the file "COPYING" for details.
 ************************************************************************/
/* $Id: expand.h,v 35004.45 2007/01/13 23:12:39 kkeys Exp $ */

#ifndef EXPAND_H
#define EXPAND_H

/* note: these numbers must agree with enum_subs[] in variable.c. */
#define SUB_LITERAL -1  /* send literally (no /command interpretation, even) */
#define SUB_KEYWORD  0  /* SUB_NEWLINE if initial keyword, else no subs      */
#define SUB_NEWLINE  1  /* %; subs and command execution                     */
#define SUB_FULL     2  /* all subs and command execution                    */
#define SUB_MACRO    3  /* all subs and command execution, from macro        */

#if 0
typedef struct {
    Program *prog;
    int ip;		/* instruction pointer */
    String *cmd;
} Process;
#endif

extern void init_expand(void);
extern void prog_free(Program *prog);
extern int prog_run(const Program *prog, const String *args, int offset,
    const char *name, int kbnumlocal);
extern struct Program *compile_tf(conString *body, int bodystart, int subs,
    int is_expr, int optimize);
extern int macro_run(conString *body, int boffset, String *args, int offset,
    int subs, const char *name);
extern Value *prog_interpret(const Program *prog, int in_expr);
extern String *do_mprefix(void);
extern const char **keyword(const char *id);
extern void eat_newline(Program *prog);
extern void eat_space(Program *prog);


#if USE_DMALLOC
extern void   free_expand(void);
#endif

extern struct Value *user_result;
extern const char *current_command;
extern int recur_count, breaking;

#define return_user_result() do { \
        struct Value *v = user_result; \
        user_result = NULL; \
        return v; \
    } while (0)

#define set_user_result(val)  do { \
	Value *tmp = val; /* evaluate val once, before freeing user_result */ \
        freeval(user_result); user_result = tmp; \
    } while(0)

#endif /* EXPAND_H */
