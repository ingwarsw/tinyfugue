;;;; Mapping.
;;;; This file implements mapping and movement ("speedwalking") commands
;;;; similar to those found in tintin.  Once mapping is enabled with /mark,
;;;; all movement commands (n,s,e,w,ne,sw,nw,se,u,d) will be remembered in
;;;; your "path".

;;;; usage:
;;; /map <dir>		Add <dir> to remembered path.
;;; /mark		Reset path and enable mapping.
;;; /path		Display remembered path.
;;; /revert		Move in the opposite direction of the last remembered
;;;			  movement, and remove that last movement from the path.
;;; /savepath <name>	Create a macro <name> to execute the current path.
;;;			  Note: macro is not written to a file.
;;; /unpath		Remove the last movement from the path.
;;; /unmark		Disable maping.
;;; /dopath <path>	Execute <path>, where <path> is a space-separated list
;;;			  of commands with optional repeat counts.  E.g.,
;;;			  "/dopath 10 n 3 e d 2 w" will execute "n" 10
;;;			  times, "e" 3 times, "d" once, and "w" twice.

/loaded __TFLIB__/map.tf

/set path=

/def -i mark = \
	/echo -e %% Will start mapping here.%;\
	/set path=%;\
;       note: _map_hook can also be called from speedwalk.tf.
	/def -iFp9999 -mglob -h'send {n|s|e|w|ne|sw|nw|se|u|d}' _map_hook = \
            /map %%*%;\
;       _map_send catches and sends anything _map_hook caught, unless there was
;       a non-fall-thru hook of intermediate priority that blocked it.
;       note: _map_send is tested by speedwalk.tf.
	/def -i -mglob -h'send {n|s|e|w|ne|sw|nw|se|u|d}' _map_send = \
            /send %%*

/def -i map	= /set path=%path %1

/def -i unmark	=\
    /set path=%;\
    /undef _map_hook%;\
    /undef _map_send%;\
    /echo -e %% Mapping disabled.

/def -i path	= /echo -e %% Path: %path

/def -i savepath= /def -i %1 = /dopath %path

/def -i dopath = \
    /while ({#}) \
	/if ( {1} =/ '[0-9]*' & {#} >= 2 ) \
	    /for i 1 %1 %2%; \
	    /shift 2%; \
	/else \
	    %1%; \
	    /shift%; \
	/endif%; \
    /done

/def -i unpath	= /set path=$(/all_but_last %path)

/def -i revert = \
	/let _dir=$(/last %path)%;\
	/unpath%;\
;       These directions must be listed in complementary pairs.
	/_revert_aux  n s  e w  ne sw  nw se  u d%;

/def -i _revert_aux = \
    /while ({#}) \
        /if ( _dir =~ {1} ) /send - %2%; /return%; \
        /elseif ( _dir =~ {2} ) /send - %1%; /return%; \
        /else   /shift 2%;\
        /endif%; \
    /done%; \
    /echo -e %% Don't know how to revert from "%_dir".%;\
    /set path=%path %_dir

/def -i all_but_last = /echo - %-L
