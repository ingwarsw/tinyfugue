/set c1=@{Cbgrgb500} @{}
/set c2=@{Cbgrgb400} @{}
/set c3=@{Cbgrgb300} @{}
/set c4=@{Cbgrgb200} @{}
/set c5=@{Cbgrgb100} @{}
/set c6=@{Cbgrgb000} @{}
/eval /set cylon0=@{Cbgblack}%{c5}%{c4}%{c3}%{c2}%{c1}%{c6}       @{n}
/eval /set cylon1=@{Cbgblack} %{c5}%{c4}%{c3}%{c2}%{c1}%{c6}      @{n}
/eval /set cylon2=@{Cbgblack}  %{c5}%{c4}%{c3}%{c2}%{c1}%{c6}     @{n}
/eval /set cylon3=@{Cbgblack}   %{c5}%{c4}%{c3}%{c2}%{c1}%{c6}    @{n}
/eval /set cylon4=@{Cbgblack}    %{c5}%{c4}%{c3}%{c2}%{c1}%{c6}   @{n}
/eval /set cylon5=@{Cbgblack}     %{c5}%{c4}%{c3}%{c2}%{c1}%{c6}  @{n}
/eval /set cylon6=@{Cbgblack}      %{c5}%{c4}%{c3}%{c2}%{c1}%{c6} @{n}
/eval /set cylon7=@{Cbgblack}       %{c5}%{c4}%{c3}%{c2}%{c1}%{c6}@{n}
/eval /set cylon8=@{Cbgblack}        %{c5}%{c4}%{c1}%{c2}%{c6}@{n}
/eval /set cylon9=@{Cbgblack}         %{c1}%{c2}%{c3}%{c6}@{n}
/eval /set cylon10=@{Cbgblack}        %{c1}%{c2}%{c3}%{c4}%{c6}@{n}
/eval /set cylon11=@{Cbgblack}       %{c1}%{c2}%{c3}%{c4}%{c5}%{c6}@{n}
/eval /set cylon12=@{Cbgblack}      %{c1}%{c2}%{c3}%{c4}%{c5}%{c6} @{n}
/eval /set cylon13=@{Cbgblack}     %{c1}%{c2}%{c3}%{c4}%{c5}%{c6}  @{n}
/eval /set cylon14=@{Cbgblack}    %{c1}%{c2}%{c3}%{c4}%{c5}%{c6}   @{n}
/eval /set cylon15=@{Cbgblack}   %{c1}%{c2}%{c3}%{c4}%{c5}%{c6}    @{n}
/eval /set cylon16=@{Cbgblack}  %{c1}%{c2}%{c3}%{c4}%{c5}%{c6}     @{n}
/eval /set cylon17=@{Cbgblack} %{c1}%{c2}%{c3}%{c4}%{c5}%{c6}      @{n}
/eval /set cylon18=@{Cbgblack}%{c1}%{c2}%{c3}%{c4}%{c5}%{c6}       @{n}
/eval /set cylon19=@{Cbgblack}%{c2}%{c1}%{c4}%{c5}%{c6}        @{n}
/eval /set cylon20=@{Cbgblack}%{c3}%{c2}%{c1}%{c6}         @{n}
/eval /set cylon21=@{Cbgblack}%{c4}%{c3}%{c2}%{c1}%{c6}        @{n}

/for i 0 21 /test cylon%i := decode_attr(cylon%i)
/for i 1 6 /unset c%i

/def cylon = \
    /if (cylon_pid > 0) /kill %cylon_pid%; /endif%; \
    /if ({*} =~ "off") \
	/status_rm cylon%; \
    /else \
	/set cylon_i=0%; \
	/set cylon=%; \
	/repeat -0.08 i \
	    /test cylon := cylon%%cylon_i%%; \
	    /test cylon_i := mod(cylon_i + 1, 22)%; \
	/eval /set cylon_pid=%%?%; \
	/status_add %{*--A@world} cylon:12%; \
    /endif
