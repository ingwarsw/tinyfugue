;;; /watch
; Watch for people to connect to a mud.
; Requires that the mud have a WHO command that lists one player per line,
; and OUTPUTPREFIX and OUTPUTSUFFIX commands.
;
; Usage:
; /watch <player>	Tells you when <player> logs on to the mud.
; /watch		Tells you who you are still watching for.
; /unwatch <player>	Stops looking for <player>.
; /unwatch -a		Stops looking for everyone.
;
; This version polls for all names with a single WHO, unlike previous
; versions which did a separate WHO for each name being watched.
;
; Written by David Moore ("OliverJones").


/loaded __TFLIB__/watch.tf
/require pcmd.tf

;;; Global variables:
;/set watch_pid
;/set watch_list
;/set watch_glob

/def -i watch = \
    /let _who=$[tolower(%1)]%;\
    /if (_who =~ "") \
	/echo \% You are watching for: %{_watch_list}%;\
	/break%;\
    /endif%;\
    /if (_who =/ _watch_glob) \
	/echo \% You are already watching for that person!%;\
	/break%;\
    /endif%;\
    /if (_watch_pid =~ "") \
	/repeat -60 1 /_watch%;\
	/set _watch_pid=%?%;\
    /endif%;\
    /set _watch_list=%{_who}|%{_watch_list}%;\
    /set _watch_list=$(/replace || | %{_watch_list})%;\
    /set _watch_glob={%{_watch_list}}

/def -i unwatch =\
    /let _who=$[tolower(%1)]%;\
    /if (_who =~ "") \
	/echo \% Use /unwatch <name> or /unwatch -a for all.%;\
	/break%;\
    /endif%;\
    /if ((_who !~ "-a") & (_who !/ _watch_glob)) \
	/echo \% You already weren't watching for that person!%;\
	/break%;\
    /endif%;\
    /if (_who =~ "-a") \
	/set _watch_list=|%;\
    /else \
	/set _watch_list=$(/replace %{_who}| | %{_watch_list})%;\
	/set _watch_list=$(/replace || | %{_watch_list})%;\
    /endif%;\
    /set _watch_glob={%{_watch_list}}%;\
    /if ((_watch_list =~ "|") & (_watch_pid !~ "")) \
	/kill %{_watch_pid}%;\
	/unset _watch_pid%;\
    /endif

/def -i _watch =\
    /unset _watch_pid%;\
    /def -i -p100 -1 -aGg -msimple -t"%{outputprefix}" _watch_start =\
	/def -i -p100 -aGg -mglob -t"*" _watch_ignore =%%;\
	/def -i -p101 -aGg -mglob -t"%{_watch_glob}*" _watch_match =\
	    /echo # %%%1 has connected.%%%;\
	    /unwatch %%%1%%;\
	/def -i -p101 -1 -aGg -msimple -t"%{outputsuffix}" _watch_end =\
	    /undef _watch_ignore%%%;\
	    /undef _watch_match%%%;\
	    /if (_watch_list !~ "|") \
		/repeat -60 1 /_watch%%%;\
		/set _watch_pid=%%%?%%%;\
	    /endif%;\
    /pcmd WHO


