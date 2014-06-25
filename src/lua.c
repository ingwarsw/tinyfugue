#include <string.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "lua.h"

#define myerror(str) eputs(str)
#define myinfo(str) oputs(str)

// it's the function that Lua scripts will see as "tfexec"
static int tfeval_for_lua(lua_State *state)
{
	String *func;
	const char *arg = luaL_checkstring(state, 1);

	(func = Stringnew(NULL, 0, 0))->links++;
    Sprintf(func, arg);
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
		myerror("no script loaded");
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

	lua_getfield(lua_state, LUA_GLOBALSINDEX, func);
	if(lua_isfunction(lua_state, -1) != 1)
	{
		myerror("no such function");
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
		lua_state = lua_open();
		luaL_openlibs(lua_state);

		// add our tf_eval() function for Lua scripts to call
		lua_pushcfunction(lua_state, tfeval_for_lua);
		lua_setglobal(lua_state, "tf_eval");
	}

	// now read and execute the scripts that user asked for
	if(luaL_dofile(lua_state, args->data + offset) != 0)
	{
		myerror("cannot load script:");
		myerror(lua_tostring(lua_state, -1));
		lua_pop(lua_state, 1);
	}
	else
		myinfo("loaded");

	return newint(0);
}

struct Value *handle_purgelua_command(String *args, int offset)
{
	lua_close(lua_state);
	lua_state = NULL;

	myinfo("all scripts forgotten");

	return newint(0);
}
