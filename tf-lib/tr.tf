;;;; Character translation
;;; usage:  /tr <domain> <range> <string>
;;; <domain> and <range> are lists of charcters.  Each character in <string>
;;; that appears in <domain> will be translated to the corresponding
;;; character in <range>.

;;; Example:
;;; command: /def biff = /tr OIS. 01Z! $[toupper({*})]
;;; command: /biff TinyFugue is cool wares, dude.
;;; output:  T1NYFUGUE 1Z C00L WAREZ, DUDE!

/loaded __TFLIB__/tr.tf

/def -i tr = \
    /let _old=%;\
    /let _new=%;\
    /let _tail=%;\
    /test _old:={1}%;\
    /test _new:={2}%;\
    /test _tail:={-2}%;\
    /let _dest=%;\
    /while /let _i=$[strchr(_tail, _old)]%; /@test _i >= 0%; /do \
        /let _j=$[strchr(_old, substr(_tail, _i, 1))]%;\
        /test _dest:=strcat(_dest, substr(_tail,0,_i), substr(_new, _j, 1))%;\
        /test _tail:=substr(_tail,_i+1)%;\
    /done%;\
    /result strcat(_dest, _tail)

