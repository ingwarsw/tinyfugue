/*************************************************************************
 *  TinyFugue - programmable mud client
 *  Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2002, 2003, 2004, 2005, 2006-2007 Ken Keys
 *
 *  TinyFugue (aka "tf") is protected under the terms of the GNU
 *  General Public License.  See the file "COPYING" for details.
 ************************************************************************/
/* $Id: variable.h,v 35004.40 2007/01/13 23:12:39 kkeys Exp $ */

#ifndef VARIABLE_H
#define VARIABLE_H

/*********************************************
 * Internal, user, and environment variables *
 *********************************************/

#define set_str_var_by_name(name, sval) \
    set_str_var_by_namehash(name, hash_string(name), CS(sval), 0)
#define set_var_by_id(id, i) \
    setintvar(&special_var[id], i, FALSE)

extern Pattern looks_like_special_sub_ic;

extern void init_variables(void);
extern Var   *newglobalvar(const char *name);
extern Var   *findorcreateglobalvar(const char *name);
extern Var   *hfindnearestvar(const Value *idval);
extern Value *hgetnearestvarval(const Value *idval);
extern Value *getvarval(Var *var);
extern const char *getvar(const char *name);
extern Var *ffindglobalvar(const char *name);
extern void set_str_var_direct(Var *var, int type, conString *value);
extern void set_int_var_direct(Var *var, int type, int value);
extern void set_time_var_direct(Var *var, int type, struct timeval *value);
extern void set_float_var_direct(Var *var, int type, double value);
extern Var *hsetnearestvar(const Value *idval, conString *value);
extern Var *set_str_var_by_namehash(const char *name, unsigned int hash,
    conString *value, int exportflag);
extern Var *setvar(Var *var, Value *value, int exportflag);
extern Var *setstrvar(Var *var, conString *sval, int exportflag);
extern Var *setintvar(Var *var, long ival, int exportflag);
extern int  unsetvar(Var *var);
extern void freevar(Var *var);
extern char *spanvar(const char *start);
extern int  setdelim(const char **pp);
extern int  do_set(const char *name, unsigned int hash, conString *value,
	    int offset, int exportflag, int localflag);
extern int  command_set(String *args, int offset, int exportflag,
	    int localflag);
extern Var *setlocalstrvar(const char *name, conString *value);
extern Var *setlocalintvar(const char *name, int value);
extern Var *setlocaldtimevar(const char *name, struct timeval *value);
extern void pushvarscope(struct List *level);
extern void popvarscope(void);

#if USE_DMALLOC
extern void   free_vars(void);
#endif

#endif /* VARIABLE_H */
