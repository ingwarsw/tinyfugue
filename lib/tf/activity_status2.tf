;;;; Socket line counts on status line

/loaded __TFLIB__/activity_status2.tf

/require -q activity_status.tf

/status_rm @read
/status_rm insert
/status_rm @log
/set status_int_read=nread() ? "R" : ""
/set status_var_insert=insert ? "" : "O"
/set status_int_log=nlog() ? "L" : ""
/status_add -A"activity_status" @read:1:r insert:1:r @log:1
