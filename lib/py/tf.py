# This is a dummy tf module to allow module testing outside of TinyFugue.
#
# If you are actually /python_load-ing or import-ing the module in TF
# then this file is NOT LOADED, it is only a stub for debugging.

def err( argstr ):
	print "tf.err  |", argstr

def eval( argstr ):
	print "tf.eval |", argstr
	return ""

def getvar( var, default="" ):
	print "tf.getvar |", var, default
	return default

def out( argstr ):
	print "tf.out  |", argstr

def send( text, world="<current>" ):
	print "tf.send %s |" % test
	
def world():
	return "Dummy"

