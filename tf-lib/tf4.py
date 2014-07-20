#
# Converts your TF5 to act (sort of) like TF4.
#
# Witness the power of this fully operational death star! This demonstrates
# just how powerful TinyFugue's triggers are. With enough hooks you can
# really transform the behavior, though this is admittedly pretty obscene.
# I just wanted to prove that it was (mostly) possible.
#
# What this does is create a fake world then route all input/output through
# that world. This approach won't work if you're doing anything too fancy with
# your own macros. Unfortunately because of the way TF handles color we can't
# do anything with that - text color are hidden attributes, you can't just
# pass along ascii codes.
#
# NOTE: This seems to blow up on Python 2.5.0 (2.5.1 is fine) because it chokes
#       on the key=val argument calls for no reason I can find with a None
#       object error.
#
# Usage:
#    /python_load tf4
#    /tf4 help
#
# Copyright 2008 Ron Dippold
#
# v1.00 - Jan 25 '08 - Initial version

import tf, tfutil

#----------------------------------------------------------
# A glorious real world - which we're hiding
#----------------------------------------------------------
class World( object ):

	def __init__( self, name ):
		self.name = name
		self.socket = not not name

		global WDICT, WLIST
		WDICT[self.name.lower()] = self
		if self.socket:
			WLIST.append( self )
		self.buffer = []

	# send a line to the world
	def send( self, text ):
		if self.socket:
			tf.send( text, self.name )

	# called when receiving a line from the world
	def rx( self, text ):
		global FG, MODE

		# just accumulate in buffer
		if MODE == MODE_ON and FG != self:
			if not self.buffer:
				tf.out( "%% Activity in world %s" % self.name )
				ACTIVITY.append( self )
				_activity_status()
			self.buffer.append( text )
			return

		if MODE == MODE_SWITCH:
			self.activate()

		elif MODE == MODE_MIX:
			self.check_last()
			
		tf.out( text )

	def divider( self ):
		# show divider
		if self.name:
			tf.out( "---- World %s ----" % self.name )
		else:
			tf.out( "---- No world ----" )

	def check_last( self ):
		global LAST_OUT
		if LAST_OUT != self:
			self.divider()
			self.dump_buffer()
			LAST_OUT = self

	# activate this world
	def activate( self ):
		self.check_last()

		global FG
		if self != FG:
			FG = self
			tf.eval( "/set _tf4world=%s" % self.name )

	def dump_buffer( self ):
		# clean out buffer, no longer active world
		
		if not self.buffer:
			return
		for line in self.buffer:
			tf.out( line )
		self.buffer = []

		global ACTIVITY
		if self in ACTIVITY:
			ACTIVITY.remove( self )
			_activity_status()

	def connect( self ):
		global WLIST
		self.socket = True
		if not self in WLIST:
			WLIST.append( self )
		
		self.activate() #???

	def disconnect( self, reason ):
		try:
			WLIST.remove( self )
		except ValueError: pass
		self.socket = False
		

# -----------------------------------------------------------------------------
# Global data structures
# -----------------------------------------------------------------------------

# Known worlds - in dict format for fast lookup and list format for ordering.
# Since everything is by reference, this is cheap.
WDICT = {}
WLIST = []

# The world we're pretending is foreground
FG=World( '' )

# World we last showed output for
LAST_OUT=FG

# queue of worlds with activity
ACTIVITY = []

# Our current state
MODE_OFF, MODE_ON, MODE_MIX, MODE_SWITCH = ( 0, 1, 2, 3 )
MODE = MODE_OFF

# ------------------------------------------------------------------------------
# Helper funcs
# ------------------------------------------------------------------------------

# state singleton
STATE = None
def getstate():
	global STATE
	if not STATE:
		STATE = tfutil.State()
	return STATE


def _activity_status():
	if ACTIVITY:
		text = "(Active:%2d)" % len(ACTIVITY)
	else:
		text = ""
	tf.eval( "/set _tf4activity="+text )
	
# ------------------------------------------------------------------------------
# Our /python_call functions
# ------------------------------------------------------------------------------

# Called whenever we get a line from a world
def rx( argstr ):
	name, text = argstr.split( ' ', 1 )
	world = WDICT.get(name.lower())
	if not world:
		world = World( name )
		WDICT[name.lower()] = world
	world.rx( text )

# Dispatcher
def tx( argstr ):
	FG.send( argstr )

# Do a /dc - just don't let it /dc our _tf4 world!
def dc( argstr ):
	argstr = argstr.lower().strip()

	# do them all
	if argstr == "-all":
		for world in WLIST:
			tf.out( "/@dc %s" % world.name )
			tf.eval( "/@dc %s" % world.name )
			dischook( world.name + " tf4" )
		return

	# otherwise do foreground world
	if argstr:
		world = WDICT.get(argstr)
	else:
		world = FG

	if world:
		tf.eval( "/@dc %s" % world.name )
		dischook( world.name + " tf4" )

# change world
def fg( argstr ):
	cmd, opts, argstr = tfutil.cmd_parse( argstr, "ns<>c:|q" )
	if not opts and not argstr:
		return

	# no opts, just a world name
	if not opts:
		fg = WDICT.get( argstr.lower() )
		if fg:
			fg.activate()
		else:
			tf.out( "%% fg: not connected to '%s'" % argstr )
		return

	# handle opts
	fg = None

	# easy, just next activity world
	if 'a' in opts and ACTIVITY:
		fg = ACTIVITY[0]
		fg.activate()
		return

	# relative movement
	c = 0
	if 'c' in opts:
		try:
			c = int( opts['c'] )
		except:
			c = 1
	elif '<' in opts or 'a' in opts:
		c = -1
	elif '>' in opts:
		c = 1
		
	if c != 0:
		if WLIST:
			if FG in WLIST:
				fg = WLIST[(WLIST.index( FG ) + c) % len( WLIST )]
			else:
				fg = WLIST[0]
		else:
			fg = WDICT['']
	elif 'n' in opts:
		fg = WDICT['']
		
	if fg:
		fg.activate()


# pending hook, just show locally
def otherhook( argstr ):
	tf.out( argstr )

# world connected hook
def conhook( argstr ):
	name = argstr.split()[0]
	world = WDICT.get( name.lower() )
	if not world:
		# create a new world - adds itself to WDICT, WLIST
		world = World( name )

	tf.eval( "/@fg _tf4" ) # just in case
	world.connect()
		

# world disconnected hook
def dischook( argstr ):
	split = argstr.split(None,1)
	name = split[0].lower()
	reason = len(split)>1 and split[1] or ''
	if name in WDICT:
		WDICT[name].disconnect( reason )
	if reason != 'tf4':
		tf.out( "%% Connection to %s closed." % name )
	world( "-a" )
	
# connection request - just make sure we start in background
def connect( argstr ):
	cmd, opts, argstr = tfutil.cmd_parse( argstr, "lqxfb" )
	opts['b'] = ''
	if 'f' in opts:
		del opts['f']

	tf.out( tfutil.cmd_unparse( "/@connect", opts, argstr ) )
	tf.eval( tfutil.cmd_unparse( "/@connect", opts, argstr ) )

# world is a hybrid of connect and fg
def world( argstr ):
	cmd, opts, argstr = tfutil.cmd_parse( argstr, "lqxfb" )
	name = argstr.lower()
	if name and name in WDICT:
		name.activate()
	elif opts:
		fg( argstr )
	else:
		connect( argstr )

# Just make sure we have a specific world
def recall( argstr ):
	cmd, opts, argstr = tfutil.cmd_parse( argstr, 'w:ligvt:a:m:A:B:C:' )

	if not 'w' in opts or not opts['w']:
		opts['w'] = FG.name

	tf.eval( tfutil.cmd_unparse( "/@recall", opts, argstr ) )

def quit( argstr ):
	cmd, opts, argstr = tfutil.cmd_parse( argstr, 'y' )

	if not ACTIVITY:
		opts['y'] = ''

	tf.eval( tfutil.cmd_unparse( "/@quit", opts, argstr ) )

# ----------------------------------------------------------------------------
# Initial setup
# ----------------------------------------------------------------------------

def myhelp():
	for line in """
/tf4 (ON|mix|switch|off)

on:  Turns on TinyFugue 4 emulation mode where all world activity is shown
  in a single world separated by ---- World Foo ---- markers. This is
  emulated with hooks and scripts, so if your own scripts get too fancy
  it will break, but it seems to work pretty well with mine.

mix: Like 'on', but output in background worlds will be immediately shown
  (with a divider) so you can just watch the output of all worlds scroll
  by. However the world does not become the foreground world! Without
  /visual on you won't really have any indication of what the foreground
  world is so you won't know where you're sending text.

switch: Like 'mix' but immediately switches you to whatever world has output.
  Obviously this makes it tough to guarantee that any text you're sending
  will go to the right world unless you do a /send -wfoo text, since it
  could switch just before you hit enter.
 
off: Turn off TinyFugue 4 emulation mode, revert all your keybindings and
  macro definitions back to the way they were.
""".split("\n"):
		tf.out( line )

def tf4( argstr ):

	# Create a new base state if we don't have one
	state = getstate()


	# Just parse the command
	global MODE
	argstr = argstr.lower()
	if not argstr or 'on' in argstr:
		newmode = MODE_ON
		
	elif 'help' in argstr:
		return myhelp()

	elif 'off' in argstr:
		newmode = MODE_OFF

	elif 'mix' in argstr:
		newmode = MODE_MIX

	elif 'switch' in argstr:
		newmode = MODE_SWITCH

	else:
		return myhelp()

	# Now do what we need to
	if newmode == MODE_OFF:
		if newmode != MODE:
			state.revert()
			tf.eval( "/dc _tf4" )
			tf.eval( "/unworld _tf4" )
			sockets = tfutil.listsockets()
			for name in sockets:
				tf.eval( "/@fg -q " + name )
			MODE = MODE_OFF
		tf.out( "% tf4 mode is now off. '/tf4' to re-enable it" )
		return
		
	if MODE == MODE_OFF:
		# Create a virtual world to hold our text
		sockets = tfutil.listsockets()
		if not "_tf4" in sockets:
			tf.eval( "/addworld _tf4" )
			tf.eval( "/connect _tf4" )

			# We want background processing and triggers
			state.setvar( 'background', 'on' )
			state.setvar( 'bg_output', 'off' )
			state.setvar( 'borg', 'on' )
			state.setvar( 'gag', 'on' )
			state.setvar( 'hook', 'on' )

			# change the status bar
			state.setvar( '_tf4world', tf.eval( '/test fg_world()' ) )
			state.setvar( '_tf4activity', '' )
			state.status( "rm @world" )
			state.status( "add -A@more _tf4world" )
			state.status( "rm @active" )
			state.status( "add -A@read _tf4activity:11" )

			# Grab text from any world except our dummy world - we use a regexp instead of a glob
			# because the glob does not preserve the spacing
			state.define( flags="-p99999 -w_tf4 -ag -mglob -t*" )
			state.define( body="/python_call tf4.rx $[world_info()] %P1",
						  flags="-Fpmaxpri -q -mregexp -t(.*)" )
			
			# add a trigger for anything being sent to the generic world, so we can reroute
			# it to the right world
			state.define( body="/python_call tf4.tx %{*}", flags="-pmaxpri -w_tf4 -hSEND" )

			# Add our hooks
			state.define( body="/python_call tf4.dischook %{*}",
						  flags="-Fpmaxpri -ag -msimple -hDISCONNECT" )
			state.define( body="/python_call tf4.conhook %{*}",
						  flags="-Fpmaxpri -ag -msimple -hCONNECT" )
			state.define( body="/python_call tf4.otherhook %{*}",
						  flags='-p999 -ag -msimple -hCONFAIL|ICONFAIL|PENDING|DISCONNECT' )
			state.define( body="", flags="-pmaxpri -ag -msimple -hBGTRIG" )
			state.define( body="", flags="-pmaxpri -ag -msimple -hACTIVITY" )

			# Bind Esc-b, Esc-f to call us instead
			state.bind( key="^[b", body="/python_call tf4.fg -<", flags="i" )
			state.bind( key="^[f", body="/python_call tf4.fg ->", flags="i" )
			state.bind( key="^[w", body="/python_call tf4.fg -a", flags="i" )

			# def these commands to go to us
			state.define( "bg", "/python_call tf4.fg -n" )
			state.define( "connect", "/python_call tf4.connect %{*}" )
			state.define( "dc", "/python_call tf4.dc" )
			state.define( "fg", "/python_call tf4.fg %{*}" )
			state.define( "quit", "/python_call tf4.quit ${*}" )
			state.define( "recall", "/python_call tf4.recall %{*}" )
			state.define( "to_active_or_prev_world", "/python_call tf4.fg -a" )
			state.define( "to_active_world", "/python_call tf4.fg -a" ) # close enough
			state.define( "world", "/python_call tf4.world %{*}" )

	MODE = newmode
	if newmode == MODE_SWITCH:
		tf.out( "% TinyFugue 4 emulation mode with autoswitch to output world." )
	elif newmode == MODE_MIX:
		tf.out( "% TinyFugue 4 emulation mode with background output shown." )
	else:
		tf.out( "% TinyFugue 4 emulation mode is on." )
	tf.out( "% '/tf4 off' to disable, '/tf4 help' for help." )

# -------------------------------------
# When first loaded
# -------------------------------------

# register ourself
state = getstate()
state.define( name="tf4", body="/python_call tf4.tf4 %*", flags="-q" )
tf.out( "% '/tf4 help' for how to invoke TinyFugue 4 emulation mode." )
