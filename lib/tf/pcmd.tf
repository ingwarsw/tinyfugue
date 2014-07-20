;;; prefix/suffix for mud commands
;;; from tf 5.0 alpha 9

;; usage:
;; /pfxgen			    - generate *fixes
;; /pfxon [[-w]<world>]		    - enable *fixes on <world>
;; /pfxoff [[-w]<world>]	    - disable *fixes on <world>
;; /pcmd [-w<world>] <cmd>	    - execute *fixed <cmd> on <world>
;; /test pfcmd(<cmd>[, <world>])    - execute *fixed <cmd> on <world>

;; It is okay to issue multiple /pcmd commands without worrying that their
;; triggers will interfere with each other, because a unique prefix and
;; suffix is generated each time.

;; Example: /silent_foobar executes the command "foobar" on the mud, and gags
;; all output of the command if the command works, but lets the "foobar failed"
;; message through if if fails.  Either way, when the command is done,
;; the triggers are cleaned up.
;;
;; /def silent_foobar =\
;;   /def -1 -ag -p5009 -t"%{outputprefix}" =\
;;     /def -p5001 -t"foobar failed" foobar_fail%%;\
;;     /def -ag -p5000 -t"*" foobar_gag%;\
;;   /def -1 -ag -p5009 -t"%{outputsuffix}" =\
;;     /undef foobar_gag%%;\
;;     /undef foobar_fail%;\
;;   /pcmd foobar %1


/loaded __TFLIB__/pcmd.tf

/def -i pfxgen = \
    /set _pfx_counter=$[_pfx_counter + 1]%; \
    /set outputprefix=<pre:%{_pfx_counter}:$[rand()]>%;\
    /set outputsuffix=<suf:%{_pfx_counter}:$[rand()]>

/pfxgen

/def -i pfxon = \
    /if ({*} =/ "-*") /if (!getopts("w:", "")) /return 0%; /endif%; /else /let opt_w=%*%; /endif%; \
    /test send('OUTPUTPREFIX %{outputprefix}', opt_w) & \
	  send('OUTPUTSUFFIX %{outputsuffix}', opt_w)

/def -i pfxoff = \
    /if ({*} =/ "-*") /if (!getopts("w:", "")) /return 0%; /endif%; /else /let opt_w=%*%; /endif%; \
    /test send('OUTPUTPREFIX', opt_w) & \
	  send('OUTPUTSUFFIX', opt_w)

/def -i pfcmd = \
    /let result=$[pfxon({2}) & send({1}, {2}) & pfxoff({2})]%; \
    /pfxgen%; \
    /return result

/def -i pcmd = \
    /if (!getopts("w:", "")) /return 0%; /endif%; \
    /return pfcmd({*}, opt_w)

