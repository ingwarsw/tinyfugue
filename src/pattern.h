/*************************************************************************
 *  TinyFugue - programmable mud client
 *  Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2002, 2003, 2004, 2005, 2006-2007 Ken Keys
 *
 *  TinyFugue (aka "tf") is protected under the terms of the GNU
 *  General Public License.  See the file "COPYING" for details.
 ************************************************************************/
/* $Id: pattern.h,v 35004.3 2007/01/13 23:12:39 kkeys Exp $ */

#ifndef PATTERN_H
#define PATTERN_H

#include "pcre-2.08/pcre.h"

typedef struct RegInfo {
    pcre *re;
    pcre_extra *extra;
    conString *Str;
    int links;
    int *ovector;
    int ovecsize;
} RegInfo;

struct Pattern {
    char *str;
    RegInfo *ri;
    int mflag;
};

extern void   reset_pattern_locale(void);
extern void   restore_reg_scope(RegInfo *old);
extern int    regmatch_in_scope(Value *val, const char *pattern, String *Str);
extern int    tf_reg_exec(RegInfo *ri, conString *Sstr, const char *str,
		int offset);
extern RegInfo*new_reg_scope(RegInfo *ri, String *Str);
extern void   tf_reg_free(RegInfo *ri);
extern int    regsubstr(struct String *dest, int n);
extern int    init_pattern(Pattern *pat, const char *str, int mflag);
extern int    init_pattern_str(Pattern *pat, const char *str);
extern int    init_pattern_mflag(Pattern *pat, int mflag, int opt);
#define copy_pattern(dst, src)  (init_pattern(dst, (src)->str, (src)->mflag))
extern int    patmatch(const Pattern *pat, conString *Sstr, const char *str);
extern void   free_pattern(Pattern *pat);
extern int    smatch(const char *pat, const char *str);
extern int    smatch_check(const char *s);
extern void   free_patterns(void);

#endif /* PATTERN_H */
