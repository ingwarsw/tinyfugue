; Some broken terminal emulators (TeraTerm, NiftyTelnet) send incorrect char
; sequences for the editor keypad.  This file redefines TF's idea of named
; keys to match those broken emulators.
;
; Note, however, that the preferred method of fixing TeraTerm is to copy
; %TFLIBDIR/teraterm.keyboard.cnf to keyboard.cnf in the TeraTerm directory;
; that will fix TeraTerm for all applications that run within TeraTerm, not
; just TF.

/~keyseq insert	^[[1~
/~keyseq delete	^[[4~
/~keyseq home	^[[2~
/~keyseq end	^[[5~
/~keyseq pgup	^[[3~
/~keyseq pgdn	^[[6~

