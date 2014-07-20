;;;; Speedwalk
;;;; "/speedwalk" toggles speedwalking.  With speedwalking enabled, you can
;;;; type multiple directions on a single line, similar to tintin.  Any line
;;;; containing only numbers and the letters "n", "s", "e", "w", "u", and
;;;; "d" are interpreted by speedwalk; other lines are left alone (of course,
;;;; to guarantee that lines don't get interpreted, you should turn speedwalk
;;;; off).  Each letter is sent individually; if it is preceeded by a number,
;;;; it will be repeated that many times.  For example, with speedwalk
;;;; enabled, typing "ne3ses" will send "n", "e", "s", "s", "s", "e", "s".


/loaded __TFLIB__/spedwalk.tf

/eval \
    /def -i speedwalk = \
        /if /ismacro ~speedwalk%%; /then \
            /echo -e %%% Speedwalk disabled.%%;\
            /undef ~speedwalk%%;\
        /else \
            /echo -e %%% Speedwalk enabled.%%;\
;           NOT fallthru, so _map_send in map.tf won't catch it too.
            /def -ip%{maxpri} -mregexp -h'send ^([0-9]*[nsewud])+$$$' \
	      ~speedwalk = \
                /~do_speedwalk %%%*%%;\
        /endif

/def -i ~do_speedwalk = \
    /let _args=%*%;\
    /let _string=%;\
    /let _count=%;\
    /let _c=%;\
    /let _i=-1%;\
    /while ( (_c:=substr(_args, ++_i, 1)) !~ "" ) \
        /if ( _c =/ "[0-9]" ) \
            /@test _count:= strcat(_count, _c)%;\
        /elseif ( regmatch("[nsewud]", _c) ) \
            /if ( _string !~ "" ) /send - %{_string}%; /endif%;\
            /let _string=%;\
            /for _j 1 %{_count-1} /~do_speedwalk_aux %{_c}%;\
            /let _count=%;\
        /else \
            /@test _string:= strcat(_string, _count, _c)%;\
            /let _count=%;\
        /endif%;\
    /done%;\
    /let _string=%{_string}%{_count}%;\
    /if ( _string !~ "" ) /send - %{_string}%; /endif

/def -i ~do_speedwalk_aux = \
;   _map_hook may be defined if map.tf was loaded.
    /if /ismacro _map_hook%; /then \
        /_map_hook %*%;\
    /endif%;\
    /send %*
