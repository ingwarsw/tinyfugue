/* This is a Rexx-Script, so don't remove this comment-line */

/* get the command-line options */
PARSE ARG alloptions

/* get the extra functions */
CALL RxFuncAdd "SysSearchPath","RexxUtil","SysSearchPath"

/* check environment for MAKE */
mymake = Value('MAKE',,'OS2ENVIRONMENT')

/* if not set, try to find another make */
IF Length(mymake) = 0 THEN mymake = SysSearchPath('PATH','dmake.exe')
IF Length(mymake) = 0 THEN mymake = SysSearchPath('PATH','nmake.exe')
IF Length(mymake) = 0 THEN mymake = SysSearchPath('PATH','make.exe')

IF Length(mymake) = 0 THEN
DO
   SAY "os2make-error: Could not find a correct make-program in PATH."
   SAY "               Set the environment-variable MAKE to the full"
   SAY "               pathname of your make-program or put your make-"
   SAY "               program in your PATH."
   SAY 
   SAY "Make-Programs you can use: dmake.exe (gnu-version)"
   SAY "                           nmake.exe (OS/2 Toolkit)"
   SAY "                           make.exe  (GnuMake)"
   SAY 
   SAY "Maybe you can use Borland's make.exe by renaming it to nmake.exe."
  exit
END

CALL Value 'MAKE',mymake,'OS2ENVIRONMENT'

stem = FileSpec("name",mymake)           /* remove the path */
IF LastPos('.',stem) > 0 THEN            /* seems to be something like ABC.exe */
   stem = Left(stem,LastPos('.',stem)-1) /* remove the .exe */

/* create Makefile */
'copy os2\'stem'.hd+src\vars.mak+os2\'stem'.tl+src\rules.mak src\Makefile'

/* copy changes to libdir */
'copy changes tf-lib\changes'

/* create config-file */
'copy os2\config.h src'

'echo #define LIBDIR "'||Translate(Directory(),'/','\')||'/tf-lib" >>src\config.h'

CALL Directory 'src'
mymake' 'alloptions
