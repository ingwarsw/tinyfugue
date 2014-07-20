#
# diffedit.py
#
# This attempts to help the problem of changing three lines in a 1000 line
# program and then having to painfully recreate it line by line or reupload
# the whole thing.
#
# It will remember the last version of your world/object and then diff it
# against the new one and only upload the differences. It will recognize
# that the same object in two different worlds is a different thing, but
# it will get confused if you try to do diffedits from two different
# computers on the same object (because it will apply changes twice) so
# use the -r (raw) flag to synchronize to a known state at least once.
#
# There needs to be a class defined for each type of editor supported.
# I have supplied editors for 'lsedit' and 'muf' on FuzzBall mucks.
#
# Copyright 2008 Ron Dippold sizer@san.rr.com
#
# v1.02 - Mar 06 08 - fix bug with no progress shown
# v1.01 - Jan 13 08 - Clean up docs, make LINES_PER_SECOND settable
# v1.00 - Jan 13 08 - First version

# You might want to configure these two
LASTDIR='/tmp/diffedit'
TABSIZE=4
LINES_PER_SECOND=50

# real stuff starts here
import tf, difflib, os, shutil

def help( dummy=None ):
	for line in """
diffedit usage:

  /python_call diffedit.upload (-<lines>) (-r) <editor> <remote> <filename>

  Uploads minimal commands necessary to turn centents of <remote> into
  <filename> by diff-ing against the last version. This is per <remote>
  per world.

  options:
    -<lines>  If -S or -0, send all commands immediately. Otherwise sends
              <lines> per second. Default: 50.
    -r        Just upload the entire thing. Useful if you think things might
              be out of sync, or to start fresh. The first time you edit a
              <remote> for a specific world -r is implied.
    -p(<n>)   Show progress every <n> lines. Default 100 if -p present.

  arguments:
    <editor>  Which editor class to upload with. Current choices are:
              ( lsedit | muf )
    <remote>  Which 'thing' to upload it to.
              lsedit: <dbref>=<list name>       ex: #1234=mydesc
              muf:    <dbref>                   ex: #1234
    <fname>   The filename that contains the content you want on the object.

Example:
   To work on my huge scream.muf progam I use:
     /python_call diffedit.upload muf -p scream.muf ~/muf/scream.muf
   Since this would be annoying to type every time I'd really use:
     /def scream=/python_call diffedit.upload muf -p scream.muf ~/muf/scream.muf
   so I can just type '/scream' in TF after I make local changes.

Other functions:
   /python_call diffedit.abort
       Emergency abort all in-progress uploads.
   /python_call diffedit.lastdir <directory>
       Set where the diffedit keeps the last known version of each file for a
	   <world>_<remote> to compare new versions against. Default: /tmp/diffedit
   /python_call diffedit.tabsize 8 
       Set the tab to space expansion size. 0 means no expansion. Default: 4
   /python_call diffedit.lines_per_second 100
       Set lines per second to send if no -<size>. Default: 50
   
""".split("\n"):
		tf.out( line )

#
# Configuration
#
def lastdir( dirname ):
	global LASTDIR
	LASTDIR=dirname

def tabsize( size ):
	global TABSIZE
	TABSIZE = int( size )

def lines_per_second( lps ):
	global LINES_PER_SECOND
	LINES_PER_SECOND = int( lps )

# -----------------------------------------------------
# Uploaders
# -----------------------------------------------------
class uploader( object ):

	def __init__( self, remote, name2, raw=False ):
		self.world = tf.world()
		
		self.remote = remote
		self.name2 = name2

		# look for old name
		self.keyname = "%s_%x" % ( self.world, abs( hash( self.remote) ) )
		self.name1 = os.path.join( LASTDIR, self.keyname )
		if not os.path.isdir( LASTDIR ):
			os.makedirs( LASTDIR, 0755 )

		# We have to be raw if the old version doesn't exist
		self.raw = raw or not os.path.isfile( self.name1 )

	# see generate_commands for a description of the opcodes
	def find_diffs( self ):

		# get contents of new and old
		self.lines2 = open( self.name2, "rt" ).readlines()
		if self.raw:
			lines1 = []
			# raw has an implied delete everything existing first
			diffs = [ [ 'delete', 0, 99999, 0, 0 ] ]
		else:
			lines1 = open( self.name1, "rt" ).readlines()
			diffs = [ ]

		# We need to do it in reversed so we get the right line numbers
		diffs += reversed( difflib.SequenceMatcher( None, lines1, self.lines2 ).get_opcodes() )

		# if everything's equal then it's just an 'empty' diff
		if len(diffs)==1 and diffs[0][0]=='equal':
			return []
		else:
			return diffs

	# generate streamlined sets of opcodes from SequenceMatcher opcodes
	def massage_opcodes( self, ops ):
		for opcode, i1, i2, j1, j2 in ops:
			if opcode == 'equal':
				continue
			if opcode in ( 'delete', 'replace' ):
				yield [ 'd', i1+1, 0, i2-i1 ]
			if opcode in ( 'insert', 'replace' ):
				yield [ 'i', i1+1, j1+1, j2-j1 ]

	# This is all you have to override, generally.
	# You get a list of:
	#   [ [ opcode, pos1, pos2, n ], ]
	# All line numbers are 1 based.
	#   'd': delete n lines at position pos1. ignore pos2
	#   'i': insert n lines (of file2) from pos2 at position pos1
	def generate_commands( self, opcodes ):
		raise Exception( "your uploader class needs to def generate_commands()" )

	# Finally send them to the world
	def upload_commands( self, lps, prog, cmds ):
		self.up = self.chunks( lps, cmds )
		self.lines_sent, self.lines_sent0 = 0, 0
		self.progress_size = prog
		
		self.upload_chunk()

	# returns lps chunks of cmds at a time - or all if no chunking
	def chunks( self, lps, cmds ):
		# if lps is 0 just send the whole thing at once
		if not lps:
			yield True, cmds

		# generate chunks of lps lines at a time
		chunk = []
		while True:
			try:
				chunk.append( cmds.next() )
			except StopIteration:
				yield True, chunk

			if len( chunk ) >= lps:
				yield False, chunk
				chunk = []

	# upload the next chunk of lps lines
	def upload_chunk( self ):

		done, chunk = self.up.next()

		for cmd in chunk:
			cmd = cmd.rstrip()
			# convert tabs to spaces
			if TABSIZE>0:
				cmd = cmd.expandtabs( TABSIZE )
			# /send blank does nothing, so send a space
			if not cmd:
				cmd = " "

			# and send it
			tf.send( cmd, self.world )
			self.lines_sent += 1

		# clean up and finish, or schedule further upload
		if done:
			self.done()
			del self.up
		else:
			# show progress
			if self.progress_size and ( self.lines_sent - self.lines_sent0)>=self.progress_size:
				tf.out( "- diffedit.upload -> %s %s - %4d cmds sent" % \
						( self.world, self.remote, self.lines_sent ) )
				self.lines_sent0 = self.lines_sent - ( self.lines_sent % self.progress_size )
			# schedule a callback in 1 second
			INPROGRESS[self.keyname] = self
			tf.eval( '/repeat -w%s -1 1 /python_call diffedit.__more %s' % (
				self.world, self.keyname ) )

	def done( self ):
		"""copy new file to old file, print done message"""

		# no longer running
		if self.keyname in INPROGRESS:
			del INPROGRESS[self.keyname]

		# let this just IOError out if it wants, then user sees error
		shutil.copy2( self.name2, self.name1 )

		# tell user what we did
		print "%% diffedit.upload -> %s %s - %4d cmds sent - done" % \
			  ( self.world, self.remote, self.lines_sent )

#
# lsedit uploader
#
class up_lsedit( uploader ):
	def __init__( *args ):
		uploader.__init__( *args )

	# see class uploader for opcodes format
	def generate_commands( self, opcodes ):
		yield "lsedit %s" % self.remote

		for opcode, pos1, pos2, n in opcodes:
			if opcode == 'd':
				yield ".del %d %d" % ( pos1, pos1+n-1 )
			elif opcode == 'i':
				yield ".i %d" % ( pos1 )
				for i in xrange( pos2, pos2+n ):
					yield self.lines2[i-1]

		yield ".end"
			
#
# muf uploader
#
class up_muf( uploader ):
	def __init__( *args ):
		uploader.__init__( *args )

	# see class uploader for opcodes format
	def generate_commands( self, opcodes ):
		yield "@edit %s" % self.remote

		for opcode, pos1, pos2, n in opcodes:
			if opcode == 'd':
				yield "%d %d d" % ( pos1, pos1+n-1 )
			elif opcode == 'i':
				yield "%d i" % ( pos1 )
				for i in xrange( pos2, pos2+n ):
					yield self.lines2[i-1]
				yield "."
				
		yield "c"
		yield "q"

			
# -----------------------------------------------------
# Main logic
# -----------------------------------------------------

# Keep uploading something we started
def __more( argstr ):
	editor = INPROGRESS.get( argstr, None )
	if not editor:
		return

	#print "uploading more for %s" % ( editor.name2 )
	editor.upload_chunk()

# abort all uploads! oogah, oogah!
def abort( argstr ):
	tf.out( "* diffedit.abort - aborting all in progress uploads" )
	global INPROGRESS
	INPROGRESS = {}

# start a new upload
def upload( argstr ):

	# 
	# argument parsing is much larger than actual logic!
	#

	if not argstr:
		return help()

	args = argstr.split()

	# check for raw flag
	if "-r" in args:
		raw = True
		args.remove( "-r" )
	else:
		raw = False

	# lines per second and progress
	lps, prog = LINES_PER_SECOND, 0
	args2 = []
	for i, arg in enumerate( args ):

		# progress
		if arg.startswith( "-p" ):
			prog = 100
			if len(arg) > 2:
				try:
					prog = int(arg[2:])
				except ValueError:
					return help()

		# no waiting
		elif arg in ( '-S', '-0' ):
			lps = 0

		# fractional seconds
		elif arg.startswith("-"):
			try:
				lps = float(arg[1:])
				if lps<0:
					lps = abs( lps )
				if lps<1:
					lps = 1/lps
			except ValueError:
				pass

		else:
			args2.append( arg )

	# what's left?
	args = args2
	if len( args ) != 3 :
		return help()

	# which type
	if args[0] == 'muf':
		editor = up_muf
	elif args[0] == 'lsedit':
		editor = up_lsedit
	else:
		tf.err( "* diffedit.upload: known editors are 'muf' and 'lsedit'" )
		return -1

	# Check for the file
	if not os.path.isfile( args[2] ):
		tf.err( "* diffedit.upload: file %s not found" % args[2] )
		return -1

	#
	# Okay, here's we we actually do some work
	#

	# Create the object
	editor = editor( args[1], args[2], raw )

	# Check to make sure we're not already running
	if editor.keyname in INPROGRESS:
		tf.err( "* diffedit.upload: upload for '%s:%s' already running." % (
			editor.world, editor.remote ) )
		tf.err( "*     use '/python_call diffedit.abort' to reset if this is wrong." )
		return

	# find the diffs
	diffs = editor.find_diffs()

	# only do the rest if we need to
	if diffs:
		
		# generate the editor commands
		cmds = editor.generate_commands( editor.massage_opcodes( diffs ) )

		# upload them
		editor.upload_commands( lps, prog, cmds )

	else:
		
		# all done
		editor.lines_sent = 0
		editor.done()


# any uploads in progress?
INPROGRESS = {}
