#include <string.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "tfconfig.h"
#include "port.h"
#include "tf.h"
#include "util.h"
#include "pattern.h"
#include "search.h"
#include "tfio.h"
#include "world.h"
#include "macro.h"
#include "expand.h"
#include "attr.h"
#include "parse.h"
#include "variable.h"

#include "lua.h"

#define myerror(str) eputs(str)
#define myinfo(str) oputs(str)

static int getvar_for_lua(lua_State *state)
{
	const char *arg;
	const char *value;

	arg = luaL_checkstring(state, 1);
	if (arg == NULL || *arg == '\0')
		return luaL_argerror(state, 1, "name must not be empty");

	value = getvar(arg);
	lua_pushstring(state, value);
	return 1;
}

/*
 * Set global TF variables from lua
 *
 * Requires two arguments - variable and contents.
 *
 * Contents type can be int, float, string, bool or nil.
 * If contents type is nil, then unset the variable.
 */
static int setvar_for_lua(lua_State *state)
{
	const char *arg;
	Var *var;

	/* Get the argument */
	arg = luaL_checkstring(state, 1);
	if (arg == NULL || *arg == '\0')
		return luaL_argerror(state, 1, "name must not be empty");

	/*
	 * Get the variable - will return a lua error, finish/terminate
	 * the luaC call here
	 */
	luaL_checkany(state, 2);

	/*
	 * Find or create the global variable.
	 */
	var = findorcreateglobalvar(arg);

	/* Set the variable contents based on the lua variable type */
	switch (lua_type(state, 2)) {
	case LUA_TNUMBER:
		{
			/* handle integer or number (float) */
			if (lua_isinteger(state, 2)) {
				lua_Integer i;

				i = luaL_checkinteger(state, 2);
				set_int_var_direct(var, TYPE_INT, (int) i);
			} else {
				lua_Number i;

				i = luaL_checknumber(state, 2);
				set_float_var_direct(var, TYPE_FLOAT, (float) i);
			}
			break;
		}
	case LUA_TBOOLEAN:
		{
			int i;

			i = lua_toboolean(state, 2);
			set_int_var_direct(var, TYPE_INT, (int) i);
			break;
		}
	case LUA_TSTRING:
		{
			const char *s;

			s = luaL_checkstring(state, 2);
			set_str_var_direct(var, TYPE_STR,
			    CS(Stringnew(s, -1, 0)));
			break;
		}
	case LUA_TNIL:
		/* Setting it to nil == just straight unset */
		(void) unsetvar(var);
		break;
	default:
		return luaL_argerror(state, 2, "unknown arg type");
	}

	lua_pushboolean(state, 1);
	return 1;
}

/*
 * Unset a global variable.
 *
 * Useful for things that are eg exported as environment variables,
 * on the status line, etc.
 *
 * Takes one lua arg - the variable name.
 *
 * Returns: lua bool = true if success, false if failure
 */
static int unsetvar_for_lua(lua_State *state)
{
	const char *arg;
	int ret;
	Var *val;

	arg = luaL_checkstring(state, 1);
	if (arg == NULL || *arg == '\0')
		return luaL_argerror(state, 1, "name must not be empty");

	val = ffindglobalvar(arg);

	if (val == NULL) {
		lua_pushboolean(state, 0);
		return 0;
	}

	ret = unsetvar(val);
	lua_pushboolean(state, !! ret == 1);
	return ret == 1;
}

// it's the function that Lua scripts will see as "tfexec"
static int tfeval_for_lua(lua_State *state)
{
	String *func;
	const char *arg = luaL_checkstring(state, 1);

        (func = Stringnew(arg, -1, 0))->links++;
	handle_eval_command(func, 0);
    
	Stringfree(func);

	return 0;
}

/*
 * send() for lua code.
 *
 * This takes three parameters:
 *
 * string - required, the string to echo
 * world - optional, may be nil, blank or the world
 * flags - optiona, may be nil, blank or flags
 *
 * Returns 1 if the send was successful, 0 otherwise.
 */
static int tfsendstr_for_lua(lua_State *state)
{
	String *func;
	const char *world = NULL;
	const char *flags = NULL;
	const char *str = luaL_checkstring(state, 1);
	int ret;

	/*
	 * world and flags are optional
	 *
	 * Check that they're a string before using them.
	 * Calling lua_tolstring() on a number field will
	 * convert it to a string first and that may break
	 * things like iterators.
	 */
	if (lua_type(state, 2) == LUA_TSTRING) {
		world = lua_tolstring(state, 2, NULL);
	}
	if (lua_type(state, 3) == LUA_TSTRING) {
		flags = lua_tolstring(state, 3, NULL);
	}

	(func = Stringnew(str, -1, 0))->links++;
	ret = handle_send_function(CS(func),
	    world ? world : "" ,
	    flags ? flags : "" );
	Stringfree(func);

	lua_pushinteger(state, ret);

	return ret;
}

/*
 * send() for lua code, but with a variable, not a string.
 *
 * Useful if send() is to send a string with attributes.
 *
 * This takes three parameters:
 *
 * var - required, the variable to echo
 * world - optional, may be nil, blank or the world
 * flags - optiona, may be nil, blank or flags
 *
 * Returns 1 if the send was successful, 0 otherwise.
 */
static int tfsendvar_for_lua(lua_State *state)
{
	Var *var;
	const char *world = NULL;
	const char *flags = NULL;
	int ret;

	var = ffindglobalvar(luaL_checkstring(state, 1));
	if (var == NULL) {
		return luaL_argerror(state, 1, "non-existent variable");
	}

	/*
	 * world and flags are optional
	 *
	 * Check that they're a string before using them.
	 * Calling lua_tolstring() on a number field will
	 * convert it to a string first and that may break
	 * things like iterators.
	 */
	if (lua_type(state, 2) == LUA_TSTRING) {
		world = lua_tolstring(state, 2, NULL);
	}
	if (lua_type(state, 3) == LUA_TSTRING) {
		flags = lua_tolstring(state, 2, NULL);
	}

	ret = handle_send_function(valstr(&var->val),
	    world ? world : "" ,
	    flags ? flags : "" );

	lua_pushinteger(state, ret);

	return ret;
}



lua_State *lua_state = NULL;

struct Value *handle_calllua_command(String *args, int offset)
{
	// for the love of <enter_your_gods_name_here>,
	// REMEMBER TO FREE cstr_args before returning!
	char *cstr_args = strdup(args->data + offset);
	char *func, *tmp;
	int argpos=-1;
	int num_args;

	if(lua_state == NULL)
	{
		myerror("No script loaded");
		free(cstr_args);
		return newint(1);
	}

	func = cstr_args;
	tmp = strchr(cstr_args, ' ');
	if(tmp != NULL)
	{
		*tmp = '\0';
		argpos = tmp - func + 1;
	}

	lua_getglobal(lua_state, func);
	if(lua_isfunction(lua_state, -1) != 1)
	{
		myerror("No such function");
		lua_pop(lua_state, 1);
		free(cstr_args);
		return newint(1);
	}

	if(argpos > -1)
	{
		int i, table_i=0;

		// first argument: the string data
		lua_pushstring(lua_state, func + argpos);

		// second arg: the attributes of that string as an array of ints
		lua_newtable(lua_state);

		if(args->charattrs)
		{
			for(i=argpos; i<args->len; i++)
			{
				lua_pushinteger(lua_state, args->charattrs[i]);
				lua_rawseti(lua_state, -2, table_i++);
			}
		}

		// third arg: attributes for the whole line
		lua_pushinteger(lua_state, args->attrs);

		num_args = 3;
	}
	else
		num_args = 0;
	
	if(lua_pcall(lua_state, num_args, 0, 0) != 0)
	{
		myerror(lua_tostring(lua_state, -1));
		// pop the error message
		lua_pop(lua_state, 1);
		free(cstr_args);
		return newint(1);
	}

	free(cstr_args);
	return newint(0);
}

struct Value *handle_loadlua_command(String *args, int offset)
{
	// check if lua has been initialised
	if(lua_state == NULL)
	{ // it hasn't: initialise now
		lua_state = luaL_newstate();
		luaL_openlibs(lua_state);

		// add our tf_eval() function for Lua scripts to call
		lua_register(lua_state, "tf_eval", tfeval_for_lua);
		lua_register(lua_state, "tf_getvar", getvar_for_lua);
		lua_register(lua_state, "tf_setvar", setvar_for_lua);
		lua_register(lua_state, "tf_unsetvar", unsetvar_for_lua);
		lua_register(lua_state, "tf_sendstr", tfsendstr_for_lua);
		lua_register(lua_state, "tf_sendvar", tfsendvar_for_lua);
	}

	// now read and execute the scripts that user asked for
	if(luaL_dofile(lua_state, args->data + offset) != 0)
	{
		myerror("Cannot load script: ");
		myerror(lua_tostring(lua_state, -1));
		lua_pop(lua_state, 1);
		return newint(1);
	}

	return newint(0);
}

struct Value *handle_purgelua_command(String *args, int offset)
{
	if(lua_state != NULL)
    {
    	lua_close(lua_state);
	    lua_state = NULL;
	    myinfo("All LUA scripts forgotten");
		return newint(1);
    }
	myinfo("No LUA scripts loaded");
	return newint(0);
}

struct Value *
handle_calllua_function(int n)
{
	char *func;
	int num_args;
	struct conString *s;
	Value *v = NULL;

	if (n == 0) {
		myerror("No function name found");
		return newint(-1);
	}

	s = opdstr(n); n--;
	func = strndup(s->data, s->len);

	if(lua_state == NULL)
	{
		myerror("No script loaded");
		free(func);
		return newint(-1);
	}

	lua_getglobal(lua_state, func);

	if(lua_isfunction(lua_state, -1) != 1)
	{
		myerror("No such function");
		lua_pop(lua_state, 1);
		free(func);
		return newint(1);
	}

	num_args = 0;
	for (; n; n--) {
		s = opdstr(n);
		lua_pushlstring(lua_state, s->data, s->len);
		num_args++;
	}

	if(lua_pcall(lua_state, num_args, 1, 0) != 0)
	{
		myerror(lua_tostring(lua_state, -1));
		v = newint(-1);
		goto done;
	}

	/* Returned a boolean */
	if (lua_isboolean(lua_state, -1)) {
		v = newint((int) lua_toboolean(lua_state, -1));
		goto done;
	}


	/* Returned an integer */
	if (lua_isinteger(lua_state, -1)) {
		v = newint(lua_tointeger(lua_state, -1));
		goto done;
	}


	/* Returned a number */
	if (lua_isnumber(lua_state, -1)) {
		v = newfloat(lua_tonumber(lua_state, -1));
		goto done;
	}

	/* Returned a string */
	if (lua_isstring(lua_state, -1)) {
		v = newstr(lua_tostring(lua_state, -1),
		     lua_rawlen(lua_state, -1));
		goto done;
	}

done:
	/* Default */
	free(func);
	lua_pop(lua_state, 1);
	return v;
}
