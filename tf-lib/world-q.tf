;;;; Active worlds
;;; If you like to connect to a lot of worlds at the same time, you may find
;;; these macros useful.  Typing ESC-w will almost always fg the world you 
;;; want.

;;; Whenever activity occurs in a background world, these macros will add
;;; the name of that world to a queue.  Then, when you type ``ESC w'', they
;;; will switch to to the first world on the queue.  So by typing ``ESC w''
;;; repeatedly, you can visit all your active worlds.  If the queue is
;;; empty, ``ESC w'' will switch you to the last world you visited that is
;;; still connected.

/loaded __TFLIB__/world-q.tf

/require -q stack-q.tf
/require -q lisp.tf
/require -q textencode.tf

/if (active_worlds =~ "") /set active_worlds=%; /endif

/def -ib'^[w' = /to_active_or_prev_world

; Use %1 instead of $world_name so this can be called by name
/def -iFp1 -h"BGTEXT" activity_queue_hook = \
;   world may already be in active_worlds because of scrollback, etc
    /if /test moresize("", {1}) > 0 & active_worlds !/ "*{%1}*"%; /then \
	/enqueue %1 active_worlds%; \
    /endif

; don't queue world "rwho".
/def -ip2 -msimple -h"BGTEXT rwho" activity_rwho_hook

/def -iFp1 -h"WORLD" prev_world_hook =\
    /if (fg_world !~ "") \
        /if (moresize("", fg_world)) \
            /set active_worlds=%{active_worlds} %{fg_world}%; \
        /else \
            /set prev_worlds=%fg_world $(/remove %fg_world %prev_worlds)%;\
        /endif%; \
    /endif%;\
    /set fg_world=${world_name}%;\
    /if (fg_world !~ "") \
        /set active_worlds=$(/remove %{fg_world} %{active_worlds})%; \
    /endif

; don't remember world "rwho".
/def -ip2 -msimple -h"WORLD rwho" prev_rwho_hook

/def -i to_active_world = \
    /if ( active_worlds =~ "" ) \
        /echo -e %% No active worlds.%;\
    /else \
        /fg $(/dequeue active_worlds)%;\
    /endif

/def -i to_active_or_prev_world = \
    /if ( active_worlds !~ "" ) \
        /fg $(/dequeue active_worlds)%;\
    /else \
        /while ( prev_worlds !~ "" ) \
            /if /fg -s $(/dequeue prev_worlds)%; /then /return 1%; /endif%;\
        /done%;\
    /endif

/def -i list_active_worlds = \
    /if ( active_worlds =~ "" ) \
        /echo -e %% No active worlds.%;\
    /else \
        /echo -e %% Active worlds:  %{active_worlds}%;\
    /endif

