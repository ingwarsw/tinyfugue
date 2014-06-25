; finger - get user information.  Arguments are like unix finger:
; /finger user@host
; /finger user
; /finger @host
; /finger

; This is more complicated than it needs to be, just to make it act nicely.

; The empty WORLD hooks override normal hooks (like those in world-q.tf)
; that we don't want during finger.

/def -i finger = \
    /@test regmatch("^([^@]*)@?", {*})%; \
    /let _user=%{P1}%; \
    /let _host=%{PR-localhost}%; \
    /def -i _finger_exit = \
        /purge -i -msimple -h"CONNECT|CONFAIL|DISCONNECT {finger@%{_host}}*"%%;\
        /def -1 -i -ag -msimple -h'WORLD ${world_name}' -p%{maxpri}%%; \
        /fg ${world_name}%%; \
        /unworld finger@%{_host}%%; \
        /undef _finger_exit%; \
    /addworld finger@%{_host} %{_host} 79%; \
    /def -1 -i -ag -msimple -h'WORLD finger@%{_host}' -p%{maxpri}%; \
    /def -1 -i -ag -mglob -h'CONNECT {finger@%{_host}}*' -p%{maxpri} = \
        /fg finger@%{_host}%%; \
        /send -- %{_user}%; \
    /def -1 -i -ag -mglob -h'DISCONNECT {finger@%{_host}}*' -p%{maxpri} = \
        /_finger_exit%; \
    /def -1 -i -mglob -h'CONFAIL {finger@%{_host}}*' -p%{maxpri} = \
        /_finger_exit%; \
    /connect finger@%{_host}

