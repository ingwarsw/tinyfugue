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
		// add tf_getvar() function
        lua_register(lua_state, "tf_getvar", getvar_for_lua);
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
