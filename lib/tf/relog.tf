;;; relog - recall into a log
; syntax:  /relog <file> <recall_arguments>
; Starts logging, and silently performs a /recall into the log file.

/loaded __TFLIB__/relog.tf
/require textutil.tf

/def -i relog = \
    /let out=%; \
    /if ((out:=tfopen({1}, "a")) < 0) /return 0%; /endif%; \
    /echo -e %% Recalling to log file %1%;\
    /test tfflush(out, 0)%; \
    /quote -S -decho #%-1 %| /copyio i %out%; \
    /test tfclose(out)%; \
    /log %1

