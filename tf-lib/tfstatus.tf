;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;; TinyFugue - programmable mud client
;;;; Copyright (C) 1998-2002, 2003, 2004, 2005, 2006-2007 Ken Keys
;;;;
;;;; TinyFugue (aka "tf") is protected under the terms of the GNU
;;;; General Public License.  See the file "COPYING" for details.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;;; status bar definitions and utilities

/loaded __TFLIB__/tfstatus.tf


/set std_clock_format=%H:%M

/set status_std_int_more \
    (limit() | morepaused()) ? status_int_more() : ""
/def -i status_int_more = \
    /let new=$[moresize("ln")]%; \
    /let old=$[moresize("l") - new]%; \
    /if (limit() & !morepaused()) \
	/result "LIMIT ON"%; \
    /elseif (old) \
	/if (old >= 1000) /let old=$[old/1000]k%; /endif%; \
	/if (new >= 1000) /let new=$[new/1000]k%; /endif%; \
	/result pad(limit() ? "L" : " ", 1,  old, 3,  "+", 1,  new, 3)%; \
    /else \
	/if (new >= 10000) /let new=$[new/1000]k%; /endif%; \
	/result pad(limit() ? "LIM " : "More", 4,  new, 4)%; \
    /endif

/set status_std_int_world   \
     fg_world() =~ "" ? "(no world)" : \
     strcat(!is_open(fg_world()) ? "!" : "",  fg_world())
/set status_std_int_read    nread() ? "(Read)" : ""
/set status_std_int_active  nactive() ? pad("(Active:", 0, nactive(), 2, ")") : ""
/set status_std_int_log     nlog() ? "(Log)" : ""
/set status_std_int_mail \
    !nmail() ? "" : \
    nmail()==1 ? "(Mail)" : \
    pad("Mail", 0, nmail(), 2)
/set status_std_var_insert  insert ? "" : "(Over)"
/set status_std_int_clock   ftime(clock_format)

/set status_field_defaults \
    @more:8:Br :1 @world :1 \
    @read:6 :1 @active:11 :1 @log:5 :1 @mail:6 :1 insert:6 :1 \
    kbnum:4 :1 @clock:5

/def -i status_defaults = \
    /let _old_warn_status=%warn_status%; \
    /set warn_status=0%; \
    /set clock_format=%std_clock_format%; \
    /set status_int_more=%status_std_int_more%; \
    /set status_int_world=%status_std_int_world%; \
    /set status_int_read=%status_std_int_read%; \
    /set status_int_active=%status_std_int_active%; \
    /set status_int_log=%status_std_int_log%; \
    /set status_int_mail=%status_std_int_mail%; \
    /set status_var_insert=%status_std_var_insert%; \
    /set status_int_clock=%status_std_int_clock%; \
    /status_add -c - %status_field_defaults%; \
    /set warn_status=%_old_warn_status

/def -i status_save = \
    /set _status_save_%1=$[status_fields()]

/def -i status_restore = \
    /if /isvar _status_save_%1%; /then \
	/eval /status_add -c %%_status_save_%1%; \
    /else \
	/echo -e %% No saved status "%1".%; \
    /endif

/def -i clock = \
    /if ({*} =/ "{0|off|no}") \
	/status_rm @clock%; \
    /else \
	/if ({*} !/ "{1|on|yes}") \
	    /set clock_format=%{*-%%H:%%M}%; \
	/endif%; \
	/let width=$[strlen(ftime(clock_format))]%; \
	/if (regmatch("(^|\\s+)@clock(:[^: ]+(:[^ ]+)?)?\\b", status_fields)) \
	    /status_edit @clock:%width%P3%; \
	/else \
	    /status_add -A -x @clock:%width%; \
	/endif%; \
    /endif

/status_defaults

