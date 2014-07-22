;;;; Tick counting
;;;; This file implements several tick counting commands similar to those
;;;; found in tintin, useful on Diku muds.  To use, just /load this file.

;;;; usage:
;;; /tick		Display the time remaining until next tick.
;;; /tickon		Reset and start the tick counter.
;;; /tickoff		Stop the tick counter.
;;; /tickset		Reset and start the tick counter.
;;; /ticksize <n>	Set the tick length to <n> seconds (default is 75).

/loaded __TFLIB__/tick.tf

/set ticksize=75
/set next_tick=0
/set _tick_pid1=0
/set _tick_pid2=0

/def -i tick_warn = /echo % Next tick in 10 seconds.
/def -i tick_action = /echo % TICK

/def -i tick = \
	/if (next_tick) \
		/eval /echo %%% $$[next_tick - $(/time @)] seconds until tick%;\
	/else \
		/echo -e %% Tick counter is not running.%;\
	/endif

/def -i tickon = \
	/tickoff%;\
	/@test next_tick := $(/time @) + ticksize %;\
	/repeat -$[ticksize - 10] 1 \
		/set _tick_pid1=0%%;\
		/tick_warn%;\
        /set _tick_pid1=%?%;\
	/repeat -%ticksize 1 \
		/set _tick_pid2=0%%;\
		/tick_action%%;\
		/tickon%;\
	/set _tick_pid2=%?

/def -i tickoff = \
	/@test _tick_pid1 & (kill(_tick_pid1), _tick_pid1:=0)%;\
	/@test _tick_pid2 & (kill(_tick_pid2), _tick_pid2:=0)%;\
	/set next_tick=0

/def -i tickset	= /tickon %*

/def -i ticksize	= /set ticksize %*
