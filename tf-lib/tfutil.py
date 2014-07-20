#
# Various helper funcs/classes for TF Python that are more suited to writing
# in python than in C (so aren't in the base tf module)
#
# Copyright 2008 Ron Dippold

import tf

# ---------------------------------------------------------------------------
# Misc helper functions
# ---------------------------------------------------------------------------

def screensize():
	"""
	Returns ( rows, columns )
	"""
	return ( tf.eval( "/test columns()" ), tf.eval( "/test lines()" ) )

def visual():
	"""
	if visual is off, returns 0, otherwise returns number of input lines
	"""
	
	if tf.getvar( "visual", "off" ) == "on":
		return tf.getvar( "isize", 3 )
	else:
		return 0

def eval_escape( text ):
	"""
	returns text with %, \, and $ doubled
	"""
	return text.replace('%','%%').replace( '\\','\\\\').replace("$","$$")

def cmd_parse( argstr, opts ):
	"""
	Parse tf-style command lines into args and opts- like getopt, but for
	TinyFugue style where -w<x> may have <x> or not, and <x> must be next
	to -w, not separated by a space.

	argstr is the command line, opts is a list of 'w:ligvt:a:m:A:B:C:#' where
	: means an argument - we don't care if it's optional or not. # is a
	number option like '-42' such as in the /quote command.

	The arguments are considered over when we hit any non-flag or '--' or '-'

	Returns ( cmd, optdict, rest ) where cmd is '/recall' (or blank if there
	wasn't a command present), optdict is { 'w':'foo', 'l':'', ... }
	and rest is a string with the rest of the line (unlike getopt, we need to
	preserve the spacing in the rest of the line).

	See also cmd_unparse - you can put the command back together after changing
	the args.
	"""

	# parts opts into an flagsdict - guess we could cache these
	flagsdict = {}
	i, olen = 0, len(opts)
	while i<olen:
		if i+1<olen and opts[i+1] == ":":
			flagsdict[opts[i]] = opts[i+1]
			i += 2
		else:
			flagsdict[opts[i]] = ''
			i += 1

	# grab the command off the front (if any)
	if argstr.startswith( "/" ):
		cmd, argstr = argstr.split(None,1)
	else:
		cmd = ''

	# run through opts on command line
	outdict = {}
	while True:
		argstr = argstr.lstrip()
		if not argstr.startswith( '-' ):
			break

		if ' ' in argstr:
			arg, argstr = argstr[1:].split( None, 1 )
		else:
			arg, argstr = argstr, ''
		if arg == '' or arg == '-': # -- ends arguments
			break

		# check each flag in this word
		i, alen = 0, len( arg )
		while i<alen:
			flag = arg[i]
			if flag.isdigit() and "#" in flagsdict:
				outdict['#'] = arg[i:]
				break
			form = flagsdict.get( flag, '' )
			if form == '':
				outdict[flag] = form
				i += 1
			elif form == ':':
				outdict[flag] = arg[i+1:]
				break

	# all done!
	return ( cmd, outdict, argstr )

def cmd_unparse( cmd, optsdict, argstr ):
	"""
	Takes optional cmd ('/recall'), opts dict as created by cmd_parse,
	and the rest of the command and recreates a tf.eval() line:

	tfutil.cmd_unparse( '/recall', { 'w':'foo', '#':'12' }, '300' )
	/recall -wfoo -12 300
	
	tfutil.cmd_unparse( '', { 'i':'', 'q':'' ], '300' )
	-q -i 300
	"""

	fullcmd = []
	if cmd:
		fullcmd.append( cmd )

	for flag, arg in optsdict.items():
		fullcmd.append( '-%s%s' % ( flag, arg ) )

	# make sure we terminate args before adding command
	if argstr.startswith('-'):
		fullcmd.append( '-' )
	fullcmd.append( argstr )
	
	return ' '.join( fullcmd )

# ---------------------------------------------------------------------------
# /output helpers - there doesn't seem to be a way I can find to reliably
# get the %| pipes to work, even in the tfio examples (I may be missing
# something) so these do the job of screen scraping arbitrary commands.
# Luckily this is pretty fast.
# ---------------------------------------------------------------------------

_caught = []
def _scrape( line ):
	_caught.append( line )

def screenscrape( command ):
	"""
	Execute an arbitrary command and returns the output as a list.
	"""
	_caught[:] = []
	x = tf.eval( "/def -iq _tfscrape = /python_call tfutil._scrape \%*" )
	tf.eval( "/quote -S /_tfscrape `%s" % command )
	tf.eval( "/undefn %s" % x )
	return _caught[:]

def listsockets():
	"""
	Returns a list of socket names (as strings).
	We could get fancier later and create Socket classes with more
	    info, but don't need it now.
	"""

	return screenscrape( "/listsockets -s " )

def listworlds( asdict = False ):
	"""
	Returns a list of tfutil.World classes.
	If asdict, then as a dictionary	with the World.name as key.
	"""
	l = [ World( line ) for line in screenscrape( "/listworlds -c" ) ]
	if asdict:
		return dict( [ ( w.name, w ) for w in l ] )
	else:
		return l

# ---------------------------------------------------------
# World class - info about each /addworld
# ---------------------------------------------------------

class World( object ):
	"""
	A World() class contains info about a known worlds (/listworlds).
	This is not quite the same as a socket, because it only includes
	non-virtual worlds, and unconnected worlds as well.
	"""

	# These are the attributes on the World you can get/set - in same order
	# as listworlds command format
	KEYS=( 'name', 'type', 'host', 'port', 'character', 'password', 'file', 'flags', 'srchost' )

	# See category() below
	INVALID, NORMAL, DEFAULT, VIRTUAL = ( 0, 1, 2, 3 )

	# If we don't have a port, use this one
	DEFPORT = 23

	def __init__( self, arg ):
		"""
		You can pass one of three arguments to init the class:
		   empty string:        new empty (INVALID) world
		   string:              new world from /listworlds -c format
		   World:               copy of another world
		"""

		# copy of another World
		if isinstance( arg, World ):
			for key in World.KEYS:
				setattr( self, key, getattr( arg, key, '' ) )
			return
			
		# empty string
		if not arg:
			for key in World.KEYS:
				setattr( self, key, '' )
			self.port = World.DEFPORT
			return

		# /listworlds -c format
		if arg.startswith( "/test addworld" ):
			argstr = arg[ arg.find("(")+1 : -1 ]
			split = argstr.split(", ") + [''] * 10
			for i, key in enumerate( World.KEYS ):
				setattr( self, key, split[i].strip('"') )
			if not self.port:
				self.port = World.DEFPORT
			return

		raise Exception( "World() - invalid init string" )

	# some special funcs - comparison is done by name, as is nonzero check
	def __cmp__( self, other ):
		if isinstance( other, World ):
			return cmp( self.name.lower(), other.name.lower() )
		else:
			return cmp( self.name.lower(), other.lower() )
	def __nonzero__( self ):
		return self.name!=''
	def __str__( self ):
		return self.addworld_command()
	def __repr__( self ):
		return "tfutil.World( '%s' )" % self.addworld_command( True )

	# okay, really different
	def changed_from( self, other ):
		for key in World.KEYS:
			if getattr( self, key ) != getattr( other, key ):
				return True
		return False

	def category( self ):
		"""
		Worlds can be one of the following:
		   World.INVALID      - name is not set
		   World.NORMAL       - normal World with host name
		   World.DEFAULT      - World name is 'default', see /help addworld
		   WOrld.VIRTUAL      - World without a host
		"""
		if not self.name:
			return World.INVALID
		if not self.host:
			return World.VIRTUAL
		if self.name.lower() == 'default':
			return World.DEFAULT
		return World.NORMAL

	def send( self, text ):
		"""
		Send text to this world, even if it's not foreground. Note that a world
		existing does not guarantee that it current is connected.
		"""
		if self.name:
			tf.send( text, self.name )

	def addworld_command( self, func=False, full=True ):
		"""
		Return an addworld command for this world.
		If World.category() is World.INVALID, this returns an empty string!

		If Func is True it will return a
			/test addworld( ... )
		line, otherwise it will return an
			/addworld ...
	    
		If full is False then it will just return the addworld( ... ) part
		without the /test or the ... without the /addworld.
		
		"""

		which = self.category()
		if which == World.INVALID:
			return ''

		# function format
		if func:
			cmd = [ '"%s"' % getattr(self,key,'') for key in World.KEYS ]
			while cmd and cmd[-1]=='""':
				del cmd[-1]
			if full:
				return "/test addworld( %s )" % ", ".join( cmd )
			else:
				return "addworld( %s )" % ", ".join( cmd )

		# addworld format
		if full:
			cmd = [ '/addworld' ]
		else:
			cmd = []
		
		if which==World.NORMAL and self.flags:
			cmd.append( '-' + self.flags )
		if self.type:
			cmd.append( '-T' + self.type )
		if which!=World.DEFAULT and self.srchost:
			cmd.append( '-s' + self.srchost )
		cmd.append( self.name )
		if which!=World.VIRTUAL and self.character and self.password:
			cmd.append( self.character )
			cmd.append( self.password )
		if which==World.NORMAL:
			cmd.append( self.host )
			cmd.append( self.port )
		if which!=World.VIRTUAL and self.file:
			cmd.append( self.file )

		return ' '.join( cmd )
		

# ---------------------------------------------------------
# State class
# ---------------------------------------------------------

class State( object ):
	"""
	This class lets you do various binds and sets and then (the key) revert
	TF's configuration to undo the changes you made.
	"""

	def __init__( self ):
		self.lose_changes()

	def setvar( self, var, value ):
		"""
		old = State.setvar( <var>, <value> )
		
		set TF variable <var> to <value>, returns the old value.
		Saves the old value for when reverting State.
		"""
		old = tf.getvar( var )
		tf.eval( "/set %s=%s" % ( var, value ) )
		if not var in self.oldvar:
			self.oldvar[ var ] = old
		return old

	def define( self, name="", body="", flags="" ):
		"""
		n = State.define( name='<name>', body='<body>', flags='<flags>' )

		Adds a new /def, returns the number of the new /def. All args optional.
		Saves the old value of named /def if it exists, for reverting State.

		Not named def() because that's a Python keyword.
		"""

 		if name and not name in self.olddef:
			x = screenscrape( "/list -i -msimple %s" % name )
			if x:
				self.olddef[name] = x[0]

		if flags and not flags.startswith("-"):
			flags = "-"+flags

		n = tf.eval( "/def %s %s = %s" % ( flags, name, eval_escape(body) ) )
		self.newdef.append( n )
		
		return n

	def bind( self, key, body="", flags="" ):
		"""
		n = State.bind( key='<key>', body='<body>', flags='<flags>' )

		Create a new key binding for <key>. Returns the number of the new /def.
		Body and flags are optional. Saves old binding for reverting State.
		"""

		if not key:
			raise Exception( "tfutil.bind: empty key name" )
		
		if not key in self.oldkey:
			x = screenscrape( "/list -i -msimple -b'%s'" % key )
			if x:
				self.oldkey[key] = x[0]

		if flags and not flags.startswith("-"):
			flags = "-"+flags
		key = "-b'%s'" % key.strip("'")

		n = tf.eval( "/def %s %s = %s" % ( flags, key, eval_escape(body) ) )
		self.newkey.append( n )
		
		return n

	def status( self, argstr ):
		# save current fields
		if not self.status_saved:
			tf.eval( "/status_save _tf4status" )
			self.status_saved = True

		if not argstr.startswith("/"):
			argstr = "/status_"+argstr
		tf.eval( argstr )
		

	def lose_changes( self ):
		"""
		Lose track of all the changes you've made since creating this object.
		"""
		self.oldvar = {}    # { <name> : <oldvalue> }
		self.olddef = {}    # { <name> : <oldvalue> }
		self.newdef = []    # [ <n> ]
		self.oldkey = {}    # { <key>  : <oldvalue> }
		self.newkey = []    # [ <n> ]
		self.status_saved = False

	def revert( self ):
		"""
		Revert to previous state - this will undo all the setvar(), define(), and
		bind()s you've done.
		"""

		# variables are easy
		for var, value in self.oldvar.items():
			tf.eval( "/set %s=%s" % ( var, value ) )

		# get rid of our new definitions
		if self.newdef:
			tf.eval( "/undefn %s" % " ".join( [ str(n) for n in self.newdef ] ) )
		# restore the old ones
		for name, olddef in self.olddef.items():
			# '% 829: /def -p2 blah=  blah'
			tf.eval( eval_escape( olddef.split( ":", 1 )[1].lstrip() ) )

		# get rid of our new bindings
		if self.newkey:
			tf.eval( "/undefn %s" % " ".join( [ str(n) for n in self.newkey ] ) )
		# restore the old ones
		for name, oldkey in self.oldkey.items():
			tf.eval( eval_escape( olddef.split( ":", 1 )[1].lstrip() ) )

		# revert status line
		if self.status_saved:
			tf.eval( "/status_restore _tf4status" )
		
		# now forget all our changes
		self.lose_changes()

# We need this for the screenscrape hook
tf.eval( "/python_load tfutil" )


