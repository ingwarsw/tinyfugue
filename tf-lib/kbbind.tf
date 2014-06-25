;;;; Default keybindings
;;;; This file is loaded by stdlib.tf.

/loaded __TFLIB__/kbbind.tf

; ^J, ^M, ^H, and ^? are handled internally.

/def -i ~bind_if_not_bound = \
    /if /!ismacro -msimple -ib'%1'%; /then \
        /def -ib'%1' = %-1%;\
    /endif

/def -i ~keyseq = \
    /let name=%1%; \
    /while /shift%; /test {#} & {1} !/ "#*"%; /do \
	/def -ib'%1' = /key_%name%; \
	/def -ib'^[%1' = /key_esc_%name%; \
    /done

;;; Bind character sequences to the keys that generate them in various common
;;; terminal emulations

; arrows
/~keyseq up		^[[A	^[OA
/~keyseq down		^[[B	^[OB
/~keyseq right		^[[C	^[OC
/~keyseq left		^[[D	^[OD
/~keyseq center		^[[E	^[OE

; arrows with Ctrl, for versions of xterm with modifyCursorKeys
/~keyseq ctrl_up	^[[1;5A
/~keyseq ctrl_down	^[[1;5B
/~keyseq ctrl_right	^[[1;5C
/~keyseq ctrl_left	^[[1;5D

; arrows with Meta, for versions of xterm with modifyCursorKeys
/~keyseq meta_up	^[[1;3A
/~keyseq meta_down	^[[1;3B
/~keyseq meta_right	^[[1;3C
/~keyseq meta_left	^[[1;3D

; arrows with Shift, for versions of xterm with modifyCursorKeys
/~keyseq shift_up	^[[1;2A
/~keyseq shift_down	^[[1;2B
/~keyseq shift_right	^[[1;2C
/~keyseq shift_left	^[[1;2D

; Some broken terminal emulators (TeraTerm, NiftyTelnet) send incorrect
; char sequences for the editor keypad (the 6 keys above the arrow keys).
; We can't cater to them without breaking keys for users with correct
; terminal emulators.
;
; TeraTerm users should fix their emulators by copying
; %TFLIBDIR/teraterm.keyboard.cnf to keyboard.cnf in their TeraTerm
; directory.  Users of either emulator can work around the problem with
; "/load kb_badterm.tf".

; Editor Keypad
/~keyseq insert		^[[2~		^[[L
/~keyseq delete		^[[3~
/~keyseq home		^[[1~	^[OH	^[[H
/~keyseq end		^[[4~	^[OF	^[[F
/~keyseq pgup		^[[5~
/~keyseq pgdn		^[[6~

; Editor Keypad with Ctrl, for versions of xterm with modifyCursorKeys
/~keyseq ctrl_insert	^[[2;5~
/~keyseq ctrl_delete	^[[3;5~
/~keyseq ctrl_home	^[[1;5~	^[[1;5H
/~keyseq ctrl_end	^[[4;5~	^[[1;5F
/~keyseq ctrl_pgup	^[[5;5~
/~keyseq ctrl_pgdn	^[[6;5~

; Editor Keypad with Meta, for versions of xterm with modifyCursorKeys
/~keyseq meta_insert	^[[2;3~
/~keyseq meta_delete	^[[3;3~
/~keyseq meta_home	^[[1;3~	^[[1;3H
/~keyseq meta_end	^[[4;3~	^[[1;3F
/~keyseq meta_pgup	^[[5;3~
/~keyseq meta_pgdn	^[[6;3~

; Editor Keypad with Shift, for versions of xterm with modifyCursorKeys
; Note, xterm by default traps shift-{insert,pgup,pgdn} itself, so won't
; pass them to tf.
/~keyseq shift_insert	^[[2;2~
/~keyseq shift_delete	^[[3;2~
/~keyseq shift_home	^[[1;2~	^[[1;2H
/~keyseq shift_end	^[[4;2~	^[[1;2F
/~keyseq shift_pgup	^[[5;2~
/~keyseq shift_pgdn	^[[6;2~

;; Several common configurations of numeric keypad:
;
;       config 1 (PC)                 config 2 (PC)
;      +-----+-----+-----+-----+     +-----+-----+-----+-----+
;      |     |^[Oo |^[Oj |^[Om |     |     |^[Oo |^[Oj |^[Om |
;      +-----+-----+-----+-----+     +-----+-----+-----+-----+
;      |^[Ow |^[Ox |^[Oy |^[Ok |     |^[OH |^[OA |^[[5~|^[Ok |
;      +-----+-----+-----+     +     +-----+-----+-----+     +
;      |^[Ot |^[Ou |^[Ov |     |     |^[OD |^[OE |^[OC |     |
;      +-----+-----+-----+-----+     +-----+-----+-----+-----+
;      |^[Oq |^[Or |^[Os |^[OM |     |^[OF |^[OB |^[[6~|^[OM |
;      +-----+-----+-----+     +     +-----+-----+-----+     +
;      |^[Op       |^[On |     |     |^[Op       |^[On |     |
;      +-----+-----+-----+-----+     +-----+-----+-----+-----+
;
;       config 3 (PC)                 config 4 (PC)
;      +-----+-----+-----+-----+     +-----+-----+-----+-----+
;      |     |/    |*    |-    |     |     |/    |*    |-    |
;      +-----+-----+-----+-----+     +-----+-----+-----+-----+
;      |7    |8    |9    |+    |     |^[[H |^[[A |^[[5~|+    |
;      +-----+-----+-----+     +     +-----+-----+-----+     +
;      |4    |5    |6    |     |     |^[[D |^[[E |^[[C |     |
;      +-----+-----+-----+-----+     +-----+-----+-----+-----+
;      |1    |2    |3    |^M   |     |^[[F |^[[B |^[[6~|^M   |
;      +-----+-----+-----+     +     +-----+-----+-----+     +
;      |0          |.    |     |     |^[[2~      |^[[3~|     |
;      +-----+-----+-----+-----+     +-----+-----+-----+-----+
;
;       config 5 (Mac)                config 6 (Mac)
;      +-----+-----+-----+-----+     +-----+-----+-----+-----+
;      |     |=    |/    |*    |     |     |     |     |     |
;      +-----+-----+-----+-----+     +-----+-----+-----+-----+
;      |7    |8    |9    |-    |     |^[Ow |^[Ox |^[Oy |^[Om |
;      +-----+-----+-----+-----+     +-----+-----+-----+-----+
;      |4    |5    |6    |+    |     |^[Ot |^[Ou |^[Ov |^[Ol |
;      +-----+-----+-----+-----+     +-----+-----+-----+-----+
;      |1    |2    |3    |^M   |     |^[Oq |^[Or |^[Os |^[OM |
;      +-----+-----+-----+     +     +-----+-----+-----+     +
;      |0          |.    |     |     |^[Op       |^[On |     |
;      +-----+-----+-----+-----+     +-----+-----+-----+-----+
;
;; XFree86/XOrg xterm with "alt/numlock modifiers" off, and X Consortium xterm
; %keypad on,  numlock on:   config 1
; %keypad on,  numlock off:  config 2
; %keypad off, numlock on:   config 3
; %keypad off, numlock off:  config 4
;
;; XFree86/XOrg xterm with "alt/numlock modifiers" on (the default):
; %keypad on,  numlock on:   config 3
; %keypad on,  numlock off:  config 2
; %keypad off, numlock on:   config 3
; %keypad off, numlock off:  config 4
;
;; XFree86/XOrg xterm with "VT220 keyboard" on:
; %keypad on,  numlock on:   config 3
; %keypad on,  numlock off:  config 1
; %keypad off, numlock on:   config 3
; %keypad off, numlock off:  config 3
;
;; PuTTY 0.56:
; %keypad on,  numlock on:   config 1
; %keypad on,  numlock off:  similar to config 4
; %keypad off, numlock on:   config 3
; %keypad off, numlock off:  similar to config 4
;
;; Mac OSX Terminal, "strict vt100 keypad behavior" off (the default):
; %keypad on:   config 5
; %keypad off:  config 5
;; Mac OSX Terminal, "strict vt100 keypad behavior" on:
; %keypad on:   config 6
; %keypad off:  config 5

; Numeric Keypad
/~keyseq nkpTab		^[OI
/~keyseq nkpEnt		^[OM
/~keyseq nkp*		^[Oj
/~keyseq nkp+		^[Ok
/~keyseq nkp,		^[Ol
/~keyseq nkp-		^[Om
/~keyseq nkp.		^[On
/~keyseq nkp/		^[Oo
/~keyseq nkp0		^[Op
/~keyseq nkp1		^[Oq
/~keyseq nkp2		^[Or
/~keyseq nkp3		^[Os
/~keyseq nkp4		^[Ot
/~keyseq nkp5		^[Ou
/~keyseq nkp6		^[Ov
/~keyseq nkp7		^[Ow
/~keyseq nkp8		^[Ox
/~keyseq nkp9		^[Oy
/~keyseq nkp=		^[OX

; Function Keys		vt100/vt220/xterm	sun
/~keyseq f1		^[[11~	^[OP		^[[224z
/~keyseq f2		^[[12~	^[OQ		^[[225z
/~keyseq f3		^[[13~	^[OR		^[[226z
/~keyseq f4		^[[14~	^[OS		^[[227z
/~keyseq f5		^[[15~			^[[228z
/~keyseq f6		^[[17~			^[[229z
/~keyseq f7		^[[18~			^[[230z
/~keyseq f8		^[[19~			^[[231z
/~keyseq f9		^[[20~			^[[232z
/~keyseq f10		^[[21~			^[[233z
/~keyseq f11		^[[23~			^[[192z
/~keyseq f12		^[[24~			^[[193z
/~keyseq f13		^[[25~
/~keyseq f14		^[[26~
/~keyseq f15		^[[28~
/~keyseq f16		^[[29~
/~keyseq f17		^[[31~
/~keyseq f18		^[[32~
/~keyseq f19		^[[33~
/~keyseq f20		^[[34~

; Function with Ctrl, for some versions of xterm
/~keyseq ctrl_f1	^[[11;5~	^[O5P
/~keyseq ctrl_f2	^[[12;5~	^[O5Q
/~keyseq ctrl_f3	^[[13;5~	^[O5R
/~keyseq ctrl_f4	^[[14;5~	^[O5S
/~keyseq ctrl_f5	^[[15;5~
/~keyseq ctrl_f6	^[[17;5~
/~keyseq ctrl_f7	^[[18;5~
/~keyseq ctrl_f8	^[[19;5~
/~keyseq ctrl_f9	^[[20;5~
/~keyseq ctrl_f10	^[[21;5~
/~keyseq ctrl_f11	^[[23;5~
/~keyseq ctrl_f12	^[[24;5~
/~keyseq ctrl_f13	^[[25;5~
/~keyseq ctrl_f14	^[[26;5~
/~keyseq ctrl_f15	^[[28;5~
/~keyseq ctrl_f16	^[[29;5~
/~keyseq ctrl_f17	^[[31;5~
/~keyseq ctrl_f18	^[[32;5~
/~keyseq ctrl_f19	^[[33;5~
/~keyseq ctrl_f20	^[[34;5~

; Function with Meta, for some versions of xterm
/~keyseq meta_f1	^[[11;3~	^[O3P
/~keyseq meta_f2	^[[12;3~	^[O3Q
/~keyseq meta_f3	^[[13;3~	^[O3R
/~keyseq meta_f4	^[[14;3~	^[O3S
/~keyseq meta_f5	^[[15;3~
/~keyseq meta_f6	^[[17;3~
/~keyseq meta_f7	^[[18;3~
/~keyseq meta_f8	^[[19;3~
/~keyseq meta_f9	^[[20;3~
/~keyseq meta_f10	^[[21;3~
/~keyseq meta_f11	^[[23;3~
/~keyseq meta_f12	^[[24;3~
/~keyseq meta_f13	^[[25;3~
/~keyseq meta_f14	^[[26;3~
/~keyseq meta_f15	^[[28;3~
/~keyseq meta_f16	^[[29;3~
/~keyseq meta_f17	^[[31;3~
/~keyseq meta_f18	^[[32;3~
/~keyseq meta_f19	^[[33;3~
/~keyseq meta_f20	^[[34;3~

; Function with Shift, for some versions of xterm
/~keyseq shift_f1	^[[11;2~	^[O2P
/~keyseq shift_f2	^[[12;2~	^[O2Q
/~keyseq shift_f3	^[[13;2~	^[O2R
/~keyseq shift_f4	^[[14;2~	^[O2S
/~keyseq shift_f5	^[[15;2~
/~keyseq shift_f6	^[[17;2~
/~keyseq shift_f7	^[[18;2~
/~keyseq shift_f8	^[[19;2~
/~keyseq shift_f9	^[[20;2~
/~keyseq shift_f10	^[[21;2~
/~keyseq shift_f11	^[[23;2~
/~keyseq shift_f12	^[[24;2~
/~keyseq shift_f13	^[[25;2~
/~keyseq shift_f14	^[[26;2~
/~keyseq shift_f15	^[[28;2~
/~keyseq shift_f16	^[[29;2~
/~keyseq shift_f17	^[[31;2~
/~keyseq shift_f18	^[[32;2~
/~keyseq shift_f19	^[[33;2~
/~keyseq shift_f20	^[[34;2~

; other
/~keyseq tab		^I
/~keyseq shift_tab	^[[Z		# on some terminals

;;; Named keys with character sequences defined in termcap/terminfo.
; Many of these will redefine hardcoded sequences above
/def -i -ag -hREDEF ~gag_redef
/set warn_def_B=0
/def -i ~keyname = /if (keycode({1}) !~ "") /def -iB'%{1}' = /key_%2%; /endif

; termcap name		tf name
/~keyname Up		up
/~keyname Down		down
/~keyname Right		right
/~keyname Left		left

;/~keyname Backspace	backspace
/~keyname Insert	insert
/~keyname Delete	delete
/~keyname Home		home
/~keyname PgDn		pgdn
/~keyname PgUp		pgup

/~keyname F1		f1
/~keyname F2		f2
/~keyname F3		f3
/~keyname F4		f4
/~keyname F5		f5
/~keyname F6		f6
/~keyname F7		f7
/~keyname F8		f8
/~keyname F9		f9
/~keyname F10		f10
/~keyname F11		f11
/~keyname F12		f12
/~keyname F13		f13
/~keyname F14		f14
/~keyname F15		f15
/~keyname F16		f16
/~keyname F17		f17
/~keyname F18		f18
/~keyname F19		f19

;; Termcap definitions of these keys are questionable.  The hardcoded defaults
;; defined elsewhere are correct for vt100/xterm, so it's best to omit these.
; /~keyname KP1		nkp1
; /~keyname KP2		nkp2
; /~keyname KP3		nkp3
; /~keyname KP4		nkp0
; /~keyname KP5		nkp.

/set warn_def_B=1
/undef ~gag_redef



;;; Define each named key with the function it performs.

/def -i key_up		= /kb_up_or_recallb
/def -i key_down	= /kb_down_or_recallf
/def -i key_right	= /dokey_right
/def -i key_left	= /dokey_left

/def -i key_ctrl_up	= /dokey_recallb
/def -i key_ctrl_down	= /dokey_recallf
/def -i key_ctrl_right	= /dokey_wright
/def -i key_ctrl_left	= /dokey_wleft

/def -i key_f1		= /help

/def -i key_insert	= /@test insert := !insert
/def -i key_delete	= /dokey_dch
/def -i key_home	= /dokey_home
/def -i key_end		= /dokey_end
/def -i key_pgup	= /dokey_pgup
/def -i key_pgdn	= /dokey_pgdn

/def -i key_ctrl_home	= /dokey_recallbeg
/def -i key_ctrl_end	= /dokey_recallend
/def -i key_ctrl_pgup	= /reserved_for_future_use (scroll to first screenful)
/def -i key_ctrl_pgdn	= /dokey_flush

/def -i key_bspc	= /dokey_bspc
/def -i key_tab		= /dokey page

/def -i key_esc_left	= /dokey_socketb
/def -i key_esc_right	= /dokey_socketf
/def -i key_esc_delete	= /kb_backward_kill_word

;; make meta_<namedkey> act like esc_<namedkey>
/def -i key_meta_insert	= /key_esc_insert
/def -i key_meta_delete	= /key_esc_delete
/def -i key_meta_home	= /key_esc_home
/def -i key_meta_end	= /key_esc_end
/def -i key_meta_pgup	= /key_esc_pgup
/def -i key_meta_pgdn	= /key_esc_pgdn
/def -i key_meta_up	= /key_esc_up
/def -i key_meta_down	= /key_esc_down
/def -i key_meta_left	= /key_esc_left
/def -i key_meta_right	= /key_esc_right
/def -i key_meta_f1	= /key_esc_f1
/def -i key_meta_f2	= /key_esc_f2
/def -i key_meta_f3	= /key_esc_f3
/def -i key_meta_f4	= /key_esc_f4
/def -i key_meta_f5	= /key_esc_f5
/def -i key_meta_f6	= /key_esc_f6
/def -i key_meta_f7	= /key_esc_f7
/def -i key_meta_f8	= /key_esc_f8
/def -i key_meta_f9	= /key_esc_f9
/def -i key_meta_f10	= /key_esc_f10
/def -i key_meta_f11	= /key_esc_f11
/def -i key_meta_f12	= /key_esc_f12

;; bindings to cycle through sockets that don't depend on working arrow keys
/def -ib'^[{'	= /dokey_socketb
/def -ib'^[}'	= /dokey_socketf

;; kbnum
/def -ib'^[-'	= /set kbnum=-
/def -ib'^[0'	= /set kbnum=+
/def -ib'^[1'	= /set kbnum=+1
/def -ib'^[2'	= /set kbnum=+2
/def -ib'^[3'	= /set kbnum=+3
/def -ib'^[4'	= /set kbnum=+4
/def -ib'^[5'	= /set kbnum=+5
/def -ib'^[6'	= /set kbnum=+6
/def -ib'^[7'	= /set kbnum=+7
/def -ib'^[8'	= /set kbnum=+8
/def -ib'^[9'	= /set kbnum=+9

;;; Other useful bindings
;;; /~bind_if_not_bound is used for keys that may have been bound internally
;;; by copying from the terminal driver.

/def -i kbwarn = \
    /if /test warn_5keys & !kbwarned_%1%; /then \
	/echo -aW %% Warning: keys have changed in version 5.0 to be more like \
	    emacs and many other unix apps.  %-1  This warning can be disabled \
	    with "/set warn_5keys=off".%; \
	/set kbwarned_%1=1%; \
    /endif

/def -ib'^A'	= /dokey_home
/def -ib"^B"	= /kbwarn char ^B/^F now move the cursor by a letter; \
		    use ESC b/ESC f to move by a word.%; \
		    /dokey_left
/def -ib'^D'	= /dokey_dch
/def -ib'^E'	= /dokey_end
/def -ib"^F"	= /kbwarn char ^B/^F now move the cursor by a letter; \
		    use ESC b/ESC f to move by a word.%; \
		    /dokey_right
; note ^G does NOT honor kbnum, so it can be used to cancel kbnum entry.
/def -ib'^G'	= /beep
;def -ib"^H"	= /dokey_bspc			; internal
;def -ib'^I'	= /dokey page			; /key_tab
;def -ib"^J"	= /dokey newline		; internal
/def -ib'^K'	= /dokey_deol
/def -ib'^L'	= /dokey redraw
;def -ib"^M"	= /dokey newline		; internal
/def -ib'^N'	= /dokey recallf
;def -ib"^O"	= /operate-and-get-next		; not implemented
/def -ib'^P'	= /dokey recallb
/def -ib"^Q"	= /dokey lnext
/~bind_if_not_bound ^R	/dokey refresh
;def -ib"^R"	= /dokey searchb		; conflict
;def -ib"^S"	= /dokey searchf		; conflict
/def -ib"^S"	= /dokey pause
/def -ib'^T'	= /kb_transpose_chars
/def -ib'^U'	= /kb_backward_kill_line
/~bind_if_not_bound ^V	/dokey lnext
/~bind_if_not_bound ^W	/dokey_bword
/def -ib"^X^R"	= /load ~/.tfrc
/def -ib"^X^V"	= /version
/def -ib"^X^?"	= /kb_backward_kill_line

/def -ib'^X['	= /dokey_hpageback
/def -ib'^X]'	= /dokey_hpage
/def -ib'^X{'	= /dokey_pageback
/def -ib'^X}'	= /dokey_page

;def -ib"^?"	= /dokey_bspc			; internal

/def -ib'^[^E'	= /kb_expand_line
/def -ib'^[^H'	= /kb_backward_kill_word
/def -ib'^[^L'	= /dokey clear
/def -ib'^[^N'	= /dokey line
/def -ib'^[^P'	= /dokey lineback
;def -ib"^[!"	= /complete command		; complete.tf
;def -ib"^[%"	= /complete variable		; complete.tf
/def -ib'^[ '	= /kb_collapse_space
/def -ib'^[='	= /kb_goto_match
/def -ib'^[.'	= /kb_last_argument
;def -ib"^[/"	= /complete filename		; complete.tf
/def -ib'^[<'	= /dokey recallbeg
/def -ib'^[>'	= /dokey recallend
;def -ib"^[?"	= /possible-completions		; complete.tf
;def -ib"^[@"	= /complete hostname		; complete.tf
/def -ib'^[J'	= /dokey selflush
/def -ib'^[L'	= /kb_toggle_limit
/def -ib'^[_'	= /kb_last_argument
/def -ib"^[b"	= /kbwarn word ESC b/ESC f now move the cursor by a word; \
		    use ESC LEFT/ESC RIGHT to switch worlds.%; \
		    /dokey_wleft
/def -ib"^[c"	= /kb_capitalize_word
/def -ib"^[d"	= /kb_kill_word
/def -ib"^[f"	= /kbwarn word ESC b/ESC f now move the cursor by a word; \
		    use ESC LEFT/ESC RIGHT to switch worlds.%; \
		    /dokey_wright
/def -ib'^[h'	= /dokey_hpage
/def -ib'^[j'	= /dokey flush
/def -ib'^[l'	= /kb_downcase_word
/def -ib'^[n'	= /dokey searchf
/def -ib'^[p'	= /dokey searchb
/def -ib'^[u'	= /kb_upcase_word
/def -ib'^[v'	= /@test insert := !insert
;def -ib"^[~"	= /complete username		; complete.tf
/def -ib'^[^?'	= /kb_backward_kill_word

/def -ib'^]'	= /bg


;;;; other necessary libraries
/require kbfunc.tf
/require complete.tf

