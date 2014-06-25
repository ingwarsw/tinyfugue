/*************************************************************************
 *  TinyFugue - programmable mud client
 *  Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2002, 2003, 2004, 2005, 2006-2007 Ken Keys
 *
 *  TinyFugue (aka "tf") is protected under the terms of the GNU
 *  General Public License.  See the file "COPYING" for details.
 ************************************************************************/
static const char RCSid[] = "$Id: main.c,v 35004.115 2007/01/13 23:12:39 kkeys Exp $";


/***********************************************
 * Fugue main routine                          *
 *                                             *
 * Initializes many internal global variables, *
 * determines initial world (if any), reads    *
 * configuration file, and calls main loop in  *
 * socket.c                                    *
 ***********************************************/

#include "tfconfig.h"
#include "port.h"
#include "tf.h"
#include "util.h"
#include "pattern.h"	/* for tfio.h */
#include "search.h"
#include "tfio.h"
#include "history.h"
#include "world.h"
#include "socket.h"
#include "macro.h"
#include "output.h"
#include "attr.h"
#include "signals.h"
#include "command.h"
#include "keyboard.h"
#include "variable.h"
#include "tty.h"	/* no_tty */
#include "expand.h"
#include "expr.h"
#include "process.h"

const char sysname[] = UNAME;

/* For customized versions, please add a unique identifer (e.g., your initials)
 * to the version number, and put a brief description of the modifications
 * in the mods[] string.
 */
const char version[] =
#if DEVELOPMENT
    "DEVELOPMENT VERSION: "
#endif
    "TinyFugue version 5.0 beta 8";

const char mods[] = "";

const char copyright[] =
    "Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2002, 2003, 2004, 2005, 2006-2007 Ken Keys (kenkeys@users.sourceforge.net)";

const char contrib[] =
#ifdef PLATFORM_OS2
    "OS/2 support written by Andreas Sahlbach (asa@stardiv.de)";
#else
    "";
#endif

int restriction = 0;
int debug = 0;

static void read_configuration(const char *fname);
int main(int argc, char **argv);

int main(int argc, char *argv[])
{
    char *opt, *argv0 = argv[0];
    char *configfile = NULL, *command = NULL, *libdir = NULL;
    int worldflag = TRUE;
    int autologin = -1, quietlogin = -1, autovisual = TRUE;
    String *scratch;

    puts("");
    puts(version);
    puts(copyright);

    while (--argc > 0 && (*++argv)[0] == '-') {
        if (!(*argv)[1]) { argc--; argv++; break; }
        for (opt = *argv + 1; *opt; )
            switch (*opt++) {
            case 'd':
                debug = TRUE;
                break;
            case 'l':
                autologin = FALSE;
                break;
            case 'q':
                quietlogin = TRUE;
                break;
            case 'n':
                worldflag = FALSE;
                break;
            case 'v':
                autovisual = FALSE;
                break;
            case 'f':
                configfile = opt;
                goto nextarg;
            case 'c':
                command = opt;
                goto nextarg;
            case 'L':
                libdir = opt;
                goto nextarg;
            default:
                fprintf(stderr, "\n%s: illegal option -- %c\n", argv0, *--opt);
                goto error;
            }
        nextarg: /* empty statement */;
    }
    if (argc > 2) {
    error:
        fputs("\n", stderr);
        fprintf(stderr, "Usage: %s [-L<dir>] [-f[<file>]] [-c<cmd>] [-vnlq] [<world>]\n", argv0);
        fprintf(stderr, "       %s [-L<dir>] [-f[<file>]] [-c<cmd>] [-vlq] <host> <port>\n", argv0);
        fputs("Options:\n", stderr);
        fputs("  -L<dir>   use <dir> as library directory (%TFLIBDIR)\n", stderr);
        fputs("  -f        don't load personal config file (.tfrc)\n", stderr);
        fputs("  -f<file>  load <file> instead of config file\n", stderr);
        fputs("  -c<cmd>   execute <cmd> after loading config file\n", stderr);
        fputs("  -n        no automatic first connection\n", stderr);
        fputs("  -l        no automatic login/password\n", stderr);
        fputs("  -q        quiet login\n", stderr);
        fputs("  -v        no automatic visual mode\n", stderr);
        fputs("Arguments:\n", stderr);
        fputs("  <host>    hostname or IP address\n", stderr);
        fputs("  <port>    port number or name\n", stderr);
        fputs("  <world>   connect to <world> defined by addworld()\n", stderr);
        fputs("\n", stderr);
        exit(1);
    }

#if SOCKS
    SOCKSinit(argv0);  /* writes message to stdout */
#endif
    init_exename(argv0);

    SRAND(getpid() + time(NULL));	/* seed random generator */
    init_malloc();			/* malloc.c   */
    init_tfio();			/* tfio.c     */
    init_util1();			/* util.c     */
    init_expand();			/* expand.c   */
    init_variables();			/* variable.c */
    init_signals();			/* signals.c  */
    init_sock();			/* socket.c   */
    init_macros();			/* macro.c    */
    init_histories();			/* history.c  */
    init_output();			/* output.c   */
    init_attrs();			/* attr.c     */
    init_keyboard();			/* keyboard.c */

    oputs(version);
    oputs(copyright);
    oputs("Type `/help copyright' for more information.");
    if (*contrib) oputs(contrib);
    if (*mods) oputs(mods);
    oprintf("Using PCRE version %s", pcre_version());
    oputs("Type `/help', `/help topics', or `/help intro' for help.");
    oputs("Type `/quit' to quit tf.");
    oputs("");

    init_util2();			/* util.c     */

    if (libdir) {
        set_str_var_by_name("TFLIBDIR", Stringnew(libdir, -1, 0));
    }
    if (!ffindglobalvar("TFLIBRARY")) {
        scratch = Stringnew(NULL, 0, 0);
        Sprintf(scratch, "%s/stdlib.tf", TFLIBDIR);
        set_str_var_by_name("TFLIBRARY", scratch);
    }
    if (!ffindglobalvar("TFHELP")) {
        scratch = Stringnew(NULL, 0, 0);
        Sprintf(scratch, "%s/tf-help", TFLIBDIR);
        set_str_var_by_name("TFHELP", scratch);
    }

    read_configuration(configfile);

    if (command) {
	String *scmd;
	(scmd = Stringnew(command, -1, 0))->links++;
        macro_run(CS(scmd), 0, NULL, 0, sub, "\bSTART");
	Stringfree(scmd);
    }

    /* If %visual was not explicitly set, set it now. */
    if (getintvar(VAR_visual) < 0 && !no_tty)
        set_var_by_id(VAR_visual, autovisual);

    /* If %interactive was not explicitly set, set it now. */
    if (getintvar(VAR_interactive) < 0)
        set_var_by_id(VAR_interactive, !no_tty);

    if (argc > 0 || worldflag) {
	int flags = 0;
        if (autologin < 0) autologin = login;
        if (quietlogin < 0) quietlogin = quietflag;
	if (autologin) flags |= CONN_AUTOLOGIN;
	if (quietlogin) flags |= CONN_QUIETLOGIN;
        if (argc == 0)
            openworld(NULL, NULL, flags);
        else if (argc == 1)
            openworld(argv[0], NULL, flags);
        else /* if (argc == 2) */
            openworld(argv[0], argv[1], flags);
    } else {
        world_hook("---- No world ----", NULL);
    }

    main_loop();

    kill_procs();
#if USE_DMALLOC
    free_screen_lines(default_screen);
#endif
    minimal_fix_screen();
    reset_tty();

#if USE_DMALLOC
    free_macros();
    free_worlds();
    free_histories();
    free_output();
    free_vars();
    free_keyboard();
    free_search();
    free_expand();
    free_expr();
    free_help();
    free_util();
    free_patterns();
    free_dstring();
    free_reserve();
    fflush(stdout);
    fflush(stderr);
    debug_mstats("tf");
#endif

    return 0;
}

static void read_configuration(const char *fname)
{
#if 1 /* XXX */
    if (do_file_load(getvar("TFLIBRARY"), FALSE) < 0)
        die("Can't read required library.", 0);
#endif

    if (fname) {
        if (*fname) do_file_load(fname, FALSE);
        return;
    }

    (void)(   /* ignore value of expression */
	/* Try the next file if a file can't be read, but not if there's
	 * an error _within_ a file. */
        do_file_load("~/.tfrc", TRUE) >= 0 ||
        do_file_load("~/tfrc",  TRUE) >= 0 ||
        do_file_load("./.tfrc", TRUE) >= 0 ||
        do_file_load("./tfrc",  TRUE)
    );

    /* support for old fashioned .tinytalk files */
    do_file_load((fname = getvar("TINYTALK")) ? fname : "~/.tinytalk", TRUE);
}

