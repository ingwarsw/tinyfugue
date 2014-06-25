/*************************************************************************
 *  TinyFugue - programmable mud client
 *  Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2002, 2003, 2004, 2005, 2006-2007 Ken Keys
 *
 *  TinyFugue (aka "tf") is protected under the terms of the GNU
 *  General Public License.  See the file "COPYING" for details.
 ************************************************************************/
static const char RCSid[] = "$Id: pattern.c,v 35004.4 2007/01/13 23:12:39 kkeys Exp $";


/*
 * Regexp wrappers and glob pattern matching.
 */

#include "tfconfig.h"
#if HAVE_LOCALE_H
# include <locale.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include "port.h"
#include "tf.h"
#include "util.h"
#include "pattern.h"
#include "search.h"	/* for tfio.h */
#include "tfio.h"

static RegInfo *reginfo = NULL;
static const unsigned char *re_tables = NULL;

static const char *cmatch(const char *pat, int ch);
static RegInfo *tf_reg_compile_fl(const char *pattern, int optimize,
    const char *file, int line);

#define tf_reg_compile(pat, opt) \
    tf_reg_compile_fl(pat, opt, __FILE__, __LINE__)


void reset_pattern_locale(void)
{
    re_tables = pcre_maketables();
}

int regmatch_in_scope(Value *val, const char *pattern, String *str)
{
    if (reginfo) tf_reg_free(reginfo);
    if (!val) {
	if (!(reginfo = tf_reg_compile(pattern, 0)))
	    return 0;
    } else if (val->type & TYPE_REGEX) {
	/* use precompiled regexp */
	(reginfo = val->u.ri)->links++;
    } else {
	/* compile regexp, and store it on val for future reuse */
	if (!(reginfo = tf_reg_compile(pattern, 1)))
	    return 0;
	if (val->type == TYPE_STR) {
	    /* not a non-string or an already extended string */
	    (val->u.ri = reginfo)->links++;
	    val->type |= TYPE_REGEX;
	}
    }

    return tf_reg_exec(reginfo, CS(str), NULL, 0);
}

RegInfo *new_reg_scope(RegInfo *ri, String *Str)
{
    RegInfo *old;

    old = reginfo;
    reginfo = ri ? ri : reginfo;	/* use new ri or inherit old reginfo */
    if (reginfo) reginfo->links++;

    return old;
}

void restore_reg_scope(RegInfo *old)
{
    if (reginfo) tf_reg_free(reginfo);
    reginfo = (RegInfo *)old;
}

/* returns length of substituted string, or -1 if no substitution */
/* n>=0:  nth substring */
/* n=-1:  left substring */
/* n=-2:  right substring */
int regsubstr(String *dest, int n)
{
    int idx;
    idx = (n < 0) ? 0 : n * 2;

    if (!(reginfo && reginfo->Str && n < reginfo->ovecsize/3 && reginfo->re &&
	reginfo->ovector[idx] >= 0))
    {
        return -1;
    }
    if (n < -2 || reginfo->ovector[idx+1] < 0) {
        internal_error(__FILE__, __LINE__, "invalid subexp %d", n);
        return -1;
    }
    if (n == -1) {
        SStringncat(dest, reginfo->Str, reginfo->ovector[0]);
        return reginfo->ovector[0];
    } else if (n == -2) {
        SStringocat(dest, reginfo->Str, reginfo->ovector[1]);
        return reginfo->Str->len - reginfo->ovector[1];
    } else {
        SStringoncat(dest, reginfo->Str, reginfo->ovector[idx],
            reginfo->ovector[idx+1] - reginfo->ovector[idx]);
	return reginfo->ovector[idx+1] - reginfo->ovector[idx];
    }
}

static RegInfo *tf_reg_compile_fl(const char *pattern, int optimize,
    const char *file, int line)
{
    RegInfo *ri;
    const char *emsg, *s;
    int eoffset, n;
    /* PCRE_DOTALL optimizes patterns starting with ".*" */
    int options = PCRE_DOLLAR_ENDONLY | PCRE_DOTALL | PCRE_CASELESS;

    ri = dmalloc(NULL, sizeof(RegInfo), file, line);
    if (!ri) return NULL;
    ri->extra = NULL;
    ri->ovector = NULL;
    ri->Str = NULL;
    ri->links = 1;

    if (warn_curly_re && (s = estrchr(pattern, '{', '\\')) &&
	(is_digit(s[1]) || s[1] == ','))
    {
	wprintf("regexp contains '{', which has a new meaning in version 5.0.  "
	    "(This warning can be disabled with '/set warn_curly_re=off'.)");
    }
    for (s = pattern; *s; s++) {
	if (*s == '\\') {
	    if (s[1]) s++;
	} else if (is_upper(*s)) {
	    options &= ~PCRE_CASELESS;
	    break;
	}
    }

    ri->re = pcre_compile((char*)pattern, options, &emsg, &eoffset, re_tables);
    if (!ri->re) {
	/* don't trust emsg to be non-NULL or NUL-terminated */
	eprintf("regexp error: character %d: %.128s", eoffset,
	    emsg ? emsg : "unknown error");
	goto tf_reg_compile_error;
    }
    n = pcre_info(ri->re, NULL, NULL);
    if (n < 0) goto tf_reg_compile_error;
    ri->ovecsize = 3 * (n + 1);
    ri->ovector = dmalloc(NULL, sizeof(int) * ri->ovecsize, file, line);
    if (!ri->ovector) goto tf_reg_compile_error;
    if (optimize) {
	ri->extra = pcre_study(ri->re, 0, &emsg);
	if (emsg) {
	    eprintf("regexp study error: %.128s", emsg);
	    goto tf_reg_compile_error;
	}
    }
    return ri;

tf_reg_compile_error:
    tf_reg_free(ri);
    return NULL;
}

int tf_reg_exec(RegInfo *ri,
    conString *Sstr,	/* String to match.  Will be saved for regsubstr(). */
    const char *str,	/* Used if Sstr is NULL; not saved. */
    int startoffset)
{
    int result, len;

    /* If ovector was stolen by find_and_run_matches(), make a new one. */
    if (!ri->ovector) {
	ri->ovector = MALLOC(sizeof(int) * ri->ovecsize);
	if (!ri->ovector) return 0;
    }

    /* Free old saved Str. */
    if (ri->Str) {
	conStringfree(ri->Str);
	ri->Str = NULL;
    }

    if (Sstr) {
	str = Sstr->data;
	len = Sstr->len;
    } else {
	len = strlen(str);
    }
    result = pcre_exec(ri->re, ri->extra, str, len, startoffset,
	startoffset ? PCRE_NOTBOL : 0, ri->ovector, ri->ovecsize);
    if (result < 0) {
	result = 0;
    } else {
	if (result == 0) result = 1; /* shouldn't happen, with ovector */
	if (Sstr) (ri->Str = Sstr)->links++;	/* save, for regsubstr() */
    }
    return result;
}

void tf_reg_free(RegInfo *ri)
{
    if (--ri->links > 0) return;
    if (ri->ovector) FREE(ri->ovector);
    if (ri->re) pcre_free(ri->re);
    if (ri->extra) pcre_free(ri->extra);
    if (ri->Str) conStringfree(ri->Str);
    FREE(ri);
}

/* call with (pat, NULL, -1) to zero-out pat.
 * call with (pat, str, -1) to init pat with some outside string (ie, strdup).
 * call with (pat, str, mflag) to init pat with some outside string.
 */
int init_pattern(Pattern *pat, const char *str, int mflag)
{
    return init_pattern_str(pat, str) && init_pattern_mflag(pat, mflag, 0);
}

int init_pattern_str(Pattern *pat, const char *str)
{
    pat->ri = NULL;
    pat->mflag = -1;
    pat->str = (!str) ? NULL : STRDUP(str);
    return 1;
}

int init_pattern_mflag(Pattern *pat, int mflag, int opt)
{
    extern char current_opt;
    char saved_opt = current_opt;
    current_opt = opt;
    if (!pat->str || pat->mflag >= 0) goto ok;
    pat->mflag = mflag;
    switch (mflag) {
    case MATCH_GLOB:
        if (smatch_check(pat->str)) goto ok;
	break;
    case MATCH_REGEXP:
#if 0
        char *s = pat->str;
        while (*s == '(' || *s == '^') s++;
        if (strncmp(s, ".*", 2) == 0)
            wprintf("leading \".*\" in a regexp is inefficient.");
#endif
	if ((pat->ri = tf_reg_compile(pat->str, 1))) goto ok;
	break;
    default:
        goto ok;
    }
    FREE(pat->str);
    pat->str = NULL;
    current_opt = saved_opt;
    return 0;
ok:
    current_opt = saved_opt;
    return 1;
}

void free_pattern(Pattern *pat)
{
    if (pat->str) FREE(pat->str);
    if (pat->ri) tf_reg_free(pat->ri);
    pat->str = NULL;
    pat->ri  = NULL;
}

int patmatch(
    const Pattern *pat,
    conString *Sstr,	/* String to match.  Will be saved for regsubstr(). */
    const char *str)	/* Used if Sstr is NULL; not saved. */
{
    if (Sstr) str = Sstr->data;
    if (!pat->str) return 1;

    switch (pat->mflag) {
    /* Even a blank regexp must be exec'd, so Pn will work. */
    case MATCH_REGEXP: return !!tf_reg_exec(pat->ri, Sstr, str, 0);
    case MATCH_GLOB:   return !smatch(pat->str, str);
    case MATCH_SIMPLE: return !strcmp(pat->str, str);
    case MATCH_SUBSTR: return !!strstr(str, pat->str);
    default: eprintf("internal error: pat->mflag == %d", pat->mflag);
    }

    return 0;
}

/* class is a pointer to a string of the form "[...]..."
 * ch is compared against the character class described by class.
 * If ch matches, cmatch() returns a pointer to the char after ']' in class;
 * otherwise, cmatch() returns NULL.
 */
static const char *cmatch(const char *class, int ch)
{
    int not;

    ch = lcase(ch);
    if ((not = (*++class == '^'))) ++class;

    while (1) {
        if (*class == ']') return (char*)(not ? class + 1 : NULL);
        if (*class == '\\') ++class;
        if (class[1] == '-' && class[2] != ']') {
            char lo = *class;
            class += 2;
            if (*class == '\\') ++class;
            if (ch >= lcase(lo) && ch <= lcase(*class)) break;
        } else if (lcase(*class) == ch) break;
        ++class;
    }
    return not ? NULL : (estrchr(++class, ']', '\\') + 1);
}

/* smatch_check() should be used on pat to check pattern syntax before
 * calling smatch().
 */
/* Based on code by Leo Plotkin. */

int smatch(const char *pat, const char *str)
{
    const char *start = str;
    static int inword = FALSE;

    while (*pat) {
        switch (*pat) {

        case '\\':
            pat++;
            if (lcase(*pat++) != lcase(*str++)) return 1;
            break;

        case '?':
            if (!*str || (inword && is_space(*str))) return 1;
            str++;
            pat++;
            break;

        case '*':
            while (*pat == '*' || *pat == '?') {
                if (*pat == '?') {
                    if (!*str || (inword && is_space(*str))) return 1;
                    str++;
                }
                pat++;
            }
            if (inword) {
                while (*str && !is_space(*str))
                    if (!smatch(pat, str++)) return 0;
                return smatch(pat, str);
            } else if (!*pat) {
                return 0;
            } else if (*pat == '{') {
                if (str == start || is_space(str[-1]))
                    if (!smatch(pat, str)) return 0;
                for ( ; *str; str++)
                    if (is_space(*str) && !smatch(pat, str+1)) return 0;
                return 1;
            } else if (*pat == '[') {
                while (*str) if (!smatch(pat, str++)) return 0;
                return 1;
            } else {
                char c = (pat[0] == '\\' && pat[1]) ? pat[1] : pat[0];
                for (c = lcase(c); *str; str++)
                    if (lcase(*str) == c && !smatch(pat, str))
                        return 0;
                return 1;
            }

        case '[':
            if (inword && is_space(*str)) return 1;
            if (!(pat = cmatch(pat, *str++))) return 1;
            break;

        case '{':
            if (str != start && !is_space(*(str - 1))) return 1;
            {
                const char *end;
                int result = 1;

                /* This can't happen if smatch_check is used first. */
                if (!(end = estrchr(pat, '}', '\\'))) {
                    eprintf("smatch: unmatched '{'");
                    return 1;
                }

                inword = TRUE;
                for (pat++; pat <= end; pat++) {
                    if ((result = smatch(pat, str)) == 0) break;
                    if (!(pat = estrchr(pat, '|', '\\'))) break;
                }
                inword = FALSE;
                if (result) return result;
                pat = end + 1;
                while (*str && !is_space(*str)) str++;
            }
            break;

        case '}': case '|':
            if (inword) return (*str && !is_space(*str));
            /* else FALL THROUGH to default case */

        default:
            if (lcase(*pat++) != lcase(*str++)) return 1;
            break;
        }
    }
    return lcase(*pat) - lcase(*str);
}

/* verify syntax of smatch pattern */
int smatch_check(const char *pat)
{
    int inword = FALSE;
    const char *patstart = pat;

    while (*pat) {
        switch (*pat) {
        case '\\':
            if (*++pat) pat++;
            break;
        case '[':
            if (!(pat = estrchr(pat, ']', '\\'))) {
                eprintf("glob error: unmatched '['");
                return 0;
            }
            pat++;
            break;
        case '{':
            if (inword) {
                eprintf("glob error: nested '{'");
                return 0;
            }
	    if (!(pat==patstart || is_space(pat[-1]) || strchr("*?]", pat[-1])))
	    {
                eprintf("glob error: '%c' before '{' can never match", pat[-1]);
		return 0;
	    }
            inword = TRUE;
            pat++;
            break;
        case '}':
	    if (!(!pat[1] || is_space(pat[1]) || strchr("*?[", pat[1]))) {
                eprintf("glob error: '%c' after '}' can never match", pat[1]);
		return 0;
	    }
            inword = FALSE;
            pat++;
            break;
        case '?':
        case '*':
        default:
	    if (inword && is_space(*pat)) {
                eprintf("glob error: space inside '{...}' can never match");
		return 0;
	    }
            pat++;
            break;
        }
    }
    if (inword) eprintf("glob error: unmatched '{'");
    return !inword;
}

#if USE_DMALLOC
void free_patterns(void)
{
    if (reginfo) {
	tf_reg_free(reginfo);
	reginfo = NULL;
    }
}
#endif

