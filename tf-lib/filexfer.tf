;;;; File transfer macros
;; see "/help /putfile" and "/help /getfile".

/loaded __TFLIB__/filexfer.tf

/def -i putfile = /putfile_MUCK %*

/def -i putfile_MUCK =\
    @edit %{2-%{1}}%;\
    i%;\
    /quote -S '%1%;\
    .%;\
    q

/def -i putfile_UNIX =\
    ed %{2-%{1}}%;\
;   "1,$c" would be easier than "1,$d" "a", but doesn't work with LP's ed.
    1,$$d%;\
    a%;\
    /quote -S '%1%;\
    .%;\
    w%;\
    q

; I'm told that this works.
/def -i putfile_LP = /putfile_UNIX %*

;;; Mud-to-local file transfer:  /getfile <file> [<remote-file>]
; Note: if there is a log open for the current world, it will be closed.

/def -i getfile = /getfile_MUCK %*

/def -i getfile_MUCK =\
    /def -i -w -1 -aG -p98 -msimple -t"Editor exited." _getfile_end =\
        /log -w OFF%%;\
        /undef _getfile_quiet%;\
    /def -i -w -1 -p99 -msimple -t"Entering editor." _getfile_start =\
        /sys rm -f '%1'%%;\
        /log -w %1%%;\
        /def -i -w -p97 -ag -mglob -t"*" _getfile_quiet%;\
    @edit %{2-%{1}}%;\
    1 99999 l%;\
    q

/def -i getfile_LP =\
    /def -i -w -1 -aG -p98 -msimple -t":Exit from ed." _getfile_end =\
        /log -w OFF%%;\
        /undef _getfile_quiet%;\
    /def -i -w -1 -p99 -mregexp -t"^:" _getfile_start =\
        /sys rm -f '%1'%%;\
        /log -w %1%%;\
        /def -i -w -p97 -ag -mglob -t"*" _getfile_quiet%%;\
        /@test substitute({PR})%;\
    ed %{2-%{1}}%;\
    1,$$p%;\
    q

/def -i getfile_UNIX =\
    /def -i -w -1 -agG -p98 -msimple -t"GETFILE DONE" getfile_end =\
        /log -w OFF%%;\
        /undef _getfile_quiet%;\
    /sys rm -f '%1'%;\
    /log -w %1%;\
    /def -i -w -p97 -ag -mglob -t"*" _getfile_quiet%;\
    cat %{2-%{1}}; echo GETFILE DONE

