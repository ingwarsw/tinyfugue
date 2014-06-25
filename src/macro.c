/*************************************************************************
 *  TinyFugue - programmable mud client
 *  Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2002, 2003, 2004, 2005, 2006-2007 Ken Keys
 *
 *  TinyFugue (aka "tf") is protected under the terms of the GNU
 *  General Public License.  See the file "COPYING" for details.
 ************************************************************************/
static const char RCSid[] = "$Id: macro.c,v 35004.188 2007/01/13 23:12:39 kkeys Exp $";


/**********************************************
 * Fugue macro package                        *
 *                                            *
 * Macros, hooks, triggers, hilites and gags  *
 * are all processed here.                    *
 **********************************************/

#include "tfconfig.h"
#include "port.h"
#include "tf.h"
#include "util.h"
#include "pattern.h"
#include "search.h"
#include "tfio.h"
#include "world.h"
#include "macro.h"
#include "keyboard.h"	/* bind_key()... */
#include "expand.h"
#include "socket.h"	/* xworld() */
#include "output.h"	/* get_keycode() */
#include "attr.h"
#include "cmdlist.h"
#include "command.h"
#include "parse.h"	/* valbool() for /def -E */
#include "variable.h"	/* set_var_by_id() */

typedef struct {
    cattr_t attr;
    short subexp;
} subattr_t;

struct Macro {
    const char *name;
    struct ListEntry *numnode;		/* node in maclist */
    struct ListEntry *hashnode;		/* node in macro_table hash bucket */
    struct ListEntry *trignode;		/* node in one of the triglists */
    struct Macro *tnext;		/* temp list ptr for collision/death */
    conString *body, *expr;
    Program *prog, *exprprog;		/* compiled body, expr */
    const char *bind, *keyname;
    Pattern trig, hargs, wtype;		/* trigger/hook/worldtype patterns */
    hookvec_t hook;			/* bit vector */
    struct World *world;		/* only trig on text from world */
    int pri, num;
    attr_t attr;
    int nsubattr;
    subattr_t *subattr;
    short prob, shots, invis;
    short flags;
    signed char fallthru, quiet;
    struct BuiltinCmd *builtin;		/* builtin cmd with same name, if any */
    int used[USED_N];			/* number of calls by each method */
};

typedef struct {
    Pattern name, body, bind, keyname, expr;
} AuxPat;

typedef struct {
    int shortflag;
    int usedflag;
    Cmp *cmp;
} ListOpts;

int invis_flag = 0;

static Macro  *macro_spec(String *args, int offset, int *xmflag, ListOpts *opts);
static int     macro_match(Macro *spec, Macro *macro, AuxPat *aux);
static int     add_numbered_macro(Macro *macro, unsigned int hash, int num,
		ListEntry *numnode);
static int     complete_macro(Macro *spec, unsigned int hash, int num,
		ListEntry *numnode);
static int     init_aux_patterns(Macro *spec, int mflag, AuxPat *aux);
static Macro  *match_exact(int hooknum, const char *str, attr_t attrs);
static int     list_defs(TFILE *file, Macro *spec, int mflag, ListOpts *opts);
static void    apply_attrs_of_match(Macro *macro, String *text, int hooknum,
		String *line);
static int     run_match(Macro *macro, String *text, int hooknum);
static const String *hook_name(const hookvec_t *hook) PURE;
static conString *print_def(TFILE *file, String *buffer, Macro *p);
static int     rpricmp(const Macro *m1, const Macro *m2);
static void    nuke_macro(Macro *macro);


#define HASH_SIZE 997	/* prime number */

#define MACRO_TEMP	0x01
#define MACRO_DEAD	0x02
#define MACRO_HOOK	0x08
#define MACRO_NOHOOK	0x10	/* -h0 */

#define INVALID_SUBEXP	-3

static List maclist[1];			/* list of all (live) macros */
static List triglist[1];		/* list of macros by trigger */
static List hooklist[NUM_HOOKS];	/* lists of macros by hook */
static Macro *dead_macros;		/* head of list of dead macros */
static HashTable macro_table[1];	/* macros hashed by name */
static World NoWorld, AnyWorld;		/* explicit "no" and "any" */
static int mnum = 0;			/* macro ID number */

typedef enum {
    HT_TEXT = 0x00,	/* normal text in fg world */
    HT_ALERT = 0x01,	/* alert */
    HT_WORLD = 0x02,	/* text in xsock->world, plus alert if xsock != fsock */
    HT_XSOCK = 0x04	/* alert should be cleared if xsock is foregrounded */
} hooktype_t;

typedef struct hookrec {
    const char *name;
    hooktype_t hooktype;
} hookrec_t;

static const hookrec_t hook_table[] = {
#define gencode(id, type)  { #id, type }
#include "hooklist.h"
#undef gencode
};

#define NONNULL(str) ((str) ? (str) : "")

/* These macros allow easy sharing of trigger and hook code. */
#define MAC(Node)       ((Macro *)((Node)->datum))


void init_macros(void)
{
    int i;
    init_hashtable(macro_table, HASH_SIZE, cstrstructcmp);
    init_list(maclist);
    init_list(triglist);
    for (i = 0; i < (int)NUM_HOOKS; i++)
	init_list(&hooklist[i]);
}

/***************************************
 * Routines for parsing macro commands *
 ***************************************/

int hookname2int(const char *name)
{
    const hookrec_t *hookrec;
    hookrec = bsearch((void*)(name), (void *)hook_table,
	NUM_HOOKS, sizeof(hookrec_t), cstrstructcmp);
    if (hookrec)
	return hookrec - hook_table;
    if (cstrcmp(name, "BACKGROUND") == 0) /* backward compatability */
	return H_BGTRIG;
    if (cstrcmp(name, "CONNETFAIL") == 0) /* backward compatability */
	eprintf("invalid hook event \"%s\"; see \"/help /connect\"", name);
    else
	eprintf("invalid hook event \"%s\"", name);
    return -1;
}

/* Convert hook string to bit vector; return 0 on error, 1 on success. */
static int parse_hook(char **argp, hookvec_t *hookvec)
{
    char *in, state;

    VEC_ZERO(hookvec);
    if (!**argp) {
	memset(hookvec, 0xFF, sizeof(*hookvec));
	return 1;
    }
    for (state = '|'; state == '|'; *argp = in) {
        for (in = *argp; *in && !is_space(*in) && *in != '|'; ++in);
        state = *in;
        *in++ = '\0';
        if (strcmp(*argp, "*") == 0) {
	    memset(hookvec, 0xFF, sizeof(*hookvec));
	} else {
	    int i;
	    if ((i = hookname2int(*argp)) < 0)
                return 0;
	    VEC_SET(i, hookvec);
        }
    }
    if (!state) *argp = NULL;
    return 1;
}

/* macro_spec
 * Converts a macro description string to a more useful Macro structure.
 * Omitted fields are set to a value that means "don't care".
 * In /def, don't care fields are set to their default values;
 * in macro_match(), they are not used in the comparison.  Don't care
 * values for numeric fields are -1 or 0, depending on the field; for
 * strings, NULL; for hooks, neither MACRO_HOOK nor MACRO_NOHOOK will
 * be set.
 */
static Macro *macro_spec(String *args, int offset, int *xmflag, ListOpts *listopts)
{
    Macro *spec;
    char opt;
    const char *ptr, *s, *name = NULL, *body = NULL, *nameend, *bodyend;
    int i, n, mflag = -1, error = 0;
    ValueUnion uval;
    attr_t attrs;

    if (!(spec = (Macro *)MALLOC(sizeof(struct Macro)))) {
        eprintf("macro_spec: not enough memory");
        return NULL;
    }
    spec->num = 0;
    spec->body = spec->expr = NULL;
    spec->prog = spec->exprprog = NULL;
    spec->name = spec->bind = spec->keyname = NULL;
    spec->numnode = spec->trignode = spec->hashnode = NULL;
    init_pattern_str(&spec->trig, NULL);
    init_pattern_str(&spec->hargs, NULL);
    init_pattern_str(&spec->wtype, NULL);
    spec->world = NULL;
    spec->pri = spec->prob = spec->shots = spec->fallthru = spec->quiet = -1;
    VEC_ZERO(&spec->hook);
    spec->invis = 0;
    spec->attr = 0;
    spec->nsubattr = 0;
    spec->subattr = NULL;
    spec->flags = MACRO_TEMP;
    spec->builtin = NULL;
    spec->used[USED_NAME] = spec->used[USED_TRIG] =
	spec->used[USED_HOOK] = spec->used[USED_KEY] = 0;

    startopt(CS(args), "usSp#c#b:B:E:t:w:h:a:f:P:T:FiIn#1m:q" +
	(listopts ? 0 : 3));
    while (!error && (opt = nextopt(&ptr, &uval, NULL, &offset))) {
        switch (opt) {
        case 'u':
            listopts->usedflag = 1;
            break;
        case 's':
            listopts->shortflag = 1;
            break;
        case 'S':
            listopts->cmp = cstrpppcmp;
            break;
        case 'm':
            if (!(error = ((i = enum2int(ptr, 0, enum_match, "-m")) < 0))) {
		if ((error = (mflag >= 0 && mflag != i)))
		    eprintf("-m option conflicts with earlier -m or -P");
		mflag = i;
	    }
            break;
        case 'p':
            spec->pri = uval.ival;
            break;
        case 'c':
            spec->prob = uval.ival;
            break;
        case 'F':
            spec->fallthru = 1;
            break;
        case 'i':
            spec->invis = 1;
            break;
        case 'I':
            spec->invis = 2;
            break;
        case 'b':
            if (spec->keyname) FREE(spec->keyname);
            if (spec->bind) FREE(spec->bind);
            ptr = print_to_ascii(NULL, ptr)->data;
            spec->bind = STRDUP(ptr);
            break;
        case 'B':
	    if (warn_def_B)
		wprintf("/def -B is deprecated.  See /help keys.");
            if (spec->keyname) FREE(spec->keyname);
            if (spec->bind) FREE(spec->bind);
            spec->keyname = STRDUP(ptr);
            break;
        case 'E':
            if (spec->expr) conStringfree(spec->expr);
            (spec->expr = CS(Stringnew(ptr, -1, 0)))->links++;
            break;
        case 't':
            free_pattern(&spec->trig);
            error += !init_pattern_str(&spec->trig, ptr);
            break;
        case 'T':
            free_pattern(&spec->wtype);
            error += !init_pattern_str(&spec->wtype, ptr);
            break;
        case 'w':
            if (!*ptr || strcmp(ptr, "+") == 0)
                spec->world = &AnyWorld;
            else if (strcmp(ptr, "-") == 0)
                spec->world = &NoWorld;
            else if ((error = !(spec->world = find_world(ptr))))
                eprintf("No world %s", ptr);
            break;
        case 'h':
	    if (strcmp(ptr, "0") == 0) {
		spec->flags |= MACRO_NOHOOK;
	    } else {
		char *buf, *p;
		p = buf = STRDUP(ptr); /* XXX optimize */
		if (!(error = !parse_hook(&p, &spec->hook))) {
		    free_pattern(&spec->hargs);
		    error += !init_pattern_str(&spec->hargs, p);
		    spec->flags |= MACRO_HOOK;
		}
		FREE(buf);
            }
            break;
        case 'a': case 'f':
            error = !parse_attrs(ptr, &attrs, 0);
            spec->attr = adj_attr(spec->attr, attrs);
            break;
        case 'P':
            if ((error = (spec->nsubattr > 0))) {
                eprintf("-P can be given only once per macro.");
		break;
	    } else if ((error = (mflag >= 0 && mflag != MATCH_REGEXP))) {
		eprintf("\"-P\" requires \"-mregexp -t<pattern>\"");
		break;
	    }
	    for (n = 0, s = ptr; *s; s++) {
		if (*s == ';') continue; /* don't count empties */
		n++;
		s = strchr(s, ';');
		if (!s) break;
	    }
	    spec->subattr = MALLOC(n * sizeof(subattr_t));
	    if (!spec->subattr) {
		eprintf("-P: out of memory");
		error++;
		break;
	    }
	    spec->nsubattr = n;

	    {
	    char *start, *end, *buf = STRDUP(ptr); /* XXX optimize */
	    for (i = 0, start = buf; !error && i < n; i++, start = end+1) {
		attr_t attr;
		if ((end = strchr(start, ';')))
		    *end = '\0';
		if (end == start) { /* skip empty */
		    i--;
		    continue;
		}
		if (*start == 'L') {
		    spec->subattr[i].subexp = -1;
		    start++;
		} else if (*start == 'R') {
		    spec->subattr[i].subexp = -2;
		    start++;
		} else {
		    spec->subattr[i].subexp = strtoint(start, &start);
		    if ((error = (spec->subattr[i].subexp < 0))) {
			eprintf("-P: number must be non-negative");
			break;
		    }
		}
		error = !parse_attrs(start, &attr, 0);
		spec->subattr[i].attr = attr;
	    }
	    FREE(buf);
	    }
	    mflag = MATCH_REGEXP;
            break;
        case 'n':
            spec->shots = uval.ival;
            break;
        case 'q':
            spec->quiet = TRUE;
            break;
        case '1':
            spec->shots = 1;
            break;
        default:
            error = TRUE;
        }
    }

    if (error) {
        nuke_macro(spec);
        return NULL;
    }
    if (mflag < 0)
	mflag = matching;
    if (xmflag) *xmflag = mflag;
    if (!init_pattern_mflag(&spec->trig, mflag, 't') ||
	!init_pattern_mflag(&spec->hargs, mflag, 'h') ||
	!init_pattern_mflag(&spec->wtype, mflag, 'w'))
    {
        nuke_macro(spec);
        return NULL;
    }

    name = args->data + offset;
    while (is_space(*name)) ++name;
    if (!*name) return spec;
    nameend = name + strcspn(name, "=");

    if (*nameend == '=') {
	body = nameend + 1;
	while (is_space(*body)) ++body;
	bodyend = args->data + args->len;
	while (bodyend > body && is_space(bodyend[-1])) bodyend--;
	if (bodyend > body)
	    (spec->body = CS(Stringodup(CS(args), body - args->data)))->links++;
    }
    while (nameend > name && is_space(nameend[-1])) nameend--;
    if (nameend > name) {
	char *buf = strncpy(XMALLOC(nameend - name + 1), name, nameend - name);
	buf[nameend - name] = '\0';
	spec->name = buf;
    }

    return spec;
}


/* init_aux_patterns
 * Macro_match() needs to compare some string fields that aren't normally
 * patterns.  This function initializes patterns for those fields.
 */
static int init_aux_patterns(Macro *spec, int mflag, AuxPat *aux)
{
    init_pattern_str(&aux->name, spec->name);
    init_pattern_str(&aux->body, spec->body ? spec->body->data : NULL);
    init_pattern_str(&aux->expr, spec->expr ? spec->expr->data : NULL);
    init_pattern_str(&aux->bind, spec->bind);
    init_pattern_str(&aux->keyname, spec->keyname);
    if (mflag < 0) mflag = matching;

    return init_pattern_mflag(&aux->name, mflag, 0) &&
	init_pattern_mflag(&aux->body, mflag, 0) &&
	init_pattern_mflag(&aux->expr, mflag, 'E') &&
	init_pattern_mflag(&aux->bind, mflag, 'b') &&
	init_pattern_mflag(&aux->keyname, mflag, 'B');
}

static void free_aux_patterns(AuxPat *aux)
{
    free_pattern(&aux->name);
    free_pattern(&aux->body);
    free_pattern(&aux->bind);
    free_pattern(&aux->keyname);
    free_pattern(&aux->expr);
}

/* return 1 if attr contains any of the attributes listed in sattr */
static int attr_match(attr_t sattr, attr_t attr)
{
    if (sattr == F_NONE) {
	return !attr;
    } else {
	if ((sattr & ~F_COLORS & attr) != 0)
	    return 1;
	if ((sattr & F_FGCOLOR) && (sattr & F_FGCOLORS) == (attr & F_FGCOLORS))
	    return 1;
	if ((sattr & F_BGCOLOR) && (sattr & F_BGCOLORS) == (attr & F_BGCOLORS))
	    return 1;
	return 0;
    }
}

/* macro_match
 * Compares spec to macro.  aux contains patterns for string fields that
 * aren't normally patterns.  Returns 1 for match, 0 for nonmatch.
 */
static int macro_match(Macro *spec, Macro *macro, AuxPat *aux)
{
    if (!spec->invis && macro->invis) return 0;
    if (spec->invis == 2 && !macro->invis) return 0;
    if (spec->shots >= 0 && spec->shots != macro->shots) return 0;
    if (spec->fallthru >= 0 && spec->fallthru != macro->fallthru) return 0;
    if (spec->prob >= 0 && spec->prob != macro->prob) return 0;
    if (spec->pri >= 0 && spec->pri != macro->pri) return 0;
    if (spec->attr && !attr_match(spec->attr, macro->attr)) return 0;
    if (spec->subattr) {
	int i, j;
	for (i = 0; i < spec->nsubattr; i++) {
	    for (j = 0; ; j++) {
		if (j >= macro->nsubattr) return 0;
		if (macro->subattr[j].subexp == spec->subattr[i].subexp) break;
	    }
	    if (!attr_match(spec->subattr[i].attr, macro->subattr[j].attr))
		return 0;
	}
    }
    if (spec->world) {
        if (spec->world == &NoWorld) {
            if (macro->world) return 0;
        } else if (spec->world == &AnyWorld) {
            if (!macro->world) return 0;
        } else if (spec->world != macro->world) return 0;
    }

    if (spec->keyname) {
        if (!*spec->keyname) {
            if (!*macro->keyname) return 0;
        } else {
            if (!patmatch(&aux->keyname, NULL, macro->keyname)) return 0;
        }
    }

    if (spec->bind) {
        if (!*spec->bind) {
            if (!*macro->bind) return 0;
        } else {
            if (!patmatch(&aux->bind, NULL, macro->bind)) return 0;
        }
    }

    if (spec->expr) {
        if (!spec->expr->len) {
            if (!macro->expr) return 0;
        } else {
            if (!macro->expr) return 0;
	    if (!patmatch(&aux->expr, NULL, macro->expr->data)) return 0;
        }
    }

    if (spec->flags & MACRO_NOHOOK) {
	/* -h0 */
	if (macro->flags & MACRO_HOOK) return 0;
    } else if (spec->flags & MACRO_HOOK) {
	int i, hit = 0;
	if (!(macro->flags & MACRO_HOOK)) return 0;
	for (i = 0; i < sizeof(spec->hook)/sizeof(long); i++) {
	    if ((hit = (spec->hook.bits[i] & macro->hook.bits[i])))
		break;
	}
	if (!hit) return 0;
        if (spec->hargs.str && *spec->hargs.str) {
            if (!patmatch(&spec->hargs, NULL, NONNULL(macro->hargs.str)))
                return 0;
        }
    }

    if (spec->trig.str) {
        if (!*spec->trig.str) {
            if (!macro->trig.str) return 0;
        } else {
            if (!patmatch(&spec->trig, NULL, NONNULL(macro->trig.str)))
                return 0;
        }
    }
    if (spec->wtype.str) {
        if (!*spec->wtype.str) {
            if (!macro->wtype.str) return 0;
        } else {
            if (!patmatch(&spec->wtype, NULL, NONNULL(macro->wtype.str)))
                return 0;
        }
    }
    if (spec->num && macro->num != spec->num)
        return 0;
    if (spec->name && !patmatch(&aux->name, NULL, macro->name))
        return 0;
    if (spec->body && !patmatch(&aux->body, macro->body, NULL))
        return 0;
    return 1;
}

/* macro_equal
 * Returns true if m1 and m2 are exactly equivalent (except for num).
 */
int macro_equal(Macro *m1, Macro *m2)
{
    if (m1->invis != m2->invis) return 0;
    if (m1->shots != m2->shots) return 0;
    if (m1->fallthru != m2->fallthru) return 0;
    if (m1->quiet != m2->quiet) return 0;
    if (m1->prob != m2->prob) return 0;
    if (m1->pri != m2->pri) return 0;
    if (m1->attr != m2->attr) return 0;
    if (m1->nsubattr != m2->nsubattr) return 0;
    if (m1->subattr) {
	int i, j;
	for (i = 0; i < m1->nsubattr; i++) {
	    for (j = 0; ; j++) {
		if (j >= m2->nsubattr) return 0;
		if (m2->subattr[j].subexp == m1->subattr[i].subexp)
		    break;
	    }
	    if (m2->subattr[j].attr != m1->subattr[i].attr)
		return 0;
	}
    }
    if (m1->world != m2->world) return 0;

    if ((m1->flags ^ m2->flags) &~ MACRO_TEMP) return 0;
    if (m1->flags & MACRO_HOOK)
	if (memcmp(&m1->hook, &m2->hook, sizeof(m1->hook)) != 0) return 0;

    if (nullstrcmp(m1->keyname, m2->keyname) != 0) return 0;
    if (nullstrcmp(m1->bind, m2->bind) != 0) return 0;
    if (nullstrcmp(m1->trig.str, m2->trig.str) != 0) return 0;
    if (nullstrcmp(m1->hargs.str, m2->hargs.str) != 0) return 0;
    if (nullstrcmp(m1->wtype.str, m2->wtype.str) != 0) return 0;
    if (nullstrcmp(m1->name, m2->name) != 0) return 0;
    if (Stringcmp(m1->body, m2->body) != 0) return 0;
    if (Stringcmp(m1->expr, m2->expr) != 0) return 0;
    return 1;
}

/* find Macro by name */
Macro *find_hashed_macro(const char *name, unsigned int hash)
{
    if (!*name) return NULL;
    if (*name == '#') return find_num_macro(hash);
    return (Macro *)hashed_find(name, hash, macro_table);
}

/* find single exact match */
static Macro *match_exact(int hooknum, const char *str, attr_t attrs)
{
    ListEntry *node;
  
    if (hooknum < 0 && !*str) return NULL;
    for (node = hooknum>=0 ? hooklist[hooknum].head : triglist->head; node;
	node = node->next)
    {
        Macro *macro = MAC(node);
        if (macro->flags & MACRO_DEAD) continue;
	if (hooknum>=0) {
	    if (!VEC_ISSET(hooknum, &macro->hook)) continue;
	    if (!macro->hargs.str || cstrcmp(macro->hargs.str, str) == 0)
		return macro;
	} else {
	    if (!(macro->attr & attrs)) continue;
	    if (!macro->trig.str || cstrcmp(macro->trig.str, str) == 0)
		return macro;
	}
    }
    eprintf("%s on \"%s\" was not defined.", hooknum>=0 ? "Hook" : "Trigger",
	str);
    return NULL;
}

/**************************
 * Routines to add macros *
 **************************/

/* create a Macro */
int add_new_macro(const char *trig, const char *bind, const hookvec_t *hook,
    const char *hargs, const char *body, int pri, int prob, attr_t attr,
    int invis, int mflag)
{
    Macro *new;
    int error = 0;

    if (!(new = (Macro *) MALLOC(sizeof(struct Macro)))) {
        eprintf("add_new_macro: not enough memory");
        return 0;
    }
    new->numnode = new->trignode = new->hashnode = NULL;
    new->flags = MACRO_TEMP;
    new->prog = new->exprprog = NULL;
    new->name = STRDUP("");
    (new->body = CS(Stringnew(body, -1, 0)))->links++;
    new->expr = NULL;
    new->bind = STRDUP(bind);
    new->keyname = STRDUP("");
    if (hook) {
	new->flags |= MACRO_HOOK;
	new->hook = *hook;
    } else {
	VEC_ZERO(&new->hook);
    }
    error += !init_pattern(&new->trig, trig, mflag);
    error += !init_pattern(&new->hargs, hargs, mflag);
    init_pattern_str(&new->wtype, NULL);
    new->world = NULL;
    new->pri = pri;
    new->prob = prob;
    new->attr = attr;
    new->nsubattr = 0;
    new->subattr = NULL;
    new->shots = 0;
    new->invis = invis;
    new->fallthru = FALSE;
    new->quiet = FALSE;
    new->builtin = NULL;
    new->used[USED_NAME] = new->used[USED_TRIG] =
	new->used[USED_HOOK] = new->used[USED_KEY] = 0;

    if (!error)
	return add_numbered_macro(new, 0, 0, NULL);
    nuke_macro(new);
    return 0;
}

static int bind_key_macro(Macro *spec)
{
    Macro *orig;

    if ((orig = find_key(spec->bind))) {
	if (macro_equal(orig, spec)) {
	    return -1; /* leave orig in place, don't insert spec */
        } else if (redef) {
	    const char *obody = orig->body->data;
	    const char *sbody = spec->body->data;
	    if (strncmp(sbody, "/key_", 5) != 0 && orig->invis &&
		strncmp(obody, "/key_", 5) == 0 && !strchr(obody, ' '))
	    {
		conString *buf = print_def(NULL, NULL, orig);
		eprintf("%ARecommendation: leave \"%S\" defined and "
		    "define a macro named \"%s\" instead.",
		    warning_attr, buf, obody+1);
		conStringfree(buf);
	    }
            kill_macro(orig); /* this guarantees intrie will succeed */
	    do_hook(H_REDEF, "!Redefined %s %S", "%s %S",
		"binding", ascii_to_print(spec->bind));
        } else {
            eprintf("Binding %S already exists.", ascii_to_print(spec->bind));
            return 0;
        }
    }

    return bind_key(spec, spec->bind);
}

/* add_macro
 * Install a permanent Macro in appropriate structures.
 * Only the keybinding is checked for conflicts; everything else is assumed
 * assumed to be error- and conflict-free.  If the bind_key_macro fails, the
 * macro will be nuked.
 */
static int add_numbered_macro(Macro *macro, unsigned int hash, int num,
    ListEntry *numnode)
{
    if (!macro) return 0;

    if (*macro->bind) {
	int result = bind_key_macro(macro);
	if (result <= 0) {
	    nuke_macro(macro);
	    return -result;
	}
    }
    macro->num = num ? num : ++mnum;
    macro->numnode = inlist((void *)macro, maclist, numnode);

    if (*macro->name) {
        macro->hashnode = hashed_insert((void *)macro, hash, macro_table);
	if (macro->builtin) { /* macro->builtin was set in complete_macro() */
	    macro->builtin->macro = macro;
	}
    }
    if (macro->trig.str) {
        macro->trignode = sinsert((void *)macro,
	    macro->world ? macro->world->triglist : triglist, (Cmp *)rpricmp);
    }
    if (macro->flags & MACRO_HOOK) {
	int i;
	for (i = 0; i < (int)NUM_HOOKS; i++) {
	    if (VEC_ISSET(i, &macro->hook))
		sinsert((void *)macro, &hooklist[i], (Cmp *)rpricmp);
	}
    }
    macro->flags &= ~MACRO_TEMP;
    if (!*macro->name && (macro->trig.str || macro->flags & MACRO_HOOK) &&
	macro->shots == 0 && pedantic)
    {
        wprintf("new macro (#%d) does not have a name.", macro->num);
    }
    return macro->num;
}

/* rebind_key_macros
 * Unbinds macros with keynames, and attempts to rebind them.
 */
void rebind_key_macros(void)
{
    Macro *p;
    ListEntry *node;
    const char *code;

    for (node = maclist->tail; node; node = node->prev) {
        p = MAC(node);
        if (!*p->keyname) continue;
        code = get_keycode(p->keyname);
        if (strcmp(code, p->bind) == 0) {
            /* same code, don't need to rebind */
        } else {
            if (*p->bind) unbind_key(p->bind);
            FREE(p->bind);
            p->bind = STRDUP(code);
            if (!*code) {
                wprintf("no code for key \"%s\"", p->keyname);
            } else if (bind_key_macro(p)) {
		/* bind_key_macro can't return -1 here */
                do_hook(H_REDEF, "!Redefined %s %s", "%s %s",
                    "key", p->keyname);
            } else {
                kill_macro(p);
	    }
        }
    }
}

/* compares m1 and m2 based on reverse priority and fallthru */
static int rpricmp(const Macro *m1, const Macro *m2)
{
    if (m2->pri != m1->pri) return m2->pri - m1->pri;
    else return m2->fallthru - m1->fallthru;
}

struct Value *handle_def_command(String *args, int offset)
{
    Macro *spec;

    if (!(args->len - offset) || !(spec = macro_spec(args, offset, NULL, NULL)))
        return shareval(val_zero);
    return newint(complete_macro(spec, macro_hash(spec->name), 0, NULL));
}

/* Fill in "don't care" fields with default values, and add_numbered_macro().
 * If error checking fails, spec will be nuked.
 */
static int complete_macro(Macro *spec, unsigned int hash, int num,
    ListEntry *numnode)
{
    Macro *orig = NULL;
    int i;

    if (spec->name && *spec->name) {
        if (strchr("#@!/", *spec->name) || strchr(spec->name, ' ')) {
            eprintf("illegal macro name \"%s\".", spec->name);
            nuke_macro(spec);
            return 0;
        }
        if (keyword(spec->name) ||
	    ((spec->builtin = find_builtin_cmd(spec->name)) &&
	    (spec->builtin->reserved)))
	{
            eprintf("\"%s\" is a reserved word.", spec->name);
            nuke_macro(spec);
            return 0;
        }

        if (spec->builtin) {
            do_hook(H_CONFLICT,
                "!warning: macro \"%s\" conflicts with the builtin command.",
                "%s", spec->name);
        }
    }

    if (spec->expr) {
	/* Compiling expr at /def time allows us to report errors at /def time
	 * (and means we don't have to test at runtime) */
	extern char current_opt;
	current_opt = 'E';
	spec->exprprog = compile_tf(spec->expr, 0, -1, 1, 2);
	current_opt = '\0';
	if (!spec->exprprog) {
            nuke_macro(spec);
	    return 0;
	}
    }

    if (spec->body && defcompile) {
	spec->prog = compile_tf(spec->body, 0, SUB_MACRO, 0,
	    !spec->shots ? 2 : spec->shots > 10 ? 1 : 0);
	if (!spec->prog) {
            nuke_macro(spec);
	    return 0;
	}
    }

    if (spec->world == &AnyWorld) spec->world = xworld();
    if (spec->pri < 0) spec->pri = 1;
    if (spec->prob < 0) spec->prob = 100;
    if (spec->shots < 0) spec->shots = 0;
    if (spec->invis) spec->invis = 1;
    if (spec->fallthru < 0) spec->fallthru = 0;
    if (spec->quiet < 0) spec->quiet = 0;
    if (!spec->name) spec->name = STRNDUP("", 0);
    if (!spec->body) (spec->body = blankline)->links++;
    /*if (!spec->expr) (spec->expr = blankline)->links++;*/

    if (spec->nsubattr > 0 && spec->trig.mflag != MATCH_REGEXP) {
        eprintf("\"-P\" requires \"-mregexp -t<pattern>\"");
        nuke_macro(spec);
        return 0;
    }
    spec->attr &= ~F_NONE;
    if (spec->nsubattr) {
	int n = pcre_info(spec->trig.ri->re, NULL, NULL);
	for (i = 0; i < spec->nsubattr; i++) {
	    spec->subattr[i].attr &= ~F_NONE;
	    if (spec->subattr[i].subexp > n) {
		eprintf("-P%d: trigger has only %d subexpressions",
		    spec->subattr[i].subexp, n);
		nuke_macro(spec);
		return 0;
	    }
	}
    }

    if (!spec->keyname) spec->keyname = STRNDUP("", 0);
    else if (*spec->keyname) {
        if (spec->bind) FREE(spec->bind);
        spec->bind = get_keycode(spec->keyname);
        if (!spec->bind) {
            eprintf("unknown key name \"%s\".", spec->keyname);
            nuke_macro(spec);
            return 0;
        }
        spec->bind = STRDUP(spec->bind);
        if (!*spec->bind)
            wprintf("no code for key \"%s\".", spec->keyname);
    }

    if (!spec->bind) spec->bind = STRNDUP("", 0);

    if (*spec->name &&
	(orig = (Macro *)hashed_find(spec->name, hash, macro_table)))
    {
	if (macro_equal(orig, spec)) {
	    /* identical redefinition has no effect */
	    nuke_macro(spec);
	    return orig->num;
	} else if (!redef) {
	    eprintf("macro %s already exists", spec->name);
	    nuke_macro(spec);
	    return 0;
	}
    }
    if (!add_numbered_macro(spec, hash, num, numnode)) return 0;
    if (orig) {
        do_hook(H_REDEF, "!Redefined %s %s", "%s %s", "macro", orig->name);
        kill_macro(orig);
    }
    return spec->num;
}

/* define a new Macro with hook */
int add_hook(char *args, const char *body)
{
    hookvec_t hook;

    VEC_ZERO(&hook);
    if (!parse_hook(&args, &hook)) return 0;
    if (args && !*args) args = NULL;
    return add_new_macro(NULL, "", &hook, args, body, 0, 100, 0, 0, matching);
}

/* /edit: Edit an existing macro.
 * Actually editing the macro in place is quite hairy, so instead we
 * remove the old one, create a replacement and add it.  If the replacement
 * fails, we re-add the original.  Either way, the number and position in
 * maclist are unchanged.
 */
struct Value *handle_edit_command(String *args, int offset)
{
    Macro *spec, *macro = NULL;
    int error = 0;
    int num;
    unsigned int hash = 0;
    ListEntry *numnode;

    if (!(args->len - offset) || !(spec = macro_spec(args, offset, NULL, NULL))) {
        return shareval(val_zero);
    } else if (!spec->name) {
        eprintf("You must specify a macro.");
    } else if (spec->name[0] == '$') {
        macro = match_exact(0, spec->name + 1, F_ATTR);
	if (macro) hash = macro_hash(macro->name);
    } else {
	hash = macro_hash(spec->name); /* used for lookup and insertion */
	if (!(macro = find_hashed_macro(spec->name, hash)))
	    eprintf("macro %s does not exist", spec->name);
    }

    if (!macro) {
        nuke_macro(spec);
        return shareval(val_zero);
    }

    num = macro->num;
    numnode = macro->numnode->prev;
    kill_macro(macro);

    FREE(spec->name);
    spec->name = STRDUP(macro->name);

    if (!spec->body && macro->body) (spec->body = macro->body)->links++;
    if (!spec->expr && macro->expr) (spec->expr = macro->expr)->links++;
    if (!spec->bind && macro->bind) spec->bind = STRDUP(macro->bind);
    if (!spec->keyname && macro->keyname) spec->keyname =STRDUP(macro->keyname);
    if (!spec->wtype.str && macro->wtype.str)
        error += !copy_pattern(&spec->wtype, &macro->wtype);
    if (!spec->trig.str && macro->trig.str)
        error += !copy_pattern(&spec->trig, &macro->trig);
    if (!(spec->flags & (MACRO_HOOK | MACRO_NOHOOK))) {
        spec->flags |= MACRO_HOOK;
	spec->hook = macro->hook;
	if (macro->hargs.str)
	    error += !copy_pattern(&spec->hargs, &macro->hargs);
    }
    if (!spec->world) spec->world = macro->world;
    else if (spec->world == &AnyWorld) spec->world = xworld();
    if (spec->pri < 0) spec->pri = macro->pri;
    if (spec->prob < 0) spec->prob = macro->prob;
    if (spec->shots < 0) spec->shots = macro->shots;
    if (spec->fallthru < 0) spec->fallthru = macro->fallthru;
    if (spec->quiet < 0) spec->quiet = macro->quiet;
    if (spec->attr == 0) spec->attr = macro->attr;
    if (spec->nsubattr == 0 && macro->nsubattr > 0) {
	spec->nsubattr = macro->nsubattr;
	spec->subattr = MALLOC(spec->nsubattr * sizeof(subattr_t));
	memcpy(spec->subattr, macro->subattr,
	    spec->nsubattr * sizeof(subattr_t));
    }

    spec->used[USED_NAME] = macro->used[USED_NAME];
    spec->used[USED_TRIG] = macro->used[USED_TRIG];
    spec->used[USED_HOOK] = macro->used[USED_HOOK];
    spec->used[USED_KEY] = macro->used[USED_KEY];

    if (!error) {
        complete_macro(spec, hash, num, numnode);
        return newint(spec->num);
    }

    /* Edit failed.  Resurrect original macro. */
    macro = dead_macros;
    macro->flags &= ~MACRO_DEAD;
    dead_macros = macro->tnext;
    add_numbered_macro(macro, hash, num, numnode);
    return shareval(val_zero);
}


/********************************
 * Routines for removing macros *
 ********************************/

void kill_macro(Macro *macro)
{
   /* Remove macro from maclist, macro_table, and key_trie, and put it on
    * the dead_macros list.  When called from find_and_run_matches(), this
    * allows a new macro to be defined without conflicting with the name
    * or binding of this macro.
    * The macro must NOT be removed from triglist and hooklist, so
    * find_and_run_matches() can work correctly when a macro kills itself,
    * is a one-shot, or defines another macro that is inserted immediately
    * after it in triglist/hooklist.  macro will be removed from triglist
    * and hooklist in nuke_macro().
    */

    if (macro->flags & MACRO_DEAD) return;
    macro->flags |= MACRO_DEAD;
    macro->tnext = dead_macros;
    dead_macros = macro;
    unlist(macro->numnode, maclist);
    if (*macro->name) hash_remove(macro->hashnode, macro_table);
    if (*macro->bind) unbind_key(macro->bind);
}

void nuke_dead_macros(void)
{
    Macro *macro;

    while ((macro = dead_macros)) {
        dead_macros = dead_macros->tnext;
        nuke_macro(macro);
    }
}

/* free macro structure */
static void nuke_macro(Macro *m)
{
    if (!(m->flags & MACRO_DEAD) && !(m->flags & MACRO_TEMP)) {
        kill_macro(m);
    }
    if (m->trignode)
	unlist(m->trignode, m->world ? m->world->triglist : triglist);
    if (m->flags & MACRO_HOOK) {
	int i;
	ListEntry *node;
	for (i = 0; i < (int)NUM_HOOKS; i++) {
	    if (!VEC_ISSET(i, &m->hook)) continue;
	    for (node = hooklist[i].head; node; node = node->next) {
		if (MAC(node) == m) {
		    unlist(node, &hooklist[i]);
		    break; /* macro can only be in hooklist[i] once */
		}
	    }
	}
    }

    if (m->body) conStringfree(m->body);
    if (m->expr) conStringfree(m->expr);
    if (m->bind) FREE(m->bind);
    if (m->keyname) FREE(m->keyname);
    if (m->subattr) FREE(m->subattr);
    if (m->prog) prog_free(m->prog);
    if (m->exprprog) prog_free(m->exprprog);
    if (m->builtin && m->builtin->macro == m)
	m->builtin->macro = NULL;
    free_pattern(&m->trig);
    free_pattern(&m->hargs);
    free_pattern(&m->wtype);
    if (m->name) FREE(m->name);
    FREE(m);
}

/* delete a macro */
int remove_macro_by_name(const char *str)
{
    Macro *macro;

    if (!(macro = find_macro(str))) {
	eprintf("Macro \"%s\" was not defined.", str);
	return 0;
    }
    kill_macro(macro);
    return 1;
}

/* delete specified macros */
struct Value *handle_purge_command(String *args, int offset)
{
    Macro *spec;
    ListEntry *node, *next;
    int result = 0;
    int mflag;
    AuxPat aux;

    if (!(spec = macro_spec(args, offset, &mflag, NULL)))
        return shareval(val_zero);
    if (spec->name && *spec->name == '#') {
        spec->num = atoi(spec->name + 1);
        FREE(spec->name);
        spec->name = NULL;
    }
    if (!(init_aux_patterns(spec, mflag, &aux)))
	goto error;
    for (node = maclist->head; node; node = next) {
	next = node->next;
	if (macro_match(spec, MAC(node), &aux)) {
	    kill_macro(MAC(node));
	    result++;
	}
    }
    /* regrelease(); */
error:
    free_aux_patterns(&aux);
    nuke_macro(spec);
    return newint(result);
}

/* delete macro by number */
struct Value *handle_undefn_command(String *args, int offset)
{
    int num, result = 0;
    Macro *macro;
    const char *ptr = args->data + offset;

    while (*ptr) {
        if ((num = numarg(&ptr)) >= 0 && (macro = find_num_macro(num))) {
            kill_macro(macro);
            result++;
        }
    }
    return newint(result);
}

Macro *find_num_macro(int num)
{
    if (maclist->tail && num >= MAC(maclist->tail)->num) {
	/* search from high end, assuming high macros are more likely;
	 * stop when past */
	ListEntry *node = maclist->head;
	for ( ; node && MAC(node)->num >= num; node = node->next)
	    if (MAC(node)->num == num) return MAC(node);
    }
    eprintf("no macro with number %d", num);
    return NULL;
}

void remove_world_macros(World *w)
{
    ListEntry *node, *next;

    /* This could be more efficient by using w->triglist and
     * the hooklists, but this is not used often. */
    for (node = maclist->head; node; node = next) {
        next = node->next;
	if (MAC(node)->world == w)
	    kill_macro(MAC(node));
    }
}


/**************************
 * Routine to list macros *
 **************************/

/* convert hook vector to string */
static const String *hook_name(const hookvec_t *hook)
{
    int i;
    STATIC_BUFFER(buf);

    Stringtrunc(buf, 0);
    for (i = 0; i < (int)NUM_HOOKS; i++) {
                 /* ^^^^^ Some brain dead compilers need that cast */
	if (!VEC_ISSET(i, hook)) continue;
        if (buf->len) Stringadd(buf, '|');
        Stringcat(buf, hook_table[i].name);
    }
    return buf;
}

static conString *print_def(TFILE *file, String *buffer, Macro *p)
{
    int mflag = -1;

    if (!buffer)
	buffer = Stringnew(NULL, 0, 0);
    buffer->links++;

    if (file && file == tfout)
	Sprintf(buffer, "%% %d: /def ", p->num);
    else Stringcpy(buffer, "/def ");
    if (p->invis) Stringcat(buffer, "-i ");
    if (p->trig.str || (p->flags & MACRO_HOOK))
	Sappendf(buffer, "-%sp%d ", p->fallthru ? "F" : "", p->pri);
    if (p->prob != 100)
	Sappendf(buffer, "-c%d ", p->prob);
    if (p->attr) {
	Stringcat(attr2str(Stringcat(buffer, "-a"), p->attr), " ");
    }
    if (p->nsubattr > 0) {
	int i;
	mflag = MATCH_REGEXP;
	Stringcat(buffer, "-P");
	for (i = 0; i < p->nsubattr; i++) {
	    if (i > 0) Stringadd(buffer, ';');
	    if (p->subattr[i].subexp == -1)
		Stringadd(buffer, 'L');
	    else if (p->subattr[i].subexp == -2)
		Stringadd(buffer, 'R');
	    else
		Sappendf(buffer, "%d", (int)p->subattr[i].subexp);
	    attr2str(buffer, p->subattr[i].attr);
	}
	Stringadd(buffer, ' ');
    }
    if (p->shots)
	Sappendf(buffer, "-n%d ", p->shots);
    if (p->world)
	Sappendf(buffer, "-w'%q' ", '\'', p->world->name);
    if (p->wtype.str) {
	if (p->wtype.mflag != mflag)
	    Sappendf(buffer, "-m%S ", &enum_match[p->wtype.mflag]);
	mflag = p->wtype.mflag;
	Sappendf(buffer, "-T'%q' ", '\'', p->wtype.str);
    }

    if (p->expr) 
	Sappendf(buffer, "-E'%q' ", '\'', p->expr->data);

    if (p->trig.str) {
	if (p->trig.mflag != mflag)
	    Sappendf(buffer, "-m%S ", &enum_match[p->trig.mflag]);
	mflag = p->trig.mflag;
	Sappendf(buffer, "-t'%q' ", '\'', p->trig.str);
    }
    if (p->flags & MACRO_HOOK) {
	if (p->hargs.str && *p->hargs.str) {
	    if (p->hargs.mflag != mflag)
		Sappendf(buffer, "-m%S ", &enum_match[mflag = p->hargs.mflag]);
	    Sappendf(buffer, "-h'%S %q' ",
		hook_name(&p->hook), '\'', p->hargs.str);
	} else {
	    Sappendf(buffer, "-h%S ", hook_name(&p->hook));
	}
    }

#if 0 /* obsolete */
    if (*p->keyname) 
	Sappendf(buffer, "-B'%s' ", p->keyname);
    else
#endif
    if (*p->bind) 
	Sappendf(buffer, "-b'%q' ", '\'', ascii_to_print(p->bind)->data);

    if (p->quiet) Stringcat(buffer, "-q ");
    if (*p->name == '-') Stringcat(buffer, "- ");
    if (*p->name) Sappendf(buffer, "%s ", p->name);
    if (p->body && p->body->len) Sappendf(buffer, "= %S", p->body);

    if (file) {
	tfputline(CS(buffer), file);
	Stringfree(buffer);
	return NULL;
    } else {
	return CS(buffer);
    }
}

/* list all specified macros */
static int list_defs(TFILE *file, Macro *spec, int mflag, ListOpts *listopts)
{
    Macro *p;
    ListEntry *node;
    AuxPat aux;
    String *buffer = NULL;
    int result = 0, i;
    Vector macs = vector_init(1024);

    if (!(init_aux_patterns(spec, mflag, &aux))) goto error;
    if (spec->name && *spec->name == '#') {
        spec->num = atoi(spec->name + 1);
        FREE(spec->name);
        spec->name = NULL;
    }

    /* maclist is in reverse numeric order, so we start from tail */
    for (node = maclist->tail; node; node = node->prev) {
        p = MAC(node);
        if (!macro_match(spec, p, &aux)) continue;
	vector_add(&macs, p);
    }

    if (listopts && listopts->cmp)
	vector_sort(&macs, listopts->cmp);

    for (i = 0; i < macs.size; i++) {
        p = macs.ptrs[i];
        result = p->num;

        if (!buffer)
            (buffer = Stringnew(NULL, 0, 0))->links++;

        if (listopts && listopts->shortflag) {
            Sprintf(buffer, "%% %d: ", p->num);
            if (p->attr & F_NOHISTORY) Stringcat(buffer, "(nohistory) ");
            if (p->attr & F_NOLOG) Stringcat(buffer, "(nolog) ");
            if (p->attr & F_GAG) Stringcat(buffer, "(gag) ");
            else if (p->attr & (F_HWRITE | F_EXCLUSIVE)) {
                if (p->attr & F_UNDERLINE) Stringcat(buffer, "(underline) ");
                if (p->attr & F_REVERSE)   Stringcat(buffer, "(reverse) ");
                if (p->attr & F_FLASH)     Stringcat(buffer, "(flash) ");
                if (p->attr & F_DIM)       Stringcat(buffer, "(dim) ");
                if (p->attr & F_BOLD)      Stringcat(buffer, "(bold) ");
                if (p->attr & F_BELL)      Stringcat(buffer, "(bell) ");
                if (p->attr & F_HILITE)    Stringcat(buffer, "(hilite) ");
                if (p->attr & F_FGCOLOR)
                    Sappendf(buffer, "(%S) ",
                        &enum_color[attr2fgcolor(p->attr)]);
                if (p->attr & F_BGCOLOR)
                    Sappendf(buffer, "(bg%S) ",
                        &enum_color[attr2bgcolor(p->attr)]);
            } else if (p->nsubattr > 0) {
                Stringcat(buffer, "(partial) ");
            } else if (p->trig.str) {
		Stringcat(buffer, "(trig");
		if (listopts && listopts->usedflag)
		    Sappendf(buffer, " %d", p->used[USED_TRIG]);
		Stringcat(buffer, ") ");
            }
            if (p->trig.str)
                Sappendf(buffer, "'%q' ", '\'', p->trig.str);
#if 0 /* obsolete */
            if (*p->keyname) {
		Stringcat(buffer, "(key");
		if (listopts && listopts->usedflag)
		    Sappendf(buffer, " %d", p->used[USED_KEY]);
                Sappendf(buffer, ") '%s' ", p->keyname);
            } else
#endif
	    if (*p->bind) {
		Stringcat(buffer, "(bind");
		if (listopts && listopts->usedflag)
		    Sappendf(buffer, " %d", p->used[USED_KEY]);
                Sappendf(buffer, ") '%q' ", '\'',
                    ascii_to_print(p->bind)->data);
                if (p->keyname && *p->keyname)
		    Sappendf(buffer, "(%s) ", p->keyname);
            }
            if (p->flags & MACRO_HOOK) {
		Stringcat(buffer, "(hook");
		if (listopts && listopts->usedflag)
		    Sappendf(buffer, " %d", p->used[USED_HOOK]);
                Sappendf(buffer, ") %S ", hook_name(&p->hook));
	    }
            if (*p->name) {
		Sappendf(buffer, "%s ", p->name);
		if (listopts && listopts->usedflag)
		    Sappendf(buffer, "(%d) ", p->used[USED_NAME]);
	    }
	    tfputline(CS(buffer), file ? file : tfout);

        } else {
	    print_def(file ? file : tfout, buffer, p);
        }

        /* If something is sharing buffer, we can't reuse it in next loop. */
        if (buffer->links > 1) {
            Stringfree(buffer);
            buffer = NULL;
        }
    }
    /* regrelease(); */
    vector_free(&macs);
error:
    free_aux_patterns(&aux);
    if (buffer) {
        Stringfree(buffer);
        buffer = NULL;
    }
    return result;
}

/* write specified macros to file */
int save_macros(String *args, int offset)
{
    Macro *spec;
    TFILE *file = NULL;
    int result = 1;
    const char *name, *mode = "w";
    char opt, *next;
    int mflag;

    startopt(CS(args), "a");
    while ((opt = nextopt(NULL, NULL, NULL, &offset))) {
        if (opt != 'a') return 0;
        mode = "a";
    }

    next = args->data + offset;
    name = stringarg(&next, NULL);
    offset = next - args->data;
    if (!(spec = macro_spec(args, offset, &mflag, NULL))) result = 0;
    if (result && !(file = tfopen(expand_filename(name), mode))) {
        operror(name);
        result = 0;
    }

    if (result) {
        oprintf("%% %sing macros to %s", *mode=='w' ? "Writ" : "Append",
            file->name);
        result = list_defs(file, spec, mflag, NULL);
    }
    if (file) tfclose(file);
    if (spec) nuke_macro(spec);
    return result;
}

/* list macros on screen */
struct Value *handle_list_command(String *args, int offset)
{
    Macro *spec;
    int result = 1;
    int mflag;
    ListOpts opts = { 0, 0, NULL };

    if (!(spec = macro_spec(args, offset, &mflag, &opts))) result = 0;
    if (result) result = list_defs(NULL, spec, mflag, &opts);
    if (spec) nuke_macro(spec);
    return newint(result);
}


/**************************
 * Routines to use macros *
 **************************/

/* Do a macro! */
int do_macro(Macro *macro, String *args, int offset, int used_type,
    int kbnumlocal)
{
    int result, old_invis_flag, oldblock;
    const char *command;
    char numbuf[16];

    if (*macro->name) {
        command = macro->name;
    } else {
        sprintf(numbuf, "#%d", macro->num);
        command = numbuf;
    }
    if (used_type >= 0) {
	macro->used[used_type]++;
    }
    old_invis_flag = invis_flag;
    invis_flag = macro->invis;
    oldblock = block; /* XXX ? */
    block = 0; /* XXX ? */
    if (!macro->prog) {
	const char *old_command = current_command;
	current_command = command; /* for macro compiler errors */
	macro->prog = compile_tf(macro->body, 0, SUB_MACRO, 0,
	    !macro->shots ? 2 : macro->shots > 10 ? 1 : 0);
	current_command = old_command;
    }
    if (!macro->prog)
	result = 0;
    else
	result = prog_run(macro->prog, args, offset, command, kbnumlocal);
    invis_flag = old_invis_flag;
    block = oldblock; /* XXX ? */
    return result;
}

/* get body of macro */
const char *macro_body(const char *name)
{
    Macro *m;
    const char *body;

    if (!name) return NULL;
    if (strncmp("world_", name, 6) == 0 && (body = world_info(NULL, name + 6)))
        return body;
    if (!(m = find_macro(name))) return NULL;
    return m->body ? m->body->data : "";
}


/****************************************
 * Routines to check triggers and hooks *
 ****************************************/

/* do_hook
 * Call macros that match <hooknum> and optionally the filled-in <argfmt>, and
 * prints the message in <fmt>.  Returns the number of matches that were run.
 * A leading '!' in <fmt> is replaced with "% ", file name, and line number.
 * Note that calling do_hook() makes the caller non-atomic; be careful.
 */
int do_hook(int hooknum, const char *fmt, const char *argfmt, ...)
{
    va_list ap;
    int ran = 0;
    String *line = NULL;
    String *args = NULL;
    /* do_hook is re-entrant, so we can't use static buffer.  macro regexps
     * may save a pointer to args, so we can't even use an auto buffer. */

    va_start(ap, argfmt);
    if (hookflag || hilite || gag) {
        (args = Stringnew(NULL, 96, 0))->links++;
        vSprintf(args, 0, argfmt, ap);
    }
    va_end(ap);

    if (fmt) {
        (line = Stringnew(NULL, 96, 0))->links++;
        if (*fmt == '!') {
            eprefix(line);
            fmt++;
        }
        va_start(ap, argfmt);
        vSprintf(line, SP_APPEND, fmt, ap);
        va_end(ap);
    }

    if (hookflag || hilite || gag) {
        ran = find_and_run_matches(args, hooknum, &line, xworld(), TRUE, 0);
        Stringfree(args);
    }

    if (line) {
        Stringfree(line);
    }
    return ran;
}

/* Find and run one or more matches for a hook or trig.
 * text is text to be matched; if NULL, *linep is used.
 * If %Pn subs are to be allowed, text should be NULL.
 * If <hooknum> is <0, this looks for a trigger;
 * if <hooknum> is >=0 it is a hook number.  If <linep> is non-NULL,
 * attributes of matching macros will be applied to *<linep>.
 */
int find_and_run_matches(String *text, int hooknum, String **linep,
    World *world, int globalflag, int exec_list_long)
{
    Queue runq[1];			    /* queue of macros to run */
    Macro *nonfallthru = NULL;		    /* list of non fall-thrus */
    int num = 0;                            /* # of non-fall-thrus */
    int ran = 0;                            /* # of executed macros */
    int lowerlimit = -1;                    /* lowest priority that can match */
    int header = 0;			    /* which headers have we printed? */
    ListEntry *gnode, *wnode, **nodep;
    Pattern *pattern;
    Macro *macro;
    const char *worldtype = NULL;

    /* Macros are sorted by decreasing priority, with fall-thrus first.  So,
     * we search the global and world lists in parallel.  For each matching
     * fall-thru, we apply its attributes; if it's a hook, we add it to a
     * queue, if it's a trigger, we execute it immediately.  When we find a
     * matching non-fall-thru, we collect a list of other non-fall-thru
     * matches of the same priority and select one, and apply its attributes;
     * again, if it's a hook, we add it to queue, if it's a trigger, we
     * execute.
     * Then, we print the line.
     * Then, if macros were queued (because this is a hook), we run all the
     * queued macros.
     * The point of the queue is so the line can be printed before any output
     * generated by the macros.  We would like to do this for triggers as well
     * as hooks, but then /substitute wouldn't work.
     */
    /* Note: kill_macro() does not remove macros from any lists, so this will
     * work correctly when a macro kills itself, or inserts a new macro just
     * after itself in a list.
     */

    if (world)
	worldtype = world_type(world);
    if (!worldtype)
	worldtype = "";
    if (!text)
        text = *linep;
    text->links++; /* in case substitute() frees text */
    recur_count++;
    if (hooknum>=0) {
	gnode = hooklist[hooknum].head;
	wnode = NULL;
    } else {
	gnode = triglist->head;
	wnode = world ? world->triglist->head : NULL;
    }

    if (exec_list_long == 0) {
	init_queue(runq);
    }

    while (gnode || wnode) {
	nodep = (!wnode) ? &gnode : (!gnode) ? &wnode :
	    (rpricmp(MAC(wnode), MAC(gnode)) > 0) ? &gnode : &wnode;
	macro = MAC(*nodep);
	*nodep = (*nodep)->next;

	if (macro->pri < lowerlimit && exec_list_long == 0)
	    break;
        if (macro->flags & MACRO_DEAD) continue;
        if (!(
	    (hooknum<0 && (
		(borg && macro->body && (macro->prob > 0)) ||
		(hilite && ((macro->attr & F_HWRITE) || macro->nsubattr)) ||
		(gag && (macro->attr & F_GAG)))) ||
	    (hooknum>=0 && VEC_ISSET(hooknum, &macro->hook))))
	{
	    continue;
	}

	/* triggers are listed by world, but hooks are not, so we must check */
        if (macro->world && macro->world != world) continue;

        if (!globalflag && !macro->world) continue;
        if (macro->wtype.str) {
            if (!world) continue;
            if (!patmatch(&macro->wtype, NULL, worldtype))
                continue;
        }
        if (macro->exprprog) {
            struct Value *result = NULL;
            int expr_condition;
	    result = expr_value_safe(macro->exprprog);
            expr_condition = valbool(result);
            freeval(result);
            if (!expr_condition) continue;
        }
        pattern = hooknum>=0 ? &macro->hargs : &macro->trig;
        if ((hooknum>=0 && !macro->hargs.str) || patmatch(pattern, CS(text), NULL))
	{
	    if (exec_list_long == 0) {
		if (macro->fallthru) {
		    if (linep && *linep)
			apply_attrs_of_match(macro, text, hooknum, *linep);
		    if (hooknum>=0) {
			enqueue(runq, macro);
		    } else {
			ran += run_match(macro, text, hooknum);
			if (linep && hooknum<0) {
			    /* in case of /substitute */ /* XXX */
			    Stringfree(text);
			    text = *linep;
			    text->links++;
			}
		    }
		} else {
		    /* collect list of non-fall-thru matches */
		    lowerlimit = macro->pri;
		    num++;
		    macro->tnext = nonfallthru;
		    nonfallthru = macro;
		}
	    } else {
		ran += (lowerlimit < 0);
		if (header < 3 && macro->pri < lowerlimit) {
		    oputs("% The following matching macros would not be "
			"applied:");
		    header = 3;
		}
		if (header < 2 && !macro->fallthru) {
		    oprintf("%% One of the following macros would %sbe "
			"applied:", ran > 1 ? "also " : "");
		    lowerlimit = macro->pri;
		    header = 2;
		}
		if (header < 1) {
		    oputs("% All of the following macros would be applied:");
		    header = 1;
		}
		if (exec_list_long > 1)
		    print_def(tfout, NULL, macro);
		else if (macro->name)
		    oprintf("%% %c %10d %s", macro->fallthru ? 'F' : ' ',
			macro->pri, macro->name);
		else
		    oprintf("%% %c %10d #%d", macro->fallthru ? 'F' :' ',
			macro->pri, macro->num);
	    }
        }
    }

    if (exec_list_long == 0) {
	/* select exactly one of the non fall-thrus. */
	if (num > 0) {
	    for (macro = nonfallthru, num = RRAND(0, num-1); num; num--)
		macro = macro->tnext;
	    if (linep && *linep)
		apply_attrs_of_match(macro, text, hooknum, *linep);
	    if (hooknum>=0) {
		enqueue(runq, macro);
	    } else {
		ran += run_match(macro, text, hooknum);
	    }
	}

	/* print the line! */
	if (hooknum>=0 && linep && *linep) {
	    if (hook_table[hooknum].hooktype & HT_ALERT) {
		alert(CS(*linep));
	    } else if (hook_table[hooknum].hooktype & HT_WORLD) {
		/* Note: world_output() must come before alert(), otherwise
		 * world_output() could cause an activity hook that would
		 * clobber the alert */
		if (xworld())
		    world_output(xworld(), CS(*linep));
		if (!xsock_is_fg()) {
		    alert(CS(*linep));
		}
	    } else {
		tfputline(CS(*linep), tferr); /* XXX conStr bug??? */
	    }
	    if (hook_table[hooknum].hooktype & HT_XSOCK) {
		xsock_alert_id(); /* alert should clear when xsock is fg'd */
	    }
	}

	/* run all of the queued macros */
	while ((macro = (Macro*)dequeue(runq))) {
	    ran += run_match(macro, text, hooknum);
	}
    } else {
	oprintf("%% %s would have %s %d macro%s.",
	    hooknum>=0 ? "Event" : "Text", hooknum>=0 ? "hooked" : "triggered",
	    ran, (ran != 1) ? "s" : "");
    }

    recur_count--;
    Stringfree(text);
    return ran;
}


/* apply attributes of a macro that has been selected by a trigger or hook */
static void apply_attrs_of_match(
    Macro *macro,	/* macro to apply */
    String *text,	/* argument text that matched trigger/hook */
    int hooknum,	/* hook number */
    String *line)	/* line to which attributes are applied */
{
    RegInfo *old, *ri;

    if (!hilite && !gag) return;

    /* Apply attributes (full and partial) to line. */
    if (!hilite)
	line->attrs = adj_attr(line->attrs, macro->attr & F_GAG);
    else if (!gag)
	line->attrs = adj_attr(line->attrs, macro->attr & ~F_GAG);
    else
	line->attrs = adj_attr(line->attrs, macro->attr);

    ri = (hooknum>=0 ? macro->hargs : macro->trig).ri;

    if (hooknum<0 && macro->trig.mflag == MATCH_REGEXP && line->len && hilite
	&& macro->nsubattr)
    {
	int i, x, offset = 0;
	int start, end;
	int *saved_ovector = NULL;

	if (text)
	    old = new_reg_scope(ri, text);

	check_charattrs(line, line->len, 0, __FILE__, __LINE__);
	do {
	    for (x = 0; x < macro->nsubattr; x++) {
		if (macro->subattr[x].subexp == -1) {
		    start = 0;
		    end = ri->ovector[0];
		} else if (macro->subattr[x].subexp == -2) {
		    start = ri->ovector[1];
		    end = line->len;
		} else {
		    start = ri->ovector[macro->subattr[x].subexp * 2];
		    end = ri->ovector[macro->subattr[x].subexp * 2 + 1];
		}
		for (i = start; i < end; ++i)
		    line->charattrs[i] =
			adj_attr(line->charattrs[i], macro->subattr[x].attr);
	    }
	    if (offset == ri->ovector[1]) break; /* offset wouldn't move */
	    offset = ri->ovector[1];
	    if (!saved_ovector) {
		saved_ovector = ri->ovector;
		ri->ovector = NULL;
	    }
	} while (offset < line->len &&
	    tf_reg_exec(ri, CS(text), NULL, offset) > 0);
	/* restore original startp/endp */
	if (saved_ovector) {
	    if (ri->ovector) FREE(ri->ovector);
	    ri->ovector = saved_ovector;
	}
	(ri->Str = CS(line))->links++;

	if (text)
	    restore_reg_scope(old);
    }
}

typedef struct max_ctr {
    int bucket[2];
    int count[2];
    const int maxid, flagid;
    const char *label;
} max_ctr_t;

static max_ctr_t trig_ctr = {{0,0}, {0,0}, VAR_max_trig, VAR_borg, "Trigger"};
static max_ctr_t hook_ctr = {{0,0}, {0,0}, VAR_max_hook, VAR_hook, "Hook"};

static int test_max_counter(max_ctr_t *c)
{
    static int bucketsize = 5;
    int now = time(NULL) / bucketsize;
    if (now != c->bucket[1]) {
	c->bucket[0] = now - 1;
	c->bucket[1] = now;
	c->count[0] = (now == c->bucket[1] + 1) ? c->count[1] : 1;
	c->count[1] = 0;
    }
    if (c->count[0] + ++c->count[1] > special_var[c->maxid].val.u.ival) {
	set_var_by_id(c->flagid, 0);
	eprintf("%s rate exceeded %%%s (%d) in less than %ds.  "
	    "Setting %%%s off.", c->label,
	    special_var[c->maxid].val.name, special_var[c->maxid].val.u.ival,
	    2*bucketsize, special_var[c->flagid].val.name);
	return 0;
    }
    return 1;
}

/* run a macro that has been selected by a trigger or hook */
static int run_match(
    Macro *macro,	/* macro to run */
    String *text,	/* argument text that matched trigger/hook */
    int hooknum)	/* hook number */
{
    int ran = 0;
    struct Sock *callingsock = xsock;
    RegInfo *old;

    if (hooknum < 0) { /* trigger */
	if (!borg) return 0;
	if (text && max_trig > 0 && !test_max_counter(&trig_ctr)) return 0;
    } else { /* hook */
	if (max_hook > 0 && !test_max_counter(&hook_ctr)) return 0;
    }

    if (text)
        old = new_reg_scope(hooknum>=0 ? macro->hargs.ri : macro->trig.ri, text);

    /* Execute the macro. */
    if ((hooknum>=0 && hookflag) || (hooknum<0 && borg)) {
	callingsock = xsock;
        if (macro->prob == 100 || RRAND(0, 99) < macro->prob) {
            if (macro->shots && !--macro->shots) kill_macro(macro);
            if (mecho > macro->invis) {
                char numbuf[16];
                if (!*macro->name) sprintf(numbuf, "#%d", macro->num);
                tfprintf(tferr, "%S%s%s: /%s %S%A", do_mprefix(),
                    hooknum>=0 ? hook_table[hooknum].name : "",
                    hooknum>=0 ? " HOOK" : "TRIGGER",
                    *macro->name ? macro->name : numbuf,
		    text, mecho_attr);
            }
            if (macro->body && macro->body->len) {
                do_macro(macro, text, 0, hooknum>=0 ? USED_HOOK : USED_TRIG, 0);
                ran += !macro->quiet;
            }
        }

	/* Restore xsock, in case macro called fg_sock().  main_loop() will
	 * set xsock=fsock, so any fg_sock() will effect xsock after the
	 * find_and_run_matches() loop is complete.
	 */
	xsock = callingsock;
    }

    if (text)
        restore_reg_scope(old);
    return ran;
}

#if USE_DMALLOC
void free_macros(void)
{
    while (maclist->head) nuke_macro((Macro *)maclist->head->datum);
    free_hash(macro_table);
}
#endif

