;;; /changes
;; Using sed would be very fast, but not portable.
;; This version is a bit more complex, but portable and reasonably fast.

/def -i changes = \
    /let _ver=%{*-$(/ver)}%; \
    /let _pat=%{_ver} *%; \
    /let _name=%TFLIBDIR/CHANGES%; \
    /let _fd=$[tfopen(_name, "r")]%; \
    /let _line=%; \
    /while (1) \
	/while (1) \
	    /if (tfread(_fd, _line) < 0) /break 2%; /endif%; \
	    /if (_line =~ _ver | _line =/ _pat) /break 1%; /endif%; \
	/done%; \
	/test echo(_line)%; \
	/while (1) \
	    /if (tfread(_fd, _line) < 0) /break 2%; /endif%; \
	    /test echo(_line)%; \
	    /if (_line =~ "") /break 1%; /endif%; \
	/done%; \
    /done%; \
    /test tfclose(_fd)
