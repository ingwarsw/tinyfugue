#ifndef LUA_H
#define LUA_H

#include "tfconfig.h"
#include "port.h"
#include "tf.h"
#include "util.h"
#include "pattern.h"
#include "search.h"
#include "tfio.h"
#include "cmdlist.h"
#include "command.h"
#include "world.h"	/* World, find_world() */
#include "socket.h"	/* openworld() */
#include "output.h"	/* oflush(), dobell() */
#include "attr.h"
#include "macro.h"
#include "keyboard.h"	/* find_key(), find_efunc() */
#include "expand.h"     /* macro_run() */
#include "signals.h"    /* suspend(), shell() */
#include "variable.h"

/* that's definitely too many headers */

struct Value *handle_calllua_command(String *args, int offset);
struct Value *handle_loadlua_command(String *args, int offset);
struct Value *handle_purgelua_command(String *args, int offset);
struct Value *handle_calllua_function(int n);

#endif 
