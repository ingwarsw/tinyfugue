;;;; Socket line counts on status line

/loaded __TFLIB__/activity_status.tf

/require -q world-q.tf
/require -q textencode.tf

/status_edit @world:11
/status_rm @active
/status_add -x -A"@world" activity_status

/def -qi -Fp2147483647 -mglob -h'WORLD' update_activity_fg = \
    /let label=$[textencode(fg_world())]%; \
    /set activity_color_%{label}=%; \
    /repeat -0 1 /update_activity

/def -i activity_color = \
    /if (${world_name} !~ fg_world()) \
	/let _var=activity_color_$[textencode(${world_name})]%; \
	/eval \
	    /if (!regmatch("(^|,)%1(,|$$)", %_var)) \
		/set %_var=%%{%_var},%1%%; \
	    /endif%; \
    /endif

/def -qi -Fp2147483647 -hPREACTIVITY activity_status_preactivity_hook = \
    /update_activity_delayed

/def -qi -Fp2147483647 -hBGTEXT activity_status_bgtext_hook = \
    /update_activity_delayed

/def -E'${world_name} !~ fg_world() & moresize("")' \
  -qi -Fp2147483647 -mglob -h'DISCONNECT' \
    update_activity_disconnect_hook = \
	/activity_queue_hook ${world_name}%; \
	/update_activity

;; /status_abbr n string
;; Abbreviate string so it fits in n characters.
;; Try to keep capitals, beginnings of words, and digits; discard everything
;; else as needed.
/def -i status_abbr = \
    /let n=%1%; \
    /let name=%-1%; \
; if name contains ':', abbreviate left side before right side
    /if (strlen(name) > n & regmatch(":", name)) \
	/let right=%{PR}%; \
	/test name:=strcat(status_abbr(n-strlen(right)-1, {PL}), ":", right)%; \
    /endif%; \
    /while (strlen(name) > n & \
        regmatch("((?:[A-Z]|(?<![A-Za-z])[a-z])[a-z]*)[a-z]((?:[^a-z]*(?:(?<![A-Za-z])[a-z])?)+)$", name)) \
        /let name=%PL%P1%P2%; \
    /done%; \
    /while (strlen(name) > n & \
        regmatch("[^A-Za-z0-9]", name)) \
        /let name=%PL%PR%; \
    /done%; \
    /result name

;; Abbreviate a string, for the status bar.  User can define custom
;; abbreviations with "/set_status_abbr <string> <abbr>", or writing his
;; own status_abbr_hook(n, str).  If both of those fail, use the default
;; intelligent abbreviator /status_abbr.
/def -i status_label = \
    /let n=$[status_width('activity_status') / $(/length %active_worlds) - 5]%;\
    /if (status_abbr_max > 0 & n > status_abbr_max) \
	/let n=%status_abbr_max%; \
    /endif%; \
    /if (strlen({*}) <= n) /result {*}%; /endif%; \
    /if /ismacro status_abbr_hook%; /then \
	/let abbr=$[status_abbr_hook(n, {*})]%; \
	/if (abbr !~ "") /result abbr%; /endif%; \
    /endif%; \
    /let abbr=%; \
    /test abbr:=status_abbr__$[textencode({*})]%; \
    /if (abbr !~ "") /result abbr%; /endif%; \
;   Abbreviate the name only as much as necessary to fit in n characters.
    /result status_abbr(n, {*})

;; /set_status_abbr <world> <abbr>
;; <abbr> may contain @{} attributes
/def -i set_status_abbr = /set status_abbr__$[textencode({-L})]=%L
/def -i unset_status_abbr = /unset status_abbr__$[textencode({*})]

;; Activity message is confusing with 5.0's per-world virtual screens, and
;; activity_status tells you what worlds have activity.
/def -i -ag -hACTIVITY gag_activity

;; NB: %* is not current world
/def -i update_activity_world = \
    /let _n=$[moresize("", {*})]%; \
    /let _name=$[textencode({*})]%; \
    /test activity_color_%{_name}%; \
    /echo -p - \
	@{%?}$[is_open({*})?"":"!"]$[status_label({*})]:\
	$[_n < 1000 ? _n : strcat(_n/1000, "k")]@{n}

/def -i update_activity = \
    /if (update_activity_pid) \
	/kill %update_activity_pid%; \
	/set update_activity_pid=0%; \
    /endif%; \
    /set activity_status=$(/mapcar /update_activity_world %active_worlds)

;; format: put in "(...)" if not blank
/set status_var_activity_status \
    (activity_status =~ "") ? "" : strcat('(', activity_status, ')')

/def -i update_activity_delayed = \
    /if (update_activity_pid) \
	/kill %update_activity_pid%; \
    /endif%; \
    /if (moresize("") == 0 | mod(moresize(""), 50) == 0) \
	/repeat -0 1 /update_activity%; \
	/set update_activity_pid=0%; \
    /else \
	/repeat -1 1 /update_activity%; \
	/set update_activity_pid=%?%; \
    /endif

/update_activity
