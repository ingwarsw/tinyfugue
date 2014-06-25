;;;; Miscellaneous useful macros.

/loaded __TFLIB__/tools.tf
/require lisp.tf


; for compatibility
/def -i shl = /sys %*


;;; Put an existing {macro,variable,world} definition in the input window for
;;; editing.
; syntax:  /ed{mac,var,world} <name>

/def -i edmac = /grab $(/cddr $(/@list -i - %{L-@}))
/def -i edvar = /grab $(/@listvar - %{L-@})
/def -i edworld = /grab $(/@listworlds -c - %{L-@})

/def -i reedit = /edmac %*


;;; name - change your name (on a TinyMUD style mud)
; syntax:  /name [<name>]

/def -i name =\
    /if (${world_type} =/ "tiny.mush*") \
        @name me=%{1-${world_character}}%;\
    /elseif (${world_type} =/ "tiny*") \
        @name me=%{1-${world_character}} ${world_password}%;\
    /else \
        /echo -e %% %0: Assuming this is a TinyMUD-like server.%;\
        @name me=%{1-${world_character}} ${world_password}%;\
    /endif


;;; getline - grab the nth line from history and stick it in the input buffer
; syntax:  /getline <n>

/def -i getline = /quote /grab #%{1}-%{1}


;;; xtitle - change the titlebar on an xterm.
; syntax:  /xtitle <text>

/def -i xtitle = /echo -r \033]2;%*\07
