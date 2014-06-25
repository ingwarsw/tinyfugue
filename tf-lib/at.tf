;;; AT - run a command at a given time
;; syntax:  /at [-v] [<date> ]<time> <commands>
;; <commands> will be executed at <date> <time>.

/loaded __TFLIB__/at.tf

/def -i at = \
    /if (!getopts("v", "")) /return 0%; /endif%; \
    /let warn=%warn_curly_re%; \
    /set warn_curly_re=off%; \
    /if (regmatch("^(?:(\\d{2})(\\d{2})?-)?(\\d{1,2})[-/](\\d{1,2})$", {1})) \
	/let explicityear=$[!!{P2}]%; \
	/let explicitday=1%; \
	/let year=%{P1-$[ftime("%Y")/100]}%{P2-$[ftime("%y")]}%; \
	/let mon=%P3%; \
	/let day=%P4%; \
	/shift%; \
    /else \
	/let explicityear=0%; \
	/let explicitday=0%; \
	/let year=$[ftime("%Y")]%; \
	/let mon=$[ftime("%m")]%; \
	/let day=$[ftime("%d")]%; \
    /endif%; \
    /if (!regmatch("^(\\d{1,2}):(\\d{2})(?::(\\d{2}(?:\\.(\\d{0,6}))?))?$", {1})) \
	/echo -e %% Usage: /%0 [-v] [[yyyy-]mm-dd] hh:mm[:ss[.uuuuuu]] command%; \
	/set warn_curly_re=%warn%; \
	/return 0%; \
    /endif%; \
    /set warn_curly_re=%warn%; \
    /shift%; \
    /let usec=$[substr(strcat({P4}, "000000"), 0, 6)]%; \
    /let t=$[mktime(year, mon, day, {P1}, {P2}, {P3}, usec)]%; \
    /if (t <= time()) \
	/if (!explicitday) \
	    /test t += 24:00:00%; \
	/elseif (!explicityear) \
	    /let t=$[mktime(year+1, mon, day, {P1}, {P2}, {P3}, usec)]%; \
	/else \
	    /echo -e %% /%0: date and time must be in the future.%; \
	    /return 0%; \
	/endif%; \
    /endif%; \
    /if (opt_v) /echo -e %% /%0: $[ftime("%F %T.%.", t)]%; /endif%; \
    /repeat -$[t-time()] 1 %*

