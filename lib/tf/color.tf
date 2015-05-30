;;;; popular color definitions
;;; I provided extended ANSI codes because they're the most common.
;;; The user can of course redefine them (in another file).

;; We expect at least one of 0 or 39;49 to turn off color on the vast majority
;; of terminals.  Note 0 also turns off simple attributes.  On terminals where
;; 39;49 does not turn off color, it has no effect (we hope; or if it does,
;; we hope the effect is undone by the 0).
/set end_color  		^[[39;49;0m
;; For white on black terminals where 39;49;0 doesn't reset color
;/set end_color  		^[[37;40m
;; For black on white terminals where 39;49;0 doesn't reset color
; /set end_color  		^[[30;47m


/set start_color_black			^[[30m
/set start_color_red			^[[31m
/set start_color_green			^[[32m
/set start_color_yellow			^[[33m
/set start_color_blue			^[[34m
/set start_color_magenta		^[[35m
/set start_color_cyan			^[[36m
/set start_color_white			^[[37m

/set start_color_bgblack		^[[40m
/set start_color_bgred			^[[41m
/set start_color_bggreen		^[[42m
/set start_color_bgyellow		^[[43m
/set start_color_bgblue			^[[44m
/set start_color_bgmagenta		^[[45m
/set start_color_bgcyan			^[[46m
/set start_color_bgwhite		^[[47m

;; 16 color extension (aixterm, some xterms, ...)
/set start_color_gray			^[[90m
/set start_color_brightred		^[[91m
/set start_color_brightgreen		^[[92m
/set start_color_brightyellow	    	^[[93m
/set start_color_brightblue		^[[94m
/set start_color_brightmagenta	    	^[[95m
/set start_color_brightcyan		^[[96m
/set start_color_brightwhite	    	^[[97m

/set start_color_bggray			^[[100m
/set start_color_bgbrightred		^[[101m
/set start_color_bgbrightgreen	    	^[[102m
/set start_color_bgbrightyellow		^[[103m
/set start_color_bgbrightblue		^[[104m
/set start_color_bgbrightmagenta	^[[105m
/set start_color_bgbrightcyan		^[[106m
/set start_color_bgbrightwhite		^[[107m


;; aixterm "white" appears gray; adding 60 gives true white.
/if ( TERM =~ "aixterm" ) \
    /set start_color_white	^[[97m%; \
    /set start_color_bgwhite	^[[107m%; \
;;  white on black
    /set end_color  		^[[97;40;0m%; \
;;  black on white
;   /set end_color  		^[[30;107;0m%; \
/endif


;; This group is set up for 16 colors on xterms.
;; Colors 0-7 correspond to the 8 named foreground colors above.  The named
;; color variables override the numbered variables below, so to use numbered
;; variables 0-7 you must unset the named variables (or reset them to
;; the codes below).
;
;/set start_color_0		^[[200m
;/set start_color_1		^[[201m
;/set start_color_2		^[[202m
;/set start_color_3		^[[203m
;/set start_color_4		^[[204m
;/set start_color_5		^[[205m
;/set start_color_6		^[[206m
;/set start_color_7		^[[207m
;/set start_color_8		^[[208m
;/set start_color_9		^[[209m
;/set start_color_10		^[[210m
;/set start_color_11		^[[211m
;/set start_color_12		^[[212m
;/set start_color_13		^[[213m
;/set start_color_14		^[[214m
;/set start_color_15		^[[215m


;; Simple commands to disable/enable color.

/purge -i -mregexp ^color_(on|off)$

/def -i color_on = \
    /load -q %TFLIBDIR/color.tf%; \
    /dokey redraw

/def -i color_off = \
    /quote -S /unset `/@listvar -s start_color_*%; \
    /unset end_color%; \
    /dokey redraw


;; 256-color extension for xterm
/if (!features("256colors")) /exit%; /endif

/for red 0 5 \
    /for green 0 5 \
	/for blue 0 5 \
	    /set start_color_rgb%%%{red}%%%{green}%%%{blue}=\
		^[[38;5;$$$[16 + red*36 + green*6 + blue]m%%%; \
	    /set start_color_bgrgb%%%{red}%%%{green}%%%{blue}=\
		^[[48;5;$$$[16 + red*36 + green*6 + blue]m

/for i 0 23 \
    /set start_color_gray%i=^[[38;5;$[232+i]m%; \
    /set start_color_bggray%i=^[[48;5;$[232+i]m

