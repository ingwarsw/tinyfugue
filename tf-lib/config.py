#
# config.py
#
# This attempts to manage your TinyFugue configuration, letting you select,
# edit, and save your worlds using /commands. Will expand this to key bindings
# later. This is my first curses program, so it's pretty uuuuugly, sorry.
#
# Usage:
#   /python_load config
#   /python_call config.worldsfile ~/.tf/worlds      (optional)
#   /worlds
#
# If you don't do the config.worldsfile() to tell it where your worlds are
# located it will attempt to figure it out by looking for a /loadworlds in
# your .tfrc. Or if it finds addworld commands in your .tfrc it will assume
# it should just dump them in there.
#
#
# Copyright 2008 Ron Dippold sizer@san.rr.com
#
# v1.01 - Jan 23 '08 - Convert to using tfutil
# v1.00 - Jan 20 '08 - First version
#
import curses, os
import tf, tfutil

# ------------------------------------------------------
# Where's the worldsfile?
# ------------------------------------------------------

WORLDSFILE=None
def worldsfile( fname ):
	global WORLDSFILE
	WORLDSFILE=fname

def _find_worldsfile():
	# already found
	global WORLDSFILE
	if WORLDSFILE:
		return WORLDSFILE

	# look in tfrc
	tfrc = tf.tfrc()
	if not tfrc:
		return None

	# search through tfrc
	f = open( tfrc, "rU" )
	for line in f:
		if line.startswith( "/loadworld " ):
			WORLDSFILE=line.split()[1].strip()
			break
		if "addworld" in line:
			WORLDSFILE = tfrc
			break
	f.close()
	if WORLDSFILE:
		WORLDSFILE = os.path.abspath( os.path.expanduser( WORLDSFILE ) )
	return WORLDSFILE
	
# ------------------------------------------------------
# helpers
# ------------------------------------------------------


# show help line with [f]oo as <b>f</b>oo
def _showkeys( window, y, x, fields ):

	maxy, maxx = window.getmaxyx()
	window.addnstr( y, x, " "*132, maxx-x-2 )

	window.addstr( y, x, fields.replace('[','').replace(']','') )
	offset, pos= 0, 0
	while True:
		pos = fields.find( '[', pos )
		if pos<0: break
		offset += 1
		end = fields.find( ']', pos )
		if end<0: break
		window.addstr( y, x+pos-offset+1, fields[pos+1:end], curses.A_BOLD )
		pos = end
		offset += 1

# -----------------------------------------------------------------------
# Edit the worlds
# -----------------------------------------------------------------------

def worlds( argstr ):
	# wrap the function in case it crashes
	curses.wrapper( _worlds )
	# screen is all messed up, redraw it
	tf.eval( "/dokey REDRAW" )

def _new_undo( undo, wdict, message, saved ):
	undo.append( ( message, dict( [ ( key, tfutil.World(val) ) \
									for key, val in wdict.items() ] ), saved ) )

SAVED = True
def _change_worlds( old, wdict ):
	# remove old worlds
	for name, item in old.items():
		if name not in wdict:
			tf.eval( "/unworld " + name )

	# add new or changed worlds
	for name, item in wdict.items():
		cmd = item.addworld_command( func=True, full=True )
		if name not in old:
			if cmd: tf.eval( cmd )
		elif item.changed_from(old[name]):
			tf.eval( "/unworld " + name )
			if cmd: tf.eval( cmd )

def _save_worlds2( fout, wdict ):
	for name, item in sorted(wdict.items()):
		cmd = item.addworld_command( func=False, full=True )
		if cmd:
			fout.write(  cmd + '\n' )
	return True

def _save_worlds( wdict ):
	fname = _find_worldsfile()
	if not fname:
		return False, "Can't find world file. Use '/python_call config.worldsfile <filename>'"

	# Read the old lines except for addworld, write to new file
	try:
		fout = open( fname+".xxx", "wt", 0600 )
	except IOError, e:
		return False, "Error opening %s for write: %s" % ( e.filename, e.strerror )

	fin = open( fname, "rU" )
	written = False
	
	for line in fin:
		if "addworld" in line:
			if not written:
				written = _save_worlds2( fout, wdict )
			continue
		# write, converting line endings
		fout.write( line  )
	fin.close()

	# write all the worlds to the end of the file if haven't done it yet
	if not written:
		written = _save_worlds2( fout, wdict )
	fout.close()

	# move the new file over the old one
	os.rename( fin.name, fin.name+".bak" )
	os.rename( fout.name, fin.name )
	tf.out( "- saved /worlds to %s" % fin.name )

	return True, None

# This is the real worlds function, invoked by curses safety wrapper
def _worlds( stdscr ):

	# get dictionary of current worlds
	wdict = tfutil.listworlds( asdict=True )

	cols, lines = tfutil.screensize()
	colors = curses.can_change_color()

	# Draw the border and create a world window
	stdscr.clear()
	stdscr.border()
	stdscr.addstr( 0, 4, "| TinyFugue Worlds |", curses.A_BOLD )
	wpos, lastwpos = 0, 9999

	worldwin, scrollwin = _worldwin( stdscr, lines-4, cols-4, 2, 2 )
	#worldwin, scrollwin = _worldwin( stdscr, 15, cols-4, 2, 2 )
	worldwin.refresh()

	# start an undo stack
	undo, redo = [], []
	global SAVED

	# now loop
	message, lastmessage = None, "dummy"
	while True:

		# sort by name
		wlist = sorted( wdict.values() )

		# draw the list, get a command
		_drawworlds( wlist, scrollwin, wpos, lastwpos )
		lastwpos = wpos

		#
		# parse keys
		#
		c = scrollwin.getkey()

		# Movement
		if c in ( 'i', 'KEY_UP', '\x10' ):
			if wpos>0:
				wpos -= 1

		elif c in ( 'j', 'KEY_DOWN', '\x0e' ) :
			if (wpos+1) < len( wlist ):
				wpos += 1

		# actual editing
		elif wlist and c in ( 'd', 'KEY_DC' ):
			message = 'Deleted world %s' % wlist[wpos].name
			_new_undo( undo, wdict, message, SAVED )
			del wdict[ wlist[wpos].name ]
			wpos = min( wpos, len(wdict)-1 )
			SAVED, lastwpos = False, 9999

		elif wlist and c in ( 'c', ):
			w = wlist[wpos]
			for i in range( 2, 99 ):
				newname = '%s(%d)' % ( w.name, i )
				if not newname in wdict:
					message = 'Copied world %s' % newname
					_new_undo( undo, wdict, message, SAVED )
					newworld = tfutil.World(w)
					newworld.name = newname
					wdict[newname] = newworld
					SAVED, lastwpos = False, 9999
					break

		elif c in ( 'a', 'KEY_INS' ):
			w = _editworld( worldwin, wdict.keys(), None )
			if w:
				message = 'Added world %s' % w.name
				_new_undo( undo, wdict, message, SAVED )
				wdict[ w.name ] = w
				SAVED = False
			lastwpos = 9999
			_worldwin_redraw( worldwin )

		elif wlist and c in ( 'e', ):
			oldname = wlist[wpos].name
			w = _editworld( worldwin, wdict.keys(), tfutil.World(wlist[wpos]) )
			if w:
				message = 'Edited world %s' % w.name
				_new_undo( undo, wdict, message, SAVED )
				if oldname!=w.name:
					del wdict[oldname]
				wdict[ w.name ] = w
				SAVED = False
			lastwpos = 9999
			_worldwin_redraw( worldwin )

		# undo/redo
		elif c == 'u':
			if undo:
				message, wdict2, SAVED2 = undo.pop()
				_new_undo( redo, wdict, message, SAVED )
				wdict, SAVED = wdict2, SAVED2
				message = "Undid: " + message
				lastwpos = 9999
			else:
				message = 'Nothing to undo'

		elif c == 'r':
			if redo:
				message, wdict2, SAVED2 = redo.pop()
				_new_undo( undo, wdict, message, SAVED )
				wdict, SAVED = wdict2, SAVED2
				message = "Redid: " + message
				lastwpos = 9999
			else:
				message = 'Nothing to redo'


		# Anything that terminates us
		elif wlist and c in ( '\n', 'KEY_ENTER', 'Q' ):
			if undo:
				_change_worlds( undo[0][1], wdict )
			if c != 'Q':
				tf.eval( "/connect %s" % wlist[wpos].name )
			if not SAVED:
				tf.err( "* Warning: your /worlds haven't been saved yet" )
			break
			
		elif c == 'S':
			if undo:
				_change_worlds( undo[0][1], wdict )
			if not SAVED:
				SAVED, message = _save_worlds( wdict )
			if SAVED:
				break

		elif c == 'A':
			if not SAVED:
				tf.err( "* all /worlds changes aborted" )
			SAVED = False
			break

		message, lastmessage = _show_message( stdscr, message, lastmessage )


def _show_message( window, message, lastmessage ):
	y, x = window.getmaxyx()
	if message:
		window.addnstr( 1, 1, " " + message + " "*x, x-2, curses.A_REVERSE )
		lastmessage = message
		window.refresh()
	elif lastmessage:	
		window.addnstr( 1, 1, " "*x, x-2, curses.A_NORMAL )
		window.refresh()
		lastmessage = None
	return None, lastmessage
			

def _worldwin_redraw( window ):
	lines, cols = window.getmaxyx()
	window.erase()

	# title
	window.addnstr( 0, 0, "World      Character         Host                  Port",
					cols, curses.A_BOLD )
	# help line
	_showkeys( window, lines-1, 0,
			   '[enter] - [a]dd [c]opy [d]el [e]dit - [u]ndo/[r]edo - [S]ave [Q]uit [A]bort' )


def _worldwin( parent, lines, cols, y, x ):
	lines -= 4
	cols -= 4
	window = parent.derwin( lines, cols, 2, 2 )
	window.keypad(1)

	_worldwin_redraw( window )
	
	scrollwin = window.derwin( lines-2, cols, 1, 0 )
	scrollwin.keypad(1)
	scrollwin.erase()

	return window, scrollwin

def _drawworlds( wlist, window, wpos, lastwpos ):

	# figure out the top world we're showing
	lines, cols = window.getmaxyx()
	top = wpos - lines + 1
	if top<0: top = 0

	lasttop = lastwpos - lines + 1
	if lasttop<0: lasttop=0

	# Try to avoid redrawing the whole thing
	start, end = min( lastwpos, wpos ), max( lastwpos, wpos )+1
	if top == lasttop:
		pass
	elif top == (lasttop+1):
		window.move( 0, 0 )
		window.deleteln()
	else:
		start, end = 0, len( wlist )

	# list each visible world
	for y in range( lines ):
		window.move( y, 0 )
		i = top + y
		if i >= len( wlist ):
			window.clrtoeol()
		else:
			if i<start or i>=end:
				continue
			attrib = (wpos == i) and curses.A_REVERSE or curses.A_NORMAL
			item = wlist[i]
			ssl = ( 'x' in item.flags ) and '*' or ' '
			window.addnstr( "%-10s %-16s %s%-20s %5s" % \
							( item.name, item.character, ssl, item.host, item.port ),
							cols, attrib )
	
	window.move( wpos-top, 0 )
	window.refresh()

# Define our functions
tf.eval( "/def worlds=/python_call config.worlds" )


# ---------------------------------------------------------------------------
# Editing a world
# ---------------------------------------------------------------------------

def _validate_port( world, worldnames, value ):
	try:
		if int(value)>0 and int(value)<65536:
			return None
	except:
		pass
	return "Port number must be from 1 and 65535."

def _validate_name( world, worldnames, value ):
	if not value:
		return "The world name may not be blank."
	if value in worldnames:
		return "There is already a world with that name."
	return None

# Required:
#   'name' : label
#   'help' : help field
# Either 'key' or 'get'/'set' must be set:
#   'key'  : name of dict key
#   'get'  : function that returns value from ( world )
#   'set'  : function that sets val in world from ( world, val )
# Optional:
#   'edit' : function to edit val from ( world, val ) if you don't want textpad
#   'validate': function passed ( world, worldnames, value ) and returns error
#               message, or None if it passes validation
EDITFIELDS = [
	{ 'name':	'World',
	  'help':	'Name of the world, used for /world',
	  'key':	'name',
	  'validate': _validate_name,
	  },
	{ 'name':	'Type',
	  'help':	'Mu* type: aber, diku, lp, lpp, telnet, tiny',
	  'key':	'type',
	  },
	{ 'name':	'Host',
	  'help':	'Host name or IP address',
	  'key':	'host',
	  },
	{ 'name':	'Port',
	  'help':	'Port number - 1 to 65535',
	  'key':	'port',
	  'validate': _validate_port,
	  },
	{ 'name':	'Character',
	  'help':	'Character name for auto-logon',
	  'key':	'character',
	  },
	{ 'name':	'Password',
	  'help':	'Character password for auto-logon',
	  'key':	'password',
	  },
	{ 'name':	'Use SSL',
	  'help':	'Use SSL to connect to the world (if compiled in)',
	  'yesno':	True,
	  'edit':	lambda w,v: v == 'Yes' and 'No' or 'Yes',
	  'get':	lambda w: ( 'x' in w.flags ) and 'Yes' or 'No',
	  'set':	lambda w,v: _setflag( w, 'x', v ),
	  },
	{ 'name':	'Ignore Proxy',
	  'help':	'ignore %{proxy_host} if set',
	  'yesno':	True,
	  'edit':	lambda w,v: v == 'Yes' and 'No' or 'Yes',
	  'get':	lambda w: ( 'p' in w.flags ) and 'Yes' or 'No',
	  'set':	lambda w,v: _setflag( w, 'p', v ),
	  },
	{ 'name':	'Echo input',
	  'help':	'Echo back all text sent to the world',
	  'yesno':	True,
	  'edit':	lambda w,v: v == 'Yes' and 'No' or 'Yes',
	  'get':	lambda w: ( 'e' in w.flags ) and 'Yes' or 'No',
	  'set':	lambda w,v: _setflag( w, 'e', v ),
	  },
	{ 'name':	'Macro file',
	  'help':	'Macro file to /load on connect',
	  'key':	'file',
	  },
	{ 'name':	'Src Host',
	  'help':	'Set source IP to bind to a specific interface',
	  'key':	'srchost',
	  },
	]

def _setflag( world, flag, val ):
	add = val.lower().startswith("y") and flag or ""
	world.flags = world.flags.replace(flag,'') + add


def _getfield( world, field ):
	if 'get' in field:
		return field['get'](world)
	else:
		return getattr( world, field['key'] )

def _setfield( world, field, value ):
	if 'set' in field:
		field['set'](world,value)
	else:
		setattr( world, field['key'], value )

def _editworld( worldwin, worldnames, world ):

	import curses.textpad

	# if the world is empty, create a default world
	if not world:
		world = World()
	else:
		worldnames.remove( world.name )

	# create a new window as a subwindow of the old
	lines0, cols0 = worldwin.getmaxyx()
	lines = min( lines0-4, 17 )
	cols = cols0-4
	y = (lines0-lines)/2
	editwin = worldwin.derwin( lines, cols, y, 2 )
	editwin.keypad( 1 )
	editwin.erase()
	editwin.border()
	editwin.addstr( 0, 4, "| Editing World: %s |" % (world.name and world.name or '<new>'), \
					curses.A_BOLD )

	_showkeys( editwin, lines-2, 2,
			   '[d]elete [enter]/[e]dit - [Q]/[S]ave [A]bort' )

	# now loop
	pos=0
	message, lastmessage = None, None
	while True:

		# Paint all the fields
		y, x = 2, 2
		for i, field in enumerate( EDITFIELDS ):
			editwin.addstr( y, x, field['name']+':', curses.A_BOLD )
			editwin.addnstr( y, x+15, _getfield( world, field ) + " "*80, cols-19,
							i==pos and curses.A_REVERSE or curses.A_NORMAL )
			y += 1


		# get input
		while True:

			# show the help for the current item
			field = EDITFIELDS[pos]
			editwin.addnstr( y+1, 2, field['help']+(" "*80), cols-4 )

			# get input
			editwin.move( 2+pos, 17 )
			editwin.refresh()
			c = editwin.getkey()

			if c in ( 'i', 'KEY_UP', '\x10' ):
				if pos>0:
					pos -= 1
					break
			elif c in ( 'j', 'KEY_DOWN', '\x0e' ):
				if (pos+1) < len( EDITFIELDS ):
					pos += 1
					break
			elif c in ( 'e', 'KEY_ENTER', ' ', '\n', 'd', 'KEY_DC' ):
				value = _getfield( world, field )
				if c in ( 'd', 'KEY_DC' ):
					value = ''
				elif 'edit' in field:
					value = field['edit'](world, value )
				else:
					# create a text editing box
					linewin = editwin.derwin( 1, cols-19, 2+pos, 17 )
					linewin.erase()
					if c != ' ':
						linewin.addstr( 0, 0, value )
					textpad = curses.textpad.Textbox( linewin )
					curses.noecho()
					value = textpad.edit().strip()
				if ' ' in value:
					message = 'No spaces allowed in values'
				elif 'validate' in field:
					message = field['validate']( world, worldnames, value )
					if not message:
						_setfield( world, field, value )
						
				else:
					_setfield( world, field, value )
				break
			elif c in ( 'y', 'n', 'Y', 'N' ):
				if 'yesno' in field:
					_setfield( world, field, c )
					break
			elif c in ( 'S', 'Q' ):
				# return world only if it has a name
				if world.category!=world.INVALID:
					return world
				else:
					return None
			elif c in ( 'A', 'KEY_ESC' ):
				return None


		message, lastmessage = _show_message( editwin, message, lastmessage )


tf.out( "% config.py loaded: use '/worlds' to edit your worlds" )
