;;;; emacs style keybindings for TinyFugue

/loaded __TFLIB__/kb-emacs.tf

/def -i -b"^a"		= /dokey_home
/def -i -b"^b"		= /dokey_left
/def -i -b"^d"		= /dokey_dch
/def -i -b"^e"		= /dokey_end
/def -i -b"^f"		= /dokey_right
;def -i -b"^j"		= /dokey newline
/def -i -b"^k"		= /dokey_deol
/def -i -b"^l"		= /dokey redraw
/def -i -b"^n"		= /dokey recallf
/def -i -b"^p"		= /dokey recallb
/def -i -b"^r"		= /dokey searchb
/def -i -b"^s"		= /dokey searchf
/def -i -b"^v"		= /dokey page
;def -i -b"^?"		= /dokey_bspc
/def -i -b"^hm"		= /visual
/def -i -b"^hb"		= /list -ib
/def -i -b"^h?"		= /help
/def -i -b"^h^h"	= /help
/def -i -b"^x^b"	= /listsockets

/if ( systype() =~ "unix" ) \
    /def -i -b"^x^d"	= /quote -S -decho !ls -FC%; \
/elseif ( systype() =~ "os/2" ) \
    /def -i -b"^x^d"	= /quote -S -decho !dir%; \
/endif

/def -i -b"^x1"		= /visual off
/def -i -b"^x2"		= /visual on
/def -i -b"^xk"		= /dc
/def -i -b"^[!"		= /sh
/def -i -b"^[>"		= /dokey flush
/def -i -b"^[b"		= /dokey wleft
/def -i -b"^[f"		= /dokey wright
/def -i -b"^[n"		= /dokey socketf
/def -i -b"^[p"		= /dokey socketb
/def -i -b"^[v"		= /dokey pageback
/def -i -b"^[^?"	= /kb_backward_kill_word
