# Make file for PCRE (Perl-Compatible Regular Expression) library.

# If you are using a Unix system, see below. I am a Unix person, so that is
# the stuff I really know about. PCRE is developed on a Unix box.

# To build mingw32 DLL uncomment the next two lines. This addition for mingw32
# was contributed by  Paul Sokolovsky <Paul.Sokolovsky@technologist.com>. I
# (Philip Hazel) don't know anything about it! There are some additional
# targets at the bottom of this Makefile.
#
# include dll.mk
# DLL_LDFLAGS=-s


######## NON-UNIX ############ NON-UNIX ############## NON-UNIX ##############
# If you want to compile PCRE for a non-Unix system, note that it consists
# entirely of code written in Standard C, and so should compile successfully
# using normal compiling commands to do the following:
#
# (1) Compile dftables.c as a stand-alone program, and then run it with
# output sent to chartables.c. This generates a set of standard character
# tables.
#
# (2) Compile maketables.c, get.c, study.c and pcre.c and link them all
# together. This is the pcre library (chartables.c gets included by means of
# an #include directive).
#
# (3) Compile pcreposix.c and link it as the pcreposix library.
#
# (4) Compile the test program pcretest.c. This needs the functions in the
# pcre and pcreposix libraries when linking.
#
# (5) Run pcretest on the testinput files, and check that the output matches
# the corresponding testoutput files. You must use the -i option with
# testinput2.


######## UNIX ################## UNIX ################## UNIX ################
# On a Unix system:
#
# Edit CC, CFLAGS, and RANLIB for your system.
#
# It is believed that RANLIB=ranlib is required for AIX, BSDI, FreeBSD, Linux,
# MIPS RISCOS, NetBSD, OpenBSD, Digital Unix, and Ultrix.
#
# Use CFLAGS = -DUSE_BCOPY on SunOS4 and any other system that lacks the
# memmove() function, but has bcopy().
#
# Use CFLAGS = -DSTRERROR_FROM_ERRLIST on SunOS4 and any other system that
# lacks the strerror() function, but can provide the equivalent by indexing
# into errlist.

AR = ar cq
CC = gcc -O2 -Wall
CFLAGS =
RANLIB = @true

# If you are going to obey "make install", edit these settings for your
# system. BINDIR is the directory in which the pgrep command is installed.
# INCDIR is the directory in which the public header file pcre.h is installed.
# LIBDIR is the directory in which the libraries are installed. MANDIR is the
# directory in which the man pages are installed. The pcretest program, as it
# is a test program, does not get installed anywhere.

BINDIR = /usr/local/bin
INCDIR = /usr/local/include
LIBDIR = /usr/local/lib
MANDIR = /usr/local/man


##############################################################################


OBJ = maketables.o get.o study.o pcre.o

all:            libpcre.a libpcreposix.a pcretest pgrep

pgrep:          libpcre.a pgrep.o
		$(CC) $(CFLAGS) -o pgrep pgrep.o libpcre.a

pcretest:       libpcre.a libpcreposix.a pcretest.o
		$(PURIFY) $(CC) $(CFLAGS) -o pcretest pcretest.o libpcre.a libpcreposix.a

libpcre.a:      $(OBJ)
		-rm -f libpcre.a
		$(AR) libpcre.a $(OBJ)
		$(RANLIB) libpcre.a

libpcreposix.a: pcreposix.o
		-rm -f libpcreposix.a
		$(AR) libpcreposix.a pcreposix.o
		$(RANLIB) libpcreposix.a

pcre.o:         chartables.c pcre.c pcre.h internal.h Makefile
		$(CC) -c $(CFLAGS) pcre.c

pcreposix.o:    pcreposix.c pcreposix.h internal.h pcre.h Makefile
		$(CC) -c $(CFLAGS) pcreposix.c

maketables.o:   maketables.c pcre.h internal.h Makefile
		$(CC) -c $(CFLAGS) maketables.c

get.o:          get.c pcre.h internal.h Makefile
		$(CC) -c $(CFLAGS) get.c

study.o:        study.c pcre.h internal.h Makefile
		$(CC) -c $(CFLAGS) study.c

pcretest.o:     pcretest.c pcre.h Makefile
		$(CC) -c $(CFLAGS) pcretest.c

pgrep.o:        pgrep.c pcre.h Makefile
		$(CC) -c $(CFLAGS) pgrep.c

# An auxiliary program makes the default character table source

chartables.c:   dftables
		./dftables >chartables.c

dftables:       dftables.c maketables.c pcre.h internal.h Makefile
		$(CC) -o dftables $(CFLAGS) dftables.c

install:        all
		cp libpcre.a libpcreposix.a $(LIBDIR)
		cp pcre.h $(INCDIR)
		cp pgrep $(BINDIR)
		cp pcre.3 pcreposix.3 $(MANDIR)/man3
		cp pgrep.1 $(MANDIR)/man1

# We deliberately omit dftables and chartables.c from 'make clean'; once made
# chartables.c shouldn't change, and if people have edited the tables by hand,
# you don't want to throw them away.

clean:;         -rm -f *.o *.a pcretest pgrep

runtest:        all
		./RunTest

######## MINGW32 ############### MINGW32 ############### MINGW32 #############

# This addition for mingw32 was contributed by  Paul Sokolovsky
# <Paul.Sokolovsky@technologist.com>. I (PH) don't know anything about it!

dll:            _dll libpcre.dll.a pgrep_d pcretest_d

_dll:
		$(MAKE) CFLAGS=-DSTATIC pcre.dll

pcre.dll:       $(OBJ) pcreposix.o pcre.def
libpcre.dll.a:  pcre.def

pgrep_d:        libpcre.dll.a pgrep.o
		$(CC) $(CFLAGS) -L. -o pgrep pgrep.o -lpcre.dll

pcretest_d:     libpcre.dll.a pcretest.o
		$(PURIFY) $(CC) $(CFLAGS) -L. -o pcretest pcretest.o -lpcre.dll

# End
