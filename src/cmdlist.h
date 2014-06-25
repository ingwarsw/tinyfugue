/* command list
 * defcmd and defvarcmd each define a command.
 * The "var" indicates that the function needs to be able to write into the
 * text of its String* argument (so the argument must be copied before calling
 * the func); "con" indicates the argument is String* and the function does
 * not write into it.
 */

#if !defined(defcmd)
#define defcmd(name, func, reserved) \
    extern struct Value *func(String *args, int offset);
#endif

#if NO_PROCESS
# define handle_kill_command         NULL
# define handle_ps_command           NULL
# define handle_quote_command        NULL
# define handle_repeat_command       NULL
#endif
#if NO_HISTORY
# define handle_histsize_command     NULL
# define handle_log_command          NULL
# define handle_recall_command       NULL
# define handle_recordline_command   NULL
# define handle_watchdog_command     NULL
# define handle_watchname_command    NULL
#endif


/* It is IMPORTANT that the commands be sorted by name! */
/*     name            function                     reserved? */
defcmd("BEEP"        , handle_beep_command        , 0)
defcmd("BIND"        , handle_bind_command        , 0)
defcmd("CONNECT"     , handle_connect_command     , 0)
defcmd("CORE"        , handle_core_command        , 0)
defcmd("DC"          , handle_dc_command          , 0)
defcmd("DEF"         , handle_def_command         , 0)
defcmd("DOKEY"       , handle_dokey_command       , 0)
defcmd("EDIT"        , handle_edit_command        , 0)
defcmd("EVAL"        , handle_eval_command        , 1)
defcmd("EXIT"        , handle_exit_command        , 0)
defcmd("EXPORT"      , handle_export_command      , 0)
defcmd("FEATURES"    , handle_features_command    , 0)
defcmd("FG"          , handle_fg_command          , 0)
defcmd("GAG"         , handle_gag_command         , 0)
defcmd("HELP"        , handle_help_command        , 0)
defcmd("HILITE"      , handle_hilite_command      , 0)
defcmd("HISTSIZE"    , handle_histsize_command    , 0)
defcmd("HOOK"        , handle_hook_command        , 0)
defcmd("INPUT"       , handle_input_command       , 0)
defcmd("KILL"        , handle_kill_command        , 0)
defcmd("LCD"         , handle_lcd_command         , 0)
defcmd("LET"         , handle_let_command         , 0)
defcmd("LIMIT"       , handle_limit_command       , 0)
defcmd("LIST"        , handle_list_command        , 0)
defcmd("LISTSOCKETS" , handle_listsockets_command , 0)
defcmd("LISTSTREAMS" , handle_liststreams_command , 0)
defcmd("LISTVAR"     , handle_listvar_command     , 0)
defcmd("LISTWORLDS"  , handle_listworlds_command  , 0)
defcmd("LOAD"        , handle_load_command        , 0)
defcmd("LOCALECHO"   , handle_localecho_command   , 0)
defcmd("LOG"         , handle_log_command         , 0)
defcmd("PS"          , handle_ps_command          , 0)
defcmd("PURGE"       , handle_purge_command       , 0)
defcmd("QUIT"        , handle_quit_command        , 0)
defcmd("QUOTE"       , handle_quote_command       , 0)
defcmd("RECALL"      , handle_recall_command      , 0)
defcmd("RECORDLINE"  , handle_recordline_command  , 0)
defcmd("RELIMIT"     , handle_relimit_command     , 0)
defcmd("REPEAT"      , handle_repeat_command      , 0)
defcmd("RESTRICT"    , handle_restrict_command    , 0)
defcmd("SAVE"        , handle_save_command        , 0)
defcmd("SAVEWORLD"   , handle_saveworld_command   , 0)
defcmd("SET"         , handle_set_command         , 0)
defcmd("SETENV"      , handle_setenv_command      , 0)
defcmd("SH"          , handle_sh_command          , 0)
defcmd("SHIFT"       , handle_shift_command       , 0)
defcmd("STATUS_ADD"  , handle_status_add_command  , 0)
defcmd("STATUS_EDIT" , handle_status_edit_command , 0)
defcmd("STATUS_RM"   , handle_status_rm_command   , 0)
defcmd("SUSPEND"     , handle_suspend_command     , 0)
defcmd("TRIGGER"     , handle_trigger_command     , 0)  
defcmd("TRIGPC"      , handle_trigpc_command      , 0)
defcmd("UNBIND"      , handle_unbind_command      , 0)
defcmd("UNDEF"       , handle_undef_command       , 0)
defcmd("UNDEFN"      , handle_undefn_command      , 0)
defcmd("UNLIMIT"     , handle_unlimit_command     , 0)
defcmd("UNSET"       , handle_unset_command       , 0)
defcmd("UNWORLD"     , handle_unworld_command     , 0)
defcmd("VERSION"     , handle_version_command     , 0)
defcmd("WATCHDOG"    , handle_watchdog_command    , 0)
defcmd("WATCHNAME"   , handle_watchname_command   , 0)

#undef defcmd
#undef defvarcmd
