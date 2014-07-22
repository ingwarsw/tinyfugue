;;;; Tintin emulation macros.
;;; If you're converting to tf from tintin, you might find this useful.
;;; This file emulates most of the commands in tintin++, although some
;;; commands will behave somewhat differently.
;;; These commands have not been fully tested.


/loaded __TFLIB__/tintin.tf

;;; Some useful stuff is stored in other files.
/require tick.tf
/require alias.tf
/require spedwalk.tf
/require map.tf

/def -i action	= /trig %*
;alias		(see alias.tf)
/def -i antisubstitute = /def -p9999 -t'$(/escape ' %*)'
/def -i all	= /send -W -- %*
/def -i bell	= /beep %*
/def -i boss	= /echo -e %0: Not implemented.
/def -i char	= /echo -e %0: Not implemented.
;/def echo	= /toggle mecho%; /: The name "echo" conflicts with standard tf.
/def -i end	= /quit
;gag		builtin
;help		builtin
/def -i highlight	= /hilite %*
/def -i history	= /recall -i %{1-15}
;if		builtin
/def -i ignore	= /toggle borg%; /set borg
;log		builtin
;loop		(see /for)
;map		(see map.tf)
;mark		(see map.tf)
/def -i math	= /@test %1 := %-1
/def -i message	= /echo -e %0: Not implemented; use hooks with gags.
/def -i -mregexp -p2 -h'send ^#([0-9]+) ' rep_hook = /repeat -S %P1 %PR
/def -i nop	= /@test 1
;path		(see map.tf)
/def -i presub =/echo -e %0: Use the -F flag in triggers that call /substitute.
;redraw		not needed (always on)
;return		(see map.tf)
/def -i read	= /load %*
;savepath	(see map.tf)

/def -i session	= \
	 	/if ({#}) \
			/if /addworld %*%; /then /world %1%; /endif%;\
		/else \
			/listsockets%;\
		/endif%;\
		/def %1 = \
			/if ({#}) \
				/send -w%1 %%*%;\
			/else \
				/world %1%;\
			/endif

/def -i showme	= /echo %*
/def -i snoop	= \
		/if /ismacro _snoop_%1%; /then \
			/echo %% Snooping %1 disabled.%;\
			/undef _snoop_%1%;\
			/undef _snoopbg_%1%;\
		/else \
			/echo %% Snooping %1 enabled.%;\
			/def -i -w%1 -hbackground -ag _snoopbg_%1%;\
			/def -i -t* -p9999 -w%1 -ag -F _snoop_%1 = \
				/echo $${world_name}: %%*%;\
		/endif

;speedwalk	(see spedwalk.tf)
/def -i split	= /isize %{1-3}%; /visual on
;subs		(see /substitute)
;substitute	(see /substitute)
/def -i system	= /sh %*
;togglesubs	(no equiv)
/def -i unaction	= /untrig %*
;unalias	(see alias.tf)
;unantisub	(no equiv)
/def -i ungag	= /nogag %*
/def -i unhighlight	= /nohilite %*
;unpath		(see map.tf)
/def -i unsplit	= /visual off
;unsubs		(no equiv)
/def -i unvariable	= /unset %*
/def -i variable	= /set %*
/def -i verbatim	= /toggle sub
;version	builtin
/def -i wizlist	= /help author
/def -i write	= /if ( {#} == 1 ) \
			/save %1%;\
		/else \
			/echo -e %% usage: /%0 <filename>%;\
		/endif

/def -i zap	= /dc %*

