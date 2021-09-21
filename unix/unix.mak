# $Id: unix.mak,v 35004.50 2007/01/13 23:12:39 kkeys Exp $
########################################################################
#  TinyFugue - programmable mud client
#  Copyright (C) 1994, 1995, 1996, 1997, 1998, 1999, 2002, 2003, 2004, 2005, 2006-2007 Ken Keys
#
#  TinyFugue (aka "tf") is protected under the terms of the GNU
#  General Public License.  See the file "COPYING" for details.
#
#  DO NOT EDIT THIS FILE.
#  To make configuration changes, see "unix/README".
########################################################################

#
# unix section of src/Makefile.
#

SHELL      = /bin/sh
BUILDERS   = Makefile


default: all

install:  _all $(TF) LIBRARY $(MANPAGE) $(SYMLINK)
	@echo
	@echo '#####################################################'
	@echo '## TinyFugue installation successful.'
	@echo '##    tf binary: $(TF)'
	@echo '##    library:   $(TF_LIBDIR)'
#	@echo '##    manpage:   $(MANPAGE)'
	@DIR=`echo $(TF) | sed 's;/[^/]*$$;;'`; \
	echo ":$(PATH):" | egrep ":$${DIR}:" >/dev/null 2>&1 || { \
	    echo "##"; \
	    echo "## Note:  $$DIR is not in your PATH."; \
	    echo "## To run tf, you will need to type its full path name"; \
	    echo "## or add $$DIR to your PATH."; \
	}
	@if test $(TF_LIBDIR) != `cat TF_LIBDIR.build`; then \
	    echo "##"; \
	    echo "## Note:  installed and compiled-in libraries disagree."; \
	    echo "## To run tf, you will need TFLIBDIR=\"$(TF_LIBDIR)\""; \
	    echo "## in your environment or the -L\"$(TF_LIBDIR)\" option."; \
	fi

all files:  _all
	@echo '$(TF_LIBDIR)' > TF_LIBDIR.build
	@echo
	@echo '#####################################################'
	@echo '## TinyFugue build successful.'
	@echo '## Use "$(MAKE) install" to install:'
	@echo '##    tf binary: $(TF)'
	@echo '##    library:   $(TF_LIBDIR)'
#	@echo '##    manpage:   $(MANPAGE)'

_all:  tf$(X) ../lib/tf/tf-help.idx

_failmsg:
#	@echo '#####################################################'
#	@echo '## TinyFugue installation FAILED.'
#	@echo '## See README for help.'
#	@if [ "$(STD_C)" != "1" ]; then \
#	    echo '## '; \
#	    echo '## TF requires a standard (ANSI/ISO 9889-1990) C compiler.'; \
#	    echo '## The standard has existed since 1989, and gcc is freely'; \
#	    echo '## available for many platforms, so there is really no'; \
#	    echo '## excuse for not having a standard compiler at this time.'; \
#	    echo '## If your system does not have one, you should complain'; \
#	    echo '## strongly to the vendor or administrator.  Do not bother'; \
#	    echo '## contacting the author of TF.'; \
#	elif [ "$(CC)" = "gcc" ]; then \
#	    echo '## '; \
#	    echo '## Perhaps $(CC) is not configured correctly.  Before'; \
#	    echo '## contacting the TF author, try setting the environment'; \
#	    echo '## variable CC to "cc", and run ./configure again.'; \
#	fi

TF tf$(X):     $(OBJS) $(BUILDERS)
	$(CC) $(LDFLAGS) -o tf$(X) $(OBJS) $(LIBS)
#	@# Some stupid linkers return ok status even if they fail.
	@test -f "tf$(X)"
#	@# ULTRIX's sh errors here if strip isn't found, despite "true".
	-test -z "$(STRIP)" || $(STRIP) tf$(X) || true

install_TF $(TF): tf$(X) $(BUILDERS)
	install -d -m755 ${DESTDIR}$(bindir)
	install -m755 tf${X} ${DESTDIR}$(TF)

SYMLINK $(SYMLINK): $(TF)
	test -z "$(SYMLINK)" || { rm -f $(SYMLINK) && ln -s $(TF) $(SYMLINK); }

LIBRARY $(TF_LIBDIR): ../lib/tf/tf-help ../lib/tf/tf-help.idx
	install -d -m755 ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/lisp.tf ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/hanoi.tf ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/spell.tf ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/spedwalk.tf ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/kb-emacs.tf ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/tf-help.idx ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/tools.tf ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/watch.tf ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/kb_badterm.tf ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/world-q.tf ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/changes.tf ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/stack-q.tf ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/factoral.tf ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/cylon.tf ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/quoter.tf ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/psh.tf ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/textutil.tf ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/finger.tf ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/savehist.tf ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/textencode.tf ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/spc-page.tf ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/teraterm.keyboard.cnf ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/testcolor.tf ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/tintin.tf ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/grep.tf ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/tfrc ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/rwho.tf ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/alias.tf ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/kbbind.tf ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/complete.tf ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/kbregion.tf ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/color.tf ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/kb-bash.tf ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/kbstack.tf ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/filexfer.tf ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/activity_status.tf ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/tfstatus.tf ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/relog.tf ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/local-eg.tf ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/stdlib.tf ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/at.tf ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/tick.tf ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/kb-os2.tf ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/kbfunc.tf ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/pcmd.tf ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/map.tf ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/activity_status2.tf ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/kb-old.tf ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/tr.tf ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/tf/examples.old ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/py/config.py ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/py/diffedit.py ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/py/tf.py ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/py/tf4.py ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/py/tfutil.py ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../lib/py/urlwatch.py ${DESTDIR}$(TF_LIBDIR)
	install -m644 ../CHANGES ${DESTDIR}$(TF_LIBDIR)

makehelp: makehelp.c
	$(CC) $(CFLAGS) -o makehelp makehelp.c

__always__:

../lib/tf/tf-help: __always__
	if test -d ../help; then cd ../help; $(MAKE) tf-help; fi
	if test -d ../help; then cp ../help/tf-help ../lib/tf; fi

../lib/tf/tf-help.idx: ../lib/tf/tf-help makehelp
	$(MAKE) -f ../unix/unix.mak CC='$(CC)' CFLAGS='$(CFLAGS)' makehelp
	./makehelp < ../lib/tf/tf-help > ../lib/tf/tf-help.idx

MANPAGE $(MANPAGE): $(BUILDERS) tf.1.$(MANTYPE)man
	install -d -m755 ${DESTDIR}${mandir}/man1
	install -m644 tf.1.${MANTYPE}man ${DESTDIR}${mandir}/man1
	cp tf.1.$(MANTYPE)man $(MANPAGE)
	chmod $(MODE) $(MANPAGE)
	chmod ugo-x $(MANPAGE)

Makefile: ../unix/vars.mak ../unix/unix.mak ../configure ../configure.ac
	@echo
	@echo "## WARNING: configuration should be rerun."
	@echo

uninstall:
	@echo "Remove $(TF_LIBDIR) $(TF) $(MANPAGE)"
	@echo "Is this okay? (y/n)"
	@read response; test "$$response" = "y"
	rm -f $(TF) $(MANPAGE)
	rm -rf $(TF_LIBDIR)

clean distclean cleanest:
	cd ..; make -f unix/Makefile $@


# development stuff, not necessarily portable.

tags: *.[ch]
	ctags --excmd=pattern port.h tf.h *.[ch] 2>/dev/null

dep: *.c
	gcc -E -MM *.c \
		| sed 's;pcre[^ ]*/pcre.h ;;' \
		| sed '/[^\\]$$/s/$$/ $$(BUILDERS)/' \
		> dep

tf.pixie: tf$(X)
	pixie -o tf.pixie tf$(X)

lint:
	lint -woff 128 $(CFLAGS) -DHAVE_PROTOTYPES $(SOURCE) $(LIBRARIES)

# The next line is a hack to get around a bug in BSD/386 make.
make:

