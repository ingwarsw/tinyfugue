;;;; factorial macros
;;; Not very useful, but they do demonstrate some macro programming techniques.

;; recursive factorial

/def rfact = \
    /if ( {1} < 0 ) \
        /echo -e %% %0: negative argument%; \
    /elseif ( {1} == 0 ) \
        /result 1%; \
    /else \
        /result {1} * rfact({1} - 1)%; \
    /endif

;; iterative factorial - more efficient

/def ifact = \
    /if ( {1} < 0 ) \
        /echo -e %% %0: negative argument%; \
    /else \
        /let n=%1%; \
        /let result=1%; \
        /while (n) \
            /test (result:=result * n), --n%; \
        /done%; \
        /result result%; \
    /endif
