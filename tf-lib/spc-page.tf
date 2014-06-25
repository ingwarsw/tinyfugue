;;;; space-page
;; This file allows the use of the SPACE key to scroll at a More prompt,
;; like tf versions prior to 2.0.  The TAB and PageDown keys still work.
;; I personally don't like it, but you might if you can't get the hang of
;; pressing TAB or PageDown.  To enable space-page, just load this file.
;; To disable it, "/undef space_page".

/def -ib" " space_page = /test moresize() > 0 ? dokey_page() : input(" ")

