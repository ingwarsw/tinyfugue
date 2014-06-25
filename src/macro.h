/*************************************************************************
 *  TinyFugue - programmable mud client
 *  Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2002, 2003, 2004, 2005, 2006-2007 Ken Keys
 *
 *  TinyFugue (aka "tf") is protected under the terms of the GNU
 *  General Public License.  See the file "COPYING" for details.
 ************************************************************************/
/* $Id: macro.h,v 35004.57 2007/01/13 23:12:39 kkeys Exp $ */

#ifndef MACRO_H
#define MACRO_H

enum { USED_NAME, USED_TRIG, USED_HOOK, USED_KEY, USED_N }; /* for Macro.used */

extern int invis_flag;

extern void   init_macros(void);
extern int    macro_equal(Macro *m1, Macro *m2);
extern int    hookname2int(const char *name);
unsigned int  macro_hash(const char *name);
extern Macro *find_hashed_macro(const char *name, unsigned int hash);
extern Macro *find_num_macro(int num); 
extern int    add_new_macro(const char *trig, const char *binding,
                 const hookvec_t *hook, const char *hargs, const char *body,
                 int pri, int prob, attr_t attr, int invis, int mflag);
extern int    add_hook(char *name, const char *body);
extern int    remove_macro_by_name(const char *name);
extern void   nuke_dead_macros(void);
extern void   kill_macro(struct Macro *macro);
extern void   rebind_key_macros(void);
extern void   remove_world_macros(struct World *w);
extern int    save_macros(String *args, int offset);
extern int    do_macro(Macro *macro, String *args, int offset,
		int used_type, int kbnumlocal);
extern const char *macro_body(const char *name);
extern int    find_and_run_matches(String *text, int hooknum, String **linep,
		struct World *world, int globalflag, int exec_list_long);

#define macro_hash(name) \
    (!name ? 0 : (*name == '#') ? atoi(name + 1) : hash_string(name))
#define find_macro(name)    find_hashed_macro(name, macro_hash(name))

#if USE_DMALLOC
extern void   free_macros(void);
#endif

#define add_ibind(key, cmd) \
    add_new_macro(NULL, key, NULL, NULL, cmd, 0, 100, 0, TRUE, 0)

#endif /* MACRO_H */
