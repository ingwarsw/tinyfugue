;;; textutil.tf
;;; unix-like text utilities

/loaded __TFLIB__/textutil.tf

; Note: users should not rely on %_loaded_libs like this.  I can get away
; with this here only because this and /loaded are both internal to TF.
/if (_loaded_libs =/ "*{__TFLIB__/grep.tf}*") \
    /echo -e %% Warning: textutil.tf and grep.tf have conflicting defintions.%;\
/endif

/require lisp.tf


/def -i _grep = \
    /let _line=%; \
    /let _count=0%; \
    /while (tfread(_line) >= 0) \
        /test (%{*}) & (++_count, opt_c | tfwrite(_line))%; \
    /done%; \
    /test opt_c & echo(_count)%; \
    /return _count

; ... %| /fgrep [-cvi] <string>
/def -i fgrep = \
    /if (!getopts("cvi", 0)) /return 0%; /endif%; \
    /if (opt_i) \
        /let _pattern=$[tolower({*})]%; \
        /_grep (strstr(tolower(_line), _pattern) < 0) == opt_v%; \
    /else \
        /let _pattern=%*%; \
        /_grep (strstr(_line, _pattern) < 0) == opt_v%; \
    /endif

; ... %| /grep [-cvi] <glob>
/def -i grep = \
    /if (!getopts("cvi", 0)) /return 0%; /endif%; \
    /let _pattern=%*%; \
    /_grep (_line !/ _pattern) == opt_v

; ... %| /egrep [-cvi] <regexp>
/def -i egrep = \
    /if (!getopts("cvi", 0)) /return 0%; /endif%; \
    /if (opt_i) \
        /let _pattern=$[tolower({*})]%; \
        /_grep !regmatch(_pattern, tolower(_line)) == opt_v%; \
    /else \
        /let _pattern=%*%; \
        /_grep !regmatch(_pattern, _line) == opt_v%; \
    /endif


; /copyio <in_handle> <out_handle>
; copies lines from <in_handle> to <out_handle>.
/def -i copyio = \
    /let _in=%{1-i}%; \
    /let _out=%{2-o}%; \
    /let _line=%; \
    /while (tfread(_in, _line) >= 0) \
        /test tfwrite(_out, _line)%; \
    /done

; /readfile <file> %| ...
/def -i readfile = \
    /let _handle=%; \
    /test ((_handle := tfopen({1}, "r")) >= 0) & \
        (copyio(_handle, "o"), tfclose(_handle))

; ... %| /writefile <file>
/def -i writefile = \
    /let _handle=%; \
    /if (!getopts("a", 0)) /return 0%; /endif%; \
    /test ((_handle := tfopen({1}, opt_a ? "a" : "w")) >= 0) & \
        (copyio("i", _handle), tfclose(_handle))


; ... %| /head [-n<count>] [<handle>]
; outputs first <count> lines of <handle> or tfin.
/def -i head = \
    /if (!getopts("n#", 10)) /return 0%; /endif%; \
    /let _handle=%{1-i}%; \
    /let _line=%; \
    /while (tfread(_handle, _line) >= 0) \
        /if (--opt_n < 0) /break%; /endif%; \
        /test echo(_line)%; \
    /done


; ... %| /wc [-lwc] [<handle>]
; counts lines, words, and/or characters of text from <handle> or tfin.
/def -i wc = \
    /if (!getopts("lwc", 0)) /return 0%; /endif%; \
    /let _handle=%{1-i}%; \
    /let _lines=0%; \
    /let _words=0%; \
    /let _chars=0%; \
    /let _line=%; \
    /let _body=0%; \
    /if (!opt_l & !opt_w & !opt_c) /test opt_l:= opt_w:= opt_c:= 1%; /endif%; \
    /if (opt_l) /let _body=%_body, ++_lines%; /endif%; \
    /if (opt_w) /let _body=%_body, _words+=$$(/length %%_line)%; /endif%; \
    /if (opt_c) /let _body=%_body, _chars+=strlen(_line)%; /endif%; \
    /eval \
        /while (tfread(_handle, _line) >= 0) \
            /test %_body%%; \
        /done%; \
    /echo $[opt_l ? _lines : ""] $[opt_w ? _words : ""] $[opt_c ? _chars : ""]

; ... %| /tee <handle> %| ...
; copies tfin to <handle> AND tfout.
/def -i tee = \
    /let _line=%; \
    /while (tfread(_line) >= 0) \
        /test tfwrite({*}, _line), tfwrite(_line)%; \
    /done

; ... %| /fmt 
; copies input to output, with adjacent non-blank lines joined
/def -i fmt = \
    /let _line=%; \
    /let _text=%; \
    /while (tfread(_line) >= 0) \
        /if (_line =~ "" & _text !~ "") \
            /echo - %{_text}%; \
            /echo%; \
            /let _text=%; \
        /else \
            /let _text=%{_text} %{_line}%; \
        /endif%; \
    /done%; \
    /echo - %{_text}

; ... %| /uniq
; copies input to output, with adjacent duplicate lines removed
/def -i uniq = \
    /let _prev=%; \
    /let _line=%; \
    /while (tfread(_line) >= 0) \
        /if (_line !~ _prev) \
            /test echo(_line), _prev:=_line%; \
        /endif%; \
    /done

; ... %| /randline [<handle>]
; reads lines from <handle> or tfin, and copies one at random to tfout.
/def -i randline = \
    /let _in=%{1-i}%; \
    /let _line=%; \
    /let _selection=%; \
    /let _lines=0%; \
    /while (tfread(_in, _line) >= 0) \
        /test rand(++_lines) | (_selection := {*})%; \
    /done%; \
    /result _selection

