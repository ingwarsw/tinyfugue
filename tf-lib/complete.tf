;;;; Completion.
;; This allows you to type part of a word, hit a special key, and have
;; the rest of the word filled in for you automagically.  ESC TAB attempts
;; to complete based on context, or from a user defined list of words;
;; a few other bindings will do more explicit completions.
;;
;; To use:  /load this file, and optionally store a list of words in
;; %{completion_list}.  For example, add this to your ~/.tfrc file:
;;
;;    /load complete.tf
;;    /set completion_list=Hawkeye TinyFugue tinyfugue.sourceforge.net
;;
;; Completion keys:
;;
;; ESC TAB	complete from context, input history, or %{completion_list}.
;; ESC ;	complete from %{completion_list}.
;; ESC i	complete from input history.
;; ESC /	filename completion (UNIX only).
;; ESC @	hostname completion.
;; ESC %	variable name completion.
;; ESC $	macro name completion.
;; ESC ^W	world name completion.

;; By "from context", I mean it will look for patterns and decide which
;; type of completion to use.  For example, if the line begins with "/load",
;; it will use filename completion; if the word begins with "%" or "%{", it
;; will use variable name completion; etc.

;; Optional user extensions.
;; To add your own completion, write a macro with a name like complete_foo
;; which takes the partial word as an argument, and calls /_complete_from_list
;; with the partial word and a list of possible matches.  Then bind a key
;; sequence that calls "/complete foo", and/or add a context
;; sensitive call in "/complete_context".

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

/loaded __TFLIB__/complete.tf

/require lisp.tf

/def -i key_esc_tab = /complete
/def -ib'^[;'	= /complete user_defined
/def -ib'^[i'	= /complete input_history
/def -ib'^[/'	= /complete filename
/def -ib'^[@'	= /complete hostname
/def -ib'^[%'	= /complete variable
/def -ib'^[$'	= /complete macroname
/def -ib'^[^W'	= /complete worldname

; /def -ib'^[~'	= /complete playername  (not implemented)
; /def -ib'^[!'	= /complete command     (not implemented)

;; /complete_playername is difficult to do correctly because of mud delays,
;; and is not portable to different muds, so it is not implemented here.


/def -i complete = \
    /complete_%{1-context} $(/last $[kbhead()])


;; /_complete_from_list <word> <list...>
;; <word> is the partial word to be completed.
;; <list> is a list of possible matches.
;; If <word> matches exactly one member of <list>, that member will be
;; inserted in the input buffer.  If multiple matches are found, the
;; longest common prefix will be inserted; if this is not the first time
;; this identical list of matches has been generated, the list will
;; be displayed.  If no matches are found, it simply beeps.
;; If exactly one match was found, %{_completion_suffix} or a space
;; will be appended to the completed word.
;; If the local variable %{_need_unique} is true, the list will be run
;; through /unique.

/def -i _complete_from_list = \
    /let _prefix=%1%;\
    /shift%;\
    /let _len=$[strlen(_prefix)]%;\
    /let _match=%;\
;
;   scan list for words which start with prefix.
    /while ({#}) \
        /if (strncmp({1}, _prefix, _len) == 0) \
            /let _match=%{_match} %{1}%;\
        /endif%;\
        /shift%;\
    /done%;\
;
;   Remove duplicates (and strip leading space)
    /if (_need_unique) \
        /let _match=$(/unique %{_match})%;\
    /endif%;\
;   strip leading/trailing space
    /let _match=$[regmatch('^ *(.*?) *$', _match), {P1}]%;\
;
    /if (_match =~ "") \
;       No match was found.
        /beep 1%;\
    /elseif (_match !/ "{*}") \
;       Multiple matches were found.  Use longest common prefix.
        /beep 1%;\
        /@test input(substr($$(/common_prefix %%{_len} %%{_match}), _len))%;\
        /if (_match =~ _prev_match) \
	    /echo - %{_match}%;\
	/endif%; \
	/set _prev_match=%_match%; \
    /else \
;       Exactly one match was found.
        /@test _match := strcat(_match, _completion_suffix)%;\
        /if (_match =/ "*[A-Za-z0-9_]") \
            /@test _match := strcat(_match, " ")%;\
        /endif%;\
        /@test input(substr(_match, _len))%;\
    /endif%;\
;   Just to be safe
    /unset _completion_suffix%;\
    /unset _need_unique


/def -i complete_user_defined = \
    /_complete_from_list %1 %completion_list

/def -i ~input_history_list = \
    /let _input=$(/recall -i #1)%;\
    /recall -i 1-$[substr(_input, 0, strchr(_input, ":")) - 1]

/def -i complete_input_history = \
    /let _need_unique=1%;\
    /_complete_from_list %1 $(/~input_history_list)

/def -i complete_dynamic = \
    /let _need_unique=1%;\
    /_complete_from_list %1 %completion_list $(/~input_history_list)


/def -i complete_filename = \
    /quote -S /_complete_from_list $[filename({1})] !\
        echo `/bin/ls -dq $[filename({1})]* 2>/dev/null | \
            while read f; do \ test -d $$f && echo $$f/ || echo $$f; done`


/def -i complete_hostname = \
    /let _need_unique=1%;\
    /let _pf=$[substr({1}, strrchr({1}, "@") + 1)]%;\
    /quote -S /_complete_from_list %{_pf} !\
       echo `cat /etc/hosts %HOME/etc/hosts 2>/dev/null | \
          sed -n '/^[^#].*[ 	][ 	]*\\\\(%{_pf}[^ 	]*\\\\).*/s//\\\\1/p'`


/def -i complete_variable = \
    /let _part=$[substr({1}, strrchr({1}, '%') + 1)]%;\
    /if (strncmp(_part, '{', 1) == 0) \
        /let _part=$[substr(_part, 1)]%;\
        /let _completion_suffix=}%;\
    /endif%;\
    /_complete_from_list %_part $(/@listvar -s)


/def -i complete_macroname = \
    /let _word=%1%;\
    /let _i=$[strrchr({1}, '$')]%;\
    /if (_i >= 0) \
        /if (substr(_word, _i+1, 1) =~ '{') \
            /@test ++_i%;\
            /let _completion_suffix=}%;\
        /endif%;\
        /let _word=$[substr(_word, _i+1)]%;\
    /elseif (strncmp(_word, '/', 1) == 0) \
        /let _word=$[substr(_word, 1)]%;\
    /endif%;\
    /_complete_from_list %{_word} $(/quote -S /last `/@list -s -i - %{_word}*)


/def -i complete_worldname = \
    /_complete_from_list %1 $(/@listworlds -s %{1}*)

/def -i complete_sockname = \
    /_complete_from_list %1 $(/@listsockets -s %{1}*)


;; /complete_context <word>
;; Uses context to determine which completion macro to use.

/def -i complete_context = \
    /let _head=$[kbhead()]%;\
    /let _word=%1%;\
    /if (strchr(_word, "@") >= 0) \
        /complete_hostname %1%;\
    /elseif (strchr(_word, "%%") >= 0) \
        /complete_variable %1%;\
    /elseif (strchr(_word, "$$") >= 0) \
        /complete_macroname %1%;\
;   /elseif (_head =/ "{/*}") \
;       /complete_command %1%;\
    /elseif (_head =/ "{/*}") \
        /complete_macroname %1%;\
    /elseif (regmatch("-w(.+)$$", _head)) \
        /complete_worldname %P1%;\
    /elseif (_head =/ "*{/[sl]et|/setenv|/unset|/listvar|/edvar} {*}") \
        /complete_variable %1%;\
    /elseif (_head =/ "*{/load*|/save*|/lcd|/cd|/log} {*}") \
        /complete_filename %1%;\
    /elseif (_head =/ "*{/def|/edit|/edmac|/reedit|/undef|/list} {*}*") \
        /complete_macroname %1%;\
;   /elseif (_head =/ "{wh*|page|tel*|kill} {*}") \
;       /complete_playername %1%;\
    /elseif (regmatch(`/quote .*'("?)(.+)$$`, _head)) \
        /let _completion_suffix=%P1%;\
        /complete_filename %P2%;\
;   /elseif (regmatch('/quote .*`("?)(.+)$$', _head)) \
;       /let _completion_suffix=%P1%;\
;       /complete_command %P2%;\
    /elseif (regmatch('/quote .*`("?)(.+)$$', _head)) \
        /let _completion_suffix=%P1%;\
        /complete_macroname %P2%;\
    /elseif (_head =/ "*{/world|/connect|/edworld} {*}") \
        /complete_worldname %1%;\
    /elseif (_head =/ "*{/fg} {*}") \
        /complete_sockname %1%;\
    /elseif (_head =/ "*{/telnet} {*}") \
        /complete_hostname %1%;\
    /elseif (_head =/ "*/quote *!*") \
        /complete_filename %1%;\
    /elseif (_head =/ "*{/@test|/expr} *") \
        /complete_variable %1%;\
    /elseif (_head =/ "*{*/*|.*|tiny.*}") \
        /complete_filename %1%;\
    /else \
        /complete_dynamic %1%;\
    /endif


;;; /common_prefix <min> <list>...
;; prints the common prefix shared by each word in <list>, assuming at least
;; first <min> chars are already known to be shared.

/def -i common_prefix = \
    /let _min=%1%;\
    /shift%;\
    /let _prefix=%1%;\
    /let _len=$[strlen(_prefix)]%;\
    /while /shift%; /@test {#} & _len > _min%; /do \
        /let _i=%_min%;\
        /while (_i < _len & strncmp(_prefix, {1}, _i+1) == 0) \
            /@test ++_i%;\
        /done%;\
        /let _len=%_i%;\
    /done%;\
    /echo - $[substr(_prefix, 0, _len)]

