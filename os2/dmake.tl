#### Makefile-tail for dmake

.EXPORT: CC O CFLAGS

.c.$O:
 $(CC) $(CFLAGS) -c $<

all: $(EXE) ..\tf-lib\tf-help.idx

usage:
@[
        @echo #### Use "dmake VIDEO=ANSI"    to build ansi-version (default)
        @echo ####                           (scrolling, using os/2-vio-functions,termcap)
        @echo #### Use "dmake VIDEO=TERMCAP" to build termcap-version
        @echo ####                           (no scrolling, using termcap.dat).
 	@echo #### Use "...DEBUG=ON..."      for debugging informations
	@echo #### Use "dmake clean"         for removing object-files
]

$(EXE): $(OBJS) $(BUILDERS)
        $(CC) $(CFLAGS) -o ..\$(EXE) $(OBJS) $(LIBS)

makehelp.exe: makehelp.c
        $(CC) $(CFLAGS) -o makehelp.exe makehelp.c
        makehelp < ..\tf-lib\tf-help > ..\tf-lib\tf-help.idx

..\tf-lib\tf-help.idx: ..\tf-lib\tf-help makehelp.exe

clean:
        -del *.o* *.exe
	-del regexp.o*

cleanest: clean
	-del Makefile config.h

regexp.$O: regexp\regexp.c regexp\regexp.h regexp\regmagic.h config.h
	cd regexp & \
	$(MAKE) -E regexp.$O & \
	copy regexp.$O .. & \
	del regexp.$O & \
	cd ..

#### end of Makefile-tail for dmake 
