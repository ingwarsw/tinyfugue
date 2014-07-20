// -------------------------------------------------------------------------------------
// Python support for TinyFugue 5 beta
//
// Copyright 2008 Ron Dippold - Modify at will, just add your changes in README.python
// -------------------------------------------------------------------------------------

// If this isn't defined, the whole file is null
#ifdef TFPYTHON

#include "tfpython.h"

// Change this 0 to 1 to add debug printfs
#if 0
#define DPRINTF oprintf
#else
#define DPRINTF(...)
#endif


static PyObject* tfvar_to_pyvar( const struct Value *rc );
static struct Value* pyvar_to_tfvar( PyObject *pRc );

// -------------------------------------------------------------------------------------
// The tf 'module' as seen by python scripts
// -------------------------------------------------------------------------------------

// def tf.eval( argstring ):
static PyObject *tf_eval( PyObject *pSelf, PyObject *pArgs )
{
	struct String *cmd;
	const char *cargs;
	struct Value *rc;

	if( !PyArg_ParseTuple( pArgs, "s", &cargs ) ) {
		eputs( "tf.eval called with non-string argument" );
		Py_RETURN_NONE;

	}

	// ask tinyfugue to eval the string
	( cmd = Stringnew( cargs, -1, 0 ) )->links++;
	rc = handle_eval_command( cmd, 0 );
	Stringfree( cmd );

	return tfvar_to_pyvar( rc );
}

//def tf.out( string ):
static PyObject *tf_out( PyObject *pSelf, PyObject *pArgs )
{
	const char *cargs;
	if( !PyArg_ParseTuple( pArgs, "s", &cargs ) ) {
		eputs( "tf.out called with non-string argument" );
	} else {
		oputs( cargs );
	}
	Py_RETURN_NONE;
}

//def tf.err( string ):
static PyObject *tf_err( PyObject *pSelf, PyObject *pArgs )
{
	const char *cargs;
	if( !PyArg_ParseTuple( pArgs, "s", &cargs ) ) {
		eputs( "tf.err called with non-string argument" );
	} else {
		eputs( cargs );
	}
	Py_RETURN_NONE;
}

// tf.send( <string>, <world>=None )
static PyObject *tf_send( PyObject *pSelf, PyObject *pArgs )
{
	const char *string, *worldname=NULL;

	if( !PyArg_ParseTuple( pArgs, "s|s", &string, &worldname ) ) {
		eputs( "tf.send called with bad arguments" );
	} else {
		String *buf = Stringnew( string, -1, 0 );
		handle_send_function( CS(buf), worldname ? worldname : "" , "" );
	}
	Py_RETURN_NONE;
}

//def tf.tfrc()
static PyObject *tf_tfrc( PyObject *pSelf, PyObject *pArgs )
{
	extern char *main_configfile;
	return Py_BuildValue( "s", main_configfile ? main_configfile : "" );
}

//def tf.world():
static PyObject *tf_world( PyObject *pSelf, PyObject *pArgs )
{
	World *world = named_or_current_world( "" );
	return Py_BuildValue( "s", world ? world->name : "");
}

// tf.getvar( varname, default )
static PyObject *tf_getvar( PyObject *pSelf, PyObject *pArgs )
{
	const char *cargs;
	PyObject *def = NULL;
	Var *var = NULL;

	if( !PyArg_ParseTuple( pArgs, "s|O", &cargs, &def ) ) {
		eputs( "tf.getvar called with bad arguments" );
	} else {
		var = ffindglobalvar( cargs );
	}

	if( var ) {
		return tfvar_to_pyvar( &var->val );
	} else if ( def ) {
		Py_INCREF( def );
		return def;
	} else {
		Py_RETURN_NONE;
	}
}

// Our tf 'module'
static PyMethodDef tfMethods[] = {
	{
		"err", tf_err, METH_VARARGS,
		"tf.err( <string> )\n"
		"Shows error <string> locally (to the tferr stream).\n"
	},
	{
		"eval", tf_eval, METH_VARARGS,
		"tf.eval( <string> )\n"
		"Calls TinyFugue's /eval with <string>.\n"
		"Returns the return code of the evaluation if any."
	},
	{
		"getvar", tf_getvar, METH_VARARGS,
		"tf.getvar( <varname>, (<default>) )\n"
		"use tf.eval() for setvar functionality\n"
		"Returns the value of <varname> if it exists, or default."
	},
	{
		"out", tf_out, METH_VARARGS,
		"tf.out( <string> )\n"
		"Shows <string> locally (to the tfout stream).\n"
	},
	{
		"send", tf_send, METH_VARARGS,
		"tf.send( <string>, (<world>) )\n"
		"Send <string> to <world> (default: current world)"
	},
	{	"tfrc", tf_tfrc, METH_VARARGS,
		"tf.tfrc()\n"
		"returns the file name of the .tfrc file (or empty string)"
	},
	{
		"world", tf_world, METH_VARARGS,
		"tf.world()\n"
		"Returns the name of the current world, or a blank string."
	},
	{ NULL, NULL, 0, NULL }
};


// -------------------------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------------------------

// run the command in the context of the __main__ dictionary
static PyObject *main_module, *main_dict=NULL;
static PyObject *common_run( const char *cmd, int start )
{
	return PyRun_String( cmd, start, main_dict, main_dict );
}


// Convert int, float, and string to python objects for return
// Otherwise, just return None
static PyObject* tfvar_to_pyvar( const struct Value *rc )
{
	switch( rc->type ) {
	case TYPE_INT:
	case TYPE_POS:
		DPRINTF( "TYPE_INT: %d", rc->u.ival );
		return Py_BuildValue( "i", rc->u.ival );
	case TYPE_FLOAT:
		DPRINTF( "TYPE_FLOAT: %f", rc->u.fval );
		return Py_BuildValue( "f", rc->u.fval );
	case TYPE_STR:
	case TYPE_ENUM:
		DPRINTF( "TYPE_STR: %s", rc->sval->data );
		return Py_BuildValue( "s", rc->sval->data );
	default:
		DPRINTF( "TYPE ??? %d", rc->type );
		Py_RETURN_NONE;
	}
}

// Helper - take a python return object and covert it to a tf return object.
// We supprt strings, integers, booleans (False->0, True->1 ), and floats
static struct Value* pyvar_to_tfvar( PyObject *pRc )
{
	struct Value *rc;
	char *cstr;
	int len; // Py_ssize_t len;

	// can be null if exception was thrown
	if( !pRc ) {
		PyErr_Print();
		return newstr( "", 0 );  
	}

	// Convert string back into tf string
	if( PyString_Check( pRc ) && ( PyString_AsStringAndSize( pRc, &cstr, &len ) != -1 ) ) {
		DPRINTF( "  rc string: %s", cstr );
		rc = newstr( cstr, len );
	} else if( PyInt_Check( pRc ) ) {
		DPRINTF( "  rc int: %ld", PyInt_AsLong( pRc ) );
		rc = newint( PyInt_AsLong( pRc ) );
	} else if( PyLong_Check( pRc ) ) {
		DPRINTF( "  rc long: %ld", PyLong_AsLong( pRc ) );
		rc = newint( PyLong_AsLong( pRc ) );
	} else if( PyFloat_Check( pRc ) ) {
		DPRINTF( "  rc float: %lf", PyFloat_AsDouble( pRc ) );
		rc = newfloat( PyFloat_AsDouble( pRc ) );
	} else if( PyBool_Check( pRc ) ) {
		DPRINTF( "  rc bool: %lf", pRc == Py_True ? 1 : 0 );
		rc = newint( pRc == Py_True ? 1 : 0 );
	} else {
		DPRINTF( "  rc None" );
		rc = newstr( "", 0 );
	}
	Py_DECREF( pRc );

	// And return
	return rc;
}

// -------------------------------------------------------------------------------------
// Structural work - initialize the interpreter if necessary
// -------------------------------------------------------------------------------------
static int py_inited=0;

// All this is executed on first load. Note - you can change the behavior of this
// on the fly, for instance to turn on stdout, by doing
//    /python sys.stdout.output=tf.out
// then returning it with
//    /python sys.stdout.output=None
//
static const char *init_src =
	"class __dummyout:\n"
	"	buf=''\n"
	"\n"
	"	def __init__( self, output ):\n"
	"		'pass in None for no output'\n"
	"		self.output = output\n"
	"	def write( self, arg ):\n"
	"		if not self.output:\n"
	"			return\n"
	"		self.buf+=arg\n"
	"		while '\\n' in self.buf:\n"
	"			a, self.buf = self.buf.split('\\n',1)\n"
	"			self.output( a )\n"
	"\n"
	"sys.stdout = __dummyout( None )\n"  	// this could be to tf.out
	"sys.stderr = __dummyout( tf.err )\n"
	"\n"
	"sys.argv=[ 'tf' ]\n"
;

static void python_init()
{
	if( py_inited )
		return;

	// Initialize python
	Py_Initialize();
	
	// Tell it about our tf_eval
	Py_InitModule( "tf", tfMethods );

	// get the basic modules
	PyRun_SimpleString( "import os, sys, tf" );
	PyRun_SimpleString( "sys.path.append( '.' )" );

	// modify python path
	if( TFPATH && *TFPATH ) {
		String *buf = Stringnew( NULL, 0, 0 );
		Sprintf( buf, "sys.path.extend( \"%s\".split() )", TFPATH );
		PyRun_SimpleString( buf->data );
	}
	if( TFLIBDIR && *TFLIBDIR ) {
		String *buf = Stringnew( NULL, 0, 0 );
		Sprintf( buf, "sys.path.append( \"%s\" )", TFLIBDIR );
		PyRun_SimpleString( buf->data );
	}

	PyRun_SimpleString( init_src );

	// These are both borrowed refs, we don't have to DECREF
	main_module = PyImport_AddModule( "__main__");
	main_dict = PyModule_GetDict( main_module );


	py_inited = 1;
}

static void python_kill()
{
	if( py_inited ) {
		Py_Finalize();
		py_inited = 0;
	}
}


// -------------------------------------------------------------------------------------
// c -> python calls
// -------------------------------------------------------------------------------------


// Handle a python call with arguments
struct Value *handle_python_call_command( String *args, int offset )
{
	char *funcstr, *argstr;
	PyObject *pRc=NULL, *function=NULL, *arglist=NULL;
	//String *buf;

	if( !py_inited )
		python_init();

	DPRINTF( "handle_python_call_command: %s", args->data + offset );

	// Look for string splitting function name and arguments
	funcstr = strdup( args->data + offset );
	if( ( argstr = strchr( funcstr, ' ' ) ) ) {
		*argstr++ = '\0';
	} else {
		argstr = "";
	}

	// Look up the function in the namespace, make sure it's callable
	function = common_run( funcstr, Py_eval_input );
	if( !function ) {
		goto bail;
	}
	if( !PyCallable_Check( function )) {
		PyErr_SetString(PyExc_TypeError, "parameter must be callable" );
		goto bail;
	}

	// Okay, so now it's callable, give it the string.
	// We go through all this because otherwise the quoting problem is insane.
	arglist = Py_BuildValue( "(s)", argstr );
	pRc = PyEval_CallObject( function, arglist );

bail:
	Py_XDECREF( function );
	Py_XDECREF( arglist );
	free( funcstr );
	return pyvar_to_tfvar( pRc );
}

// Just kill the interpreter
struct Value *handle_python_kill_command( String *args, int offset )
{
	python_kill();
	return newint( 0 );
}

// Import/reload a python module	  
struct Value *handle_python_load_command( String *args, int offset )
{
	PyObject *pRc;
	struct Value *rv;
	String *buf;
	const char *name = args->data + offset;

	if( !py_inited )
		python_init();
	
	DPRINTF( "handle_python_load_command: %s", name );

	// module could invoke tf.eval, so mark it as used
	( buf = Stringnew( NULL, 0, 0 ) )->links++;
	Sprintf( buf,
		// if it exists, reload it, otherwise import it
		"try:\n"
		"  reload( %s )\n"
		"except ( NameError, TypeError ):\n"
		"  import %s\n",
		name, name );

	pRc = common_run( buf->data, Py_file_input );
	if( pRc ) {
		Py_DECREF( pRc );
		rv = newint( 0 );
	} else {
		PyErr_Print();
		rv = newint( 1 );
	}
	Stringfree( buf );
	return rv;
}

// Run arbitrary code
struct Value *handle_python_command( String *args, int offset )
{
	PyObject *pRc;

	if( !py_inited )
		python_init();

	DPRINTF( "handle_python_command: %s", args->data + offset );
	pRc = common_run( args->data + offset, Py_single_input );
	return pyvar_to_tfvar( pRc );
}

struct Value *handle_python_function( conString *args )
{
	PyObject *pRc;

	if( !py_inited )
		python_init();

	DPRINTF( "handle_python_expression: %s", args->data );
	pRc = common_run( args->data, Py_eval_input );
	return pyvar_to_tfvar( pRc );
}

#endif
