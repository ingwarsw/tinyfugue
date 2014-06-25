/*************************************************************************
 *  TinyFugue - programmable mud client
 *  Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2002, 2003, 2004, 2005, 2006-2007 Ken Keys
 *
 *  TinyFugue (aka "tf") is protected under the terms of the GNU
 *  General Public License.  See the file "COPYING" for details.
 ************************************************************************/
static const char RCSid[] = "$Id: world.c,v 35004.77 2007/01/13 23:12:39 kkeys Exp $";


/********************************************************
 * Fugue world routines.                                *
 ********************************************************/

#include "tfconfig.h"
#include "port.h"
#include "tf.h"
#include "util.h"
#include "pattern.h"
#include "search.h"	/* for tfio.h */
#include "tfio.h"
#include "history.h"
#include "world.h"
#include "process.h"
#include "macro.h"	/* remove_world_macros() */
#include "cmdlist.h"
#include "socket.h"
#include "output.h"	/* columns */

#define LW_TABLE	001
#define LW_UNNAMED	002
#define LW_SHORT	004

static int  list_worlds(const Pattern *name, const Pattern *type,
    struct TFILE *file, int flags, int (*cmp)(const void *, const void *));
static void free_world(World *w);
static World *alloc_world(void);

static World *hworld = NULL;

World *defaultworld = NULL;


static void free_world(World *w)
{
    if (w->name)      FREE(w->name);
    if (w->character) FREE(w->character);
    if (w->pass)      FREE(w->pass);
    if (w->host)      FREE(w->host);
    if (w->port)      FREE(w->port);
    if (w->mfile)     FREE(w->mfile);
    if (w->type)      FREE(w->type);
    if (w->myhost)    FREE(w->myhost);
    if (w->screen)    free_screen(w->screen);
#if !NO_HISTORY
    if (w->history) {
        free_history(w->history);
        FREE(w->history);
    }
#endif
    FREE(w);
}

#if USE_DMALLOC
void free_worlds(void)
{
    World *next;

    if (defaultworld)
        free_world(defaultworld);
    for ( ; hworld; hworld = next) {
        next = hworld->next;
        free_world(hworld);
    }
}
#endif

static World *alloc_world(void)
{
    World *result;
    result = (World *) XMALLOC(sizeof(World));
    memset(result, 0, sizeof(World));
    return result;
}

/* A NULL name means unnamed; world will be given a temp name. */
World *new_world(const char *name, const char *type,
    const char *host, const char *port,
    const char *character, const char *pass,
    const char *mfile, int flags,
    const char *myhost)
{
    World *result;
    static int unnamed = 1;
    smallstr buffer;
    int is_redef = FALSE;

    /* unnamed worlds can't be defined but can have other fields changed. */
    if (name && (!*name || strchr(name, ' ') ||
	(*name == '(' && (*host || *port))))
    {
        eprintf("illegal world name: %s", name);
        return NULL;
    }

    if (name && cstrcmp(name, "default") == 0) {
        if (defaultworld) {
	    /* redefine existing default world */
            result = defaultworld;
            FREE(defaultworld->name);
            is_redef = TRUE;
        } else {
	    /* define default world */
            result = defaultworld = alloc_world();
        }

    } else if (name && (result = find_world(name))) {
	/* redefine existing world */
        FREE(result->name);
        is_redef = TRUE;

    } else {
	/* define new world */
        World **pp;
	if ((!*host && *port) || (*host && !*port)) {
	    eprintf("world %s: host and port must be both blank or both "
		"non-blank.", name);
	    return NULL;
	}
        for (pp = &hworld; *pp; pp = &(*pp)->next);
        *pp = result = alloc_world();
	init_list(result->triglist);
	init_list(result->hooklist);
#if !NO_HISTORY
        /* Don't allocate the history's queue until we actually need it. */
        result->history = init_history(NULL, 0);
#endif
    }

    if (name) {
        result->name = STRDUP(name);
    } else {
        sprintf(buffer, "(unnamed%d)", unnamed++);
        result->name = STRDUP(buffer);
    }

#define setfield(field) \
    do { \
        if (field && *field) { \
            if (result->field) FREE(result->field); \
            result->field = STRDUP(field); \
        } \
    } while (0);

    setfield(character);
    setfield(pass);
    setfield(host);
    setfield(port);
    setfield(mfile);
    setfield(type);
    setfield(myhost);
    result->flags |= flags;

#ifdef PLATFORM_UNIX
# ifndef __CYGWIN32__
    if (pass && *pass && loadfile && (loadfile->mode & (S_IROTH | S_IRGRP)) &&
        !loadfile->warned)
    {
        wprintf("file contains passwords and is readable by others.");
        loadfile->warned++;
    }
# endif /* __CYGWIN32__ */
#endif /* PLATFORM_UNIX */

    if (is_redef)
        do_hook(H_REDEF, "!Redefined %s %s", "%s %s", "world", result->name);
    return result;
}

/* should not be called for defaultworld */
int nuke_world(World *w)
{
    World **pp;

    if (w->sock) {
        eprintf("%s is in use.", w->name);
        return 0;
    }

    for (pp = &hworld; *pp != w; pp = &(*pp)->next);
    *pp = w->next;
    remove_world_macros(w);
    kill_procs_by_world(w);
    free_world(w);
    return 1;
}

struct Value *handle_unworld_command(String *args, int offset)
{
    World *w;
    int result = 0;
    const char *name;
    char *ptr = args->data + offset;

    while (*(name = stringarg(&ptr, NULL))) {
        if (defaultworld && cstrcmp(name, "default") == 0) {
            free_world(defaultworld);
            defaultworld = NULL;
        } else if ((w = find_world(name))) {
            result += nuke_world(w);
        } else {
            eprintf("No world %s", name);
        }
    }
    return newint(result);
}

static int worldtypecmp(const void *a, const void *b) {
    return nullcstrcmp((*(World**)a)->type, (*(World**)b)->type);
}

static int worldhostcmp(const void *a, const void *b) {
    return nullcstrcmp((*(World**)a)->host, (*(World**)b)->host);
}

static int worldportcmp(const void *a, const void *b) {
    return nullcstrcmp((*(World**)a)->port, (*(World**)b)->port);
}

static int worldcharcmp(const void *a, const void *b) {
    return nullcstrcmp((*(World**)a)->character, (*(World**)b)->character);
}

struct Value *handle_listworlds_command(String *args, int offset)
{
    int flags = LW_TABLE, mflag = matching, error = 0, result, n;
    char c;
    const char *ptr;
    int (*cmp)(const void *, const void *) = cstrpppcmp;
    Pattern type, name, *namep = NULL;

    init_pattern_str(&type, NULL);
    init_pattern_str(&name, NULL);
    startopt(CS(args), "T:uscm:S:");
    while ((c = nextopt(&ptr, NULL, NULL, &offset))) {
        switch (c) {
            case 'T':	free_pattern(&type);
			error += !init_pattern_str(&type, ptr);
			break;
            case 'u':	flags |= LW_UNNAMED; break;
            case 's':	flags |= LW_SHORT;   /* fall through */
            case 'c':	flags &= ~LW_TABLE;  break;
            case 'm':	error = ((mflag = enum2int(ptr,0,enum_match,"-m")) < 0);
			break;
	    case 'S':	n = strlen(ptr);
			if (n == 0 || cstrncmp(ptr, "name", n) == 0)
			    cmp = cstrpppcmp;
			else if (cstrncmp(ptr, "type", n) == 0)
			    cmp = worldtypecmp;
			else if (cstrncmp(ptr, "host", n) == 0)
			    cmp = worldhostcmp;
			else if (cstrncmp(ptr, "port", n) == 0)
			    cmp = worldportcmp;
			else if (cstrncmp(ptr, "character", n) == 0)
			    cmp = worldcharcmp;
			else if (cstrncmp(ptr, "-", n) == 0)
			    cmp = NULL;
			else { eprintf("invalid sort criterion"); error++; }
			break;
            default:	return shareval(val_zero);
        }
    }
    if (error) return shareval(val_zero);
    init_pattern_mflag(&type, mflag, 'T');
    Stringstriptrail(args);
    if (args->len > offset) {
        namep = &name;
        error += !init_pattern(namep, args->data + offset, mflag);
    }
    if (error) return shareval(val_zero);
    result = list_worlds(namep, type.str?&type:NULL, tfout, flags, cmp);
    free_pattern(&name);
    free_pattern(&type);
    return newint(result);
}

static int list_worlds(const Pattern *name, const Pattern *type, TFILE *file,
    int flags, int (*cmp)(const void *, const void *))
{
    World *p;
    Vector worlds = vector_init(128);
    int first = 1, i;
    int need, width, width_name, width_type, width_host, width_port;
    STATIC_BUFFER(buf);

    if (flags & LW_TABLE) {
        width = (columns < 80 ? 80 : columns) - 1;
        width_name = width / 5;
        width_type = width / 7;
        width_host = width / 3;
        width_port = 6;
        tfprintf(file, "%-*s %-*s %*s %-*s %s",
            width_name, "NAME", width_type, "TYPE", width_host, "HOST",
            width_port, "PORT", "CHARACTER");
    }

    /* collect matching worlds */
    for (p = defaultworld; p || first; p = first ? hworld : p->next, first=0)
    {
        if (!p || (!(flags & LW_UNNAMED) && p->flags & WORLD_TEMP)) continue;
        if (name && !patmatch(name, NULL, p->name)) continue;
        if (type && !patmatch(type, NULL, p->type ? p->type : "")) continue;
	vector_add(&worlds, p);
    }

    if (cmp)
	vector_sort(&worlds, cmp);

    Stringtrunc(buf, 0);
    for (i = 0; i < worlds.size; i++) {
	p = worlds.ptrs[i];
        if (flags & LW_SHORT) {
            tfputs(p->name, file);
        } else if (flags & LW_TABLE) {
            tfprintf(file, "%-*.*s %-*.*s %*.*s %-*.*s %s",
                width_name, width_name, p->name,
                width_type, width_type, p->type,
                width_host, width_host, p->host,
                width_port, width_port, p->port,
                p->character);
        } else {
            if (p->myhost) need = 9;
            else if (p->flags & ~WORLD_TEMP) need = 8;
            else if (p->mfile) need = 7;
            else if (p->character || p->pass) need = 6;
            else if (p->host || p->port) need = 4;
            else need = 2;

            Stringcpy(buf, "/test addworld(");
            Sappendf(buf, "\"%q\"", '"', p->name);
            Sappendf(buf, ", \"%q\"", '"', p->type);

            if (need < 3) goto listworld_tail;
            Sappendf(buf, ", \"%q\"", '"', p->host);

            if (need < 4) goto listworld_tail;
            Sappendf(buf, ", \"%q\"", '"', p->port);

            if (need < 5) goto listworld_tail;
            Sappendf(buf, ", \"%q\"", '"', p->character);

            if (need < 6) goto listworld_tail;
            Sappendf(buf, ", \"%q\"", '"', p->pass);

            if (need < 7) goto listworld_tail;
            Sappendf(buf, ", \"%q\"", '"', p->mfile);

            if (need < 8) goto listworld_tail;
            Sappendf(buf, ", \"%s%s%s\"",
                (p->flags & WORLD_NOPROXY) ? "p" : "",
                (p->flags & WORLD_SSL) ? "x" : "",
                (p->flags & WORLD_ECHO) ? "e" : "");

            if (need < 9) goto listworld_tail;
            Sappendf(buf, ", \"%q\"", '"', p->myhost);

listworld_tail:
	    Stringadd(buf, ')');
            tfputs(buf->data, file);
        }
    }
    vector_free(&worlds);
    return worlds.size;
}

struct Value *handle_saveworld_command(String *args, int offset)
{
    TFILE *file;
    char opt;
    const char *mode = "w";
    char *name;
    int result;

    if (restriction >= RESTRICT_FILE) {
        eprintf("restricted");
        return shareval(val_zero);
    }

    startopt(CS(args), "a");
    while ((opt = nextopt(NULL, NULL, NULL, &offset))) {
        if (opt != 'a') return shareval(val_zero);
        mode = "a";
    }
    if ((name = tfname(args->data + offset, "WORLDFILE")) == NULL)
        return shareval(val_zero);
    if ((file = tfopen(name, mode)) == NULL) {
        operror(args->data + offset);
        return shareval(val_zero);
    }
    oprintf("%% %sing world definitions to %s", *mode == 'a' ? "Append" :
        "Writ", file->name);
    result = list_worlds(NULL, NULL, file, 0, NULL);
    tfclose(file);
    return newint(result);
}

World *find_world(const char *name)
{
    World *p;

    if (!name || !*name) return hworld;
    for (p=hworld; p && (!p->name || cstrcmp(name, p->name) != 0); p = p->next);
    return p;
}

/* Perform (*func)(world) on every world */
void mapworld(void (*func)(World *world))
{
    World *w;

    for (w = hworld; w; w = w->next)
        (*func)(w);
}

