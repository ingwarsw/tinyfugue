########################################################################
#
#  TinyFugue - programmable mud client
#  Copyright (C) 1994, 1995, 1996, 1997, 1998, 1999, 2002, 2003, 2004, 2005, 2006-2007 Ken Keys
#
#  TinyFugue (aka "tf") is protected under the terms of the GNU
#  General Public Licence.  See the file "COPYING" for details.
#
########################################################################



LONGPATH=${PATH}:@PATH@

default: files

files all install tf clean uninstall: _force_
	@cd src; PATH="${LONGPATH}" ${MAKE} $@

_force_:

# The next line is a hack to get around a bug in BSD/386 make.
make:
