;;;; testcolor.tf

/test echo("               01234567 01234567")
/set _line=@
/for _i 0 7 \
    /set _line=%{_line}{C%_i}#@
/eval /set _line=%{_line}{n} @
/for _i 0 7 \
    /set _line=%{_line}{Cbg%_i} @
/eval /echo -p - Basic colors:  %{_line}{}

/set _line=@
/for _i 8 15 \
    /set _line=%{_line}{C%_i}#@
/eval /set _line=%{_line}{n} @
/for _i 8 15 \
    /set _line=%{_line}{Cbg%_i} @
/eval /echo -p - Bright colors: %{_line}{}

/_echo

/if (!features("256colors")) \
    /_echo The 256colors feature is not enabled.%; \
    /exit%; \
/endif

/_echo 6x6x6 color cubes:

/echo r=  000000111111222222333333444444555555 000000111111222222333333444444555555
/echo g=  012345012345012345012345012345012345 012345012345012345012345012345012345

/for _blue 0 5 \
    /set _line=b=%_blue @%; \
    /for _red 0 5 \
	/for _green 0 5 \
	    /set _line=%%%_line{Crgb%%%_red%%%_green%%%_blue}#@%%; \
	/set _line=%%{_line}{n}@%; \
    /set _line=%{_line}{} @%; \
    /for _red 0 5 \
	/for _green 0 5 \
	    /set _line=%%%_line{Cbgrgb%%%_red%%%_green%%%_blue} @%%; \
	/set _line=%%{_line}{n}@%; \
    /echo -p - %{_line}{}
/_echo

/test echo("                      1         2              1         2   ")
/test echo("            012345678901234567890123 012345678901234567890123")
/set _line=@
/for _i 0 23 \
    /set _line=%{_line}{Cgray%_i}#@
/eval /set _line=%{_line}{n} @
/for _i 0 23 \
    /set _line=%{_line}{Cbggray%_i} @
/eval /echo -p - Grayscales: %{_line}{}

/unset _line
