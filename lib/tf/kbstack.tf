;;;; Keyboard stack
;;;; This is useful when you're in the middle of typing a long line,
;;;; and want to execute another command without losing the current line.
;;;; Type ESC DOWN to save the current input line on a stack.
;;;; Type ESC UP to restore the saved line.  Any number of lines can
;;;; be saved and restored.

/loaded __TFLIB__/kbstack.tf

/purge -i key_esc_down
/purge -i key_esc_up
/def -i key_esc_down = /kb_push
/def -i key_esc_up = /kb_pop

/def -i kb_push = \
    /let n=$[+kbnum]%; \
    /if (n < 0) \
	/echo -A %% %0: illegal stack number %n.%; \
	/return 0%; \
    /endif%; \
    /let _line=$(/recall -i 1)%;\
    /if ( _line !~ "" ) \
        /eval \
	    /set _kb_stack_%{n}_top=$$[_kb_stack_%{n}_top + 1]%%;\
	    /set _kb_stack_%{n}_%%{_kb_stack_%{n}_top}=%%{_line}%;\
    /endif%;\
    /dokey dline

/def -i kb_pop = \
    /let n=$[+kbnum]%; \
    /if /test %{n} >= 0 & _kb_stack_%{n}_top > 0%; /then \
        /dokey dline%;\
        /eval \
	    /@test input(_kb_stack_%{n}_%%{_kb_stack_%{n}_top})%%;\
	    /unset _kb_stack_%{n}_%%{_kb_stack_%{n}_top}%%;\
	    /set _kb_stack_%{n}_top=$$[_kb_stack_%{n}_top - 1]%;\
    /else \
        /echo -A %% %0: Keyboard stack %n is empty.%;\
    /endif

