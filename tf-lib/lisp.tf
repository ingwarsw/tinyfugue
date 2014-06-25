;;; Lisp-like macros
; These macros return values via /result, so they can be used in $() command
; subs, or as functions in expressions.

/loaded __TFLIB__/lisp.tf

/def -i car = /result {1}
/def -i cdr = /result {-1}
/def -i cadr = /result {2}
/def -i cddr = /result {-2}
/def -i caddr = /result {3}
/def -i cdddr = /result {-3}

/def -i length = /result {#}

/def -i reverse = \
    /let _result=%1%;\
    /while (shift(), {#}) \
        /let _result=%1 %_result%;\
    /done%;\
    /result _result

/def -i mapcar = \
    /let _cmd=%1%; \
    /while (shift(), {#}) \
        /eval %_cmd %%1%; \
    /done

/def -i maplist = \
    /let _cmd=%1%;\
    /while (shift(), {#}) \
        /eval %_cmd %%*%;\
    /done

/def -i remove = \
    /let _word=%1%;\
    /let _result=%;\
    /while (shift(), {#}) \
        /if (_word !~ {1}) \
            /let _result=%{_result} %{1}%;\
        /endif%;\
    /done%;\
    /result _result


;; Remove all duplicate items from list.
;; Note that the speed of this macro is O(N^2), so it is very slow on
;; long lists.

/def -i unique = \
    /let _result=%1 $[{#}>1 ? $(/unique $(/remove %1 %-1)) : ""]%; \
    /result _result

