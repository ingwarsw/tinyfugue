;;;; rwho - remote who
;;; syntax:  /rwho
;;;          /rwho name=<playername>
;;;          /rwho mud=<mudname>
;;; Gets a remote WHO list from a mudwho server.  The first form gives a
;;; complete list, the other forms give partial lists.  Due to the short
;;; timeout of the mudwho server, sometimes the complete list is sent
;;; even if the second or third format is used (send complaints to the
;;; author or maintainer of the mudwho server, not to me).

;;; Make sure you /load rwho.tf _after_ you define your worlds, or rwho
;;; will be the default world.

/loaded __TFLIB__/rwho.tf

;; This site is current as of November 1993, but is subject to change.
/addworld rwho riemann.math.okstate.edu 6889

/eval /def -ag -i -p%{maxpri} -msimple -h'WORLD rwho' ~world_rwho

/def -i rwho = \
    /def -iF -1 -msimple -h'CONNECT rwho' ~connect_rwho = \
        /send -wrwho - %*%%; \
        /def -ag -iF -1 -msimple -h'DISCONNECT rwho' ~disconnect_rwho = \
            /def -ag -i -1 -p%{maxpri} -msimple -h'WORLD ${world_name}'%%%;\
            /fg -s ${world_name}%; \
    /connect rwho
