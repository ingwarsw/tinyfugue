;;;; Keybindings similar to the defaults in versions prior to 5.0.

/loaded __TFLIB__/kb-old.tf

/require kbfunc.tf

; ^J, ^M, ^H, and ^? are handled internally.

;; Named keys

/def -i key_Up		= /dokey_up
/def -i key_Down	= /dokey_down
/def -i key_Right	= /dokey_right
/def -i key_Left	= /dokey_left
/def -i key_F1		= /help
/def -i key_Home	= /dokey_home
/def -i key_End		= /dokey_end
/def -i key_Insert	= /@test insert := !insert
/def -i key_PgDn	= /dokey page


;; Defaults for keys normally defined by terminal driver.

/~bind_if_not_bound ^W /dokey_bword
/~bind_if_not_bound ^U /dokey dline
/~bind_if_not_bound ^R /dokey refresh
/~bind_if_not_bound ^V /dokey lnext


;; Other useful bindings

/def -ib'^A'	= /dokey_home
/def -ib'^B'	= /dokey_wleft
/def -ib'^D'	= /dokey_dch
/def -ib'^E'	= /dokey_end
/def -ib'^F'	= /dokey_wright
/def -ib'^G'	= /beep 1
/def -ib'^I'	= /dokey page
/def -ib'^K'	= /dokey_deol
/def -ib'^L'	= /dokey redraw
/def -ib'^N'	= /dokey recallf
/def -ib'^P'	= /dokey recallb
/def -ib'^T'	= /kb_transpose_chars
/def -ib'^[^E'	= /kb_expand_line
/def -ib'^[^L'	= /dokey clear
/def -ib'^[ '	= /kb_collapse_space
/def -ib'^[-'	= /kb_goto_match
/def -ib'^[.'	= /kb_last_argument
/def -ib'^[<'	= /dokey recallbeg
/def -ib'^[>'	= /dokey recallend
/def -ib'^[J'	= /dokey selflush
/def -ib'^[L'	= /dokey line
/def -ib'^[_'	= /kb_last_argument
/def -ib'^[b'	= /fg -<
/def -ib'^[c'	= /kb_capitalize_word
/def -ib'^[d'	= /dokey_dword
/def -ib'^[f'	= /fg ->
/def -ib'^[h'	= /dokey hpage
/def -ib'^[j'	= /dokey flush
/def -ib'^[l'	= /kb_downcase_word
/def -ib'^[n'	= /dokey searchf
/def -ib'^[p'	= /dokey searchb
/def -ib'^[u'	= /kb_upcase_word
/def -ib'^[v'	= /@test insert := !insert
/def -ib'^[^h'	= /kb_backward_kill_word
/def -ib'^[^?'	= /kb_backward_kill_word

