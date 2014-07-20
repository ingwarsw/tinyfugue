#---------------------------------------------------------------------------
# Python URL Watcher v2.00 - By Vash@AnimeMUCK ( sizer@san.rr.com )
#
#    Distribute at will! Though I'd like to hear of improvements.
#
# This is formatted for 4-space tabs.
#
# This will watch your worlds and write an html file with the urls it sees
# go by. Then you can point your browser at the file and launch the urls
# from there. It puts the newest urls at the top.
#
# THIS REQUIRES THE TF5 PYTHON PATCH from http://sizer99.com/tfpython/
#
# ---------------------------------------------------------------------------
# -- INSTALL
# - Set the CONFIG variables just below.
# - in your .tfrc file put a
#       /python_load /path/to/urlwatch.py
# - and optionally do a
#       /python_call urlwatch.config file=/tmp/tfurls.html
#   if you want to override where the html file is written
# ---------------------------------------------------------------------------
# -- VERSIONS
# - V 2.00 - Jan 09 '08 - Convert to python from tfscript
# ---------------------------------------------------------------------------

# -- CONFIGURATION

# You can use '/python_call urlwatch.config <key>=<value>' to change any of
# these is well - but it won't do any error checking.

CONFIG = {

	# This is the file which gets written and you need to view in your browser.
	#  In Firefox, File -> Open File..., in IE,  File -> Open -> Browse
	'file':		'/tmp/tfurls.html',

	# change target to '' if you don't want urls opening in a new window/tab
	'target':	'_blank',

	# Title for the browser title bar/tab title
	'title':	'TinyFugue URLs',

	# Define your own style sheet info here
	'css':		"""
td { padding: 0.5em 1em; }
.odd { background: #f0fff0; }
.even { background: #f0f0ff; }
""",

	# How many urls you want to save for the page, default 20
	'max':		20,

	# How often you want the page to auto-reload, in seconds (or 0 for not)
	'reload':	30,

	# anything else you want before the table of urls
	'header':	'<a href="javascript:window.location.reload()" class="reload">reload page</a>',
}

# ---------------------------------------------------------------------------
# Below here is the actual code
# ---------------------------------------------------------------------------

# regexp for matching
import re, cgi, tf
PATTERN = r'(http|ftp|https)://\S+[0-9A-Za-z/]'
CPATTERN = re.compile( PATTERN )

# our list of urls - each URL is ( world, text )
URLS = []

def trigger( arg ):
	"Called from our tinyfugue trigger to do the real work"

	# parse the rest as long as we keep finding urls
	line = arg
	output=[]
	while True:
		found = CPATTERN.search( line )
		if not found: break
		start, end = found.span()
		url = cgi.escape( line[start:end] )
		output.append( cgi.escape( line[:start] ) )
		output.append( '<a href="%s" %s>%s</a>' % ( url, CONFIG['target'], url ) )
		line = line[end:]
	# add the remaining to the end
	if line:
		output.append( cgi.escape( line ) )

	# add the output to the URLS, then truncate the list
	global URLS
	URLS.append( ( tf.world(), "".join( output ) ) )
	URLS = URLS[ :CONFIG['max'] ]

	# finally, write the file
	write_file()

def write_file( ):
	"Write the file using the known URL entries"

	f = open( CONFIG['file'], 'wt' )
	f.write( '<head><title>%s</title>\n' % CONFIG.get('title') )
	if CONFIG.get( 'reload' ):
		f.write( '<meta http-equiv="refresh" content="%s" />' % CONFIG['reload'] )
	if CONFIG.get( 'css' ):
		f.write( '<style type="text/css">%s</style>\n' % CONFIG['css'] )
	f.write( '</head><body>\n' )
	f.write( CONFIG.get( 'header', '' ) )

	# write each URL
	if URLS:
		f.write( '<table>\n' )
		odd = True
		for world, url in reversed( URLS ):
			classname = odd and "odd" or "even"
			odd = not odd
			f.write( '  <tr class="%s"><td>%s</td><td>%s</td></tr>\n' % ( classname, world, url ) )
		f.write( '</table>\n' )
	else:
		f.write( "<br/>No URLs caught yet.<br/>\n" )
	f.write( '</body>\n' )
		
	f.close()
		

def config_massage():
    	"Do a little CONFIG fixup."
	if CONFIG.get('target'):
		CONFIG['target'] = 'target="%s"' % CONFIG['target']
	else:
		CONFIG['target'] = ''

	try:
                CONFIG['max'] = int( CONFIG['max'] )
	except:
		CONFIG['max'] = 20

# You could also just do this with
#     /python urlwatch.CONFIG['file']='value'
# but that seems a little too fingers-in-pies
def config( keyval ):
	if '=' in keyval:
		key, val = keyval.split( '=', 1)
		CONFIG[ key ] = val
		config_massage()
	else:
		tf.echo( "use: /python_call urlwatch.config <key>=<value>" )
	
# ---------------------------------------------------------------------------
# Initialization	
# ---------------------------------------------------------------------------

# Do some CONFIG massaging
config_massage()
	
# create the trigger
tf.eval( "/def -mregexp -p9 -F -q -t%s urlwatch = /python_call urlwatch.trigger \\%%*" %
		 PATTERN.replace('//','///').replace('\\','\\\\')
		 )

# write an empty file so there's at least something there
write_file()

