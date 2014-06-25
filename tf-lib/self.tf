;;; Macro that prints its own body (without using ${self} or $(/list self)).
;;; "/eval /def self = $(/self)" will make /self redefine itself.
;;; Not useful, just interesting.

/def self=/let q="%;/let p=%% %;/let f=strcat("/let q=",q,p,";/let p=",p,p," ",p,";/let f=",f,p,";/test echo(",p,"f)")%;/test echo(%f)

