;;;; Quoting utilities
;;;
;;; /qdef   [<prefix>] <name>	- quote a (current) macro definition
;;; /qmac   [<prefix>] <name>	- quote a macro from a macro file (requires
;;;				  bourne shell, awk, ls)
;;; /qworld [<prefix>] <name>	- quote a world definition
;;; /qfile  [<prefix>] <name>	- quote a file
;;; /qtf    <cmd>		- quote a tf command
;;; /qsh    <cmd>		- quote a shell command
;;; /qmud   [-w<world>] <cmd>	- quote a mud command (requires OUTPUTPREFIX
;;;				  and OUTPUTSUFFIX on the mud)
;;;
;;; <prefix> is prepended to each generated line.  The default prefix is ":|",
;;; but can be changed in /qdef, /qmac, /qworld, and /qfile.

/loaded __TFLIB__/quoter.tf

/require lisp.tf
/require pcmd.tf

/def -i _qdef = /send - %_prefix %-2
/def -i qdef = /let _prefix=%{-L-%{qdef_prefix-:|}}%; /quote -S /_qdef `/@list -i %{L-@}

/def -i ~qmac_files = \
    /echo %{HOME}/.tfrc%; \
    /echo *.tf%; \
    /echo tiny.*%; \
    /while ({#}) /echo %{1}/*.tf%; /shift%; /done

; On some systems, nawk works better.
/set _qmac_awk=awk

/def -i qmac = \
  /setenv prog=\
      /^\\/def.* %L[ 	]*=/ { f = 1; } \
      { if (f) print \$0; } \
      /^[^;].*[^\\\\]\$/ { f = 0; }%;\
  /eval /quote -S %{-L-%{qmac_prefix-:|}} !\
      %{_qmac_awk} "\\\$prog" `ls $(/~qmac_files %TFLIBDIR %TFPATH) 2>/dev/null`

/def -i qworld = /quote -S %{-L-%{qworld_prefix-:|}} `/@listworlds %{L-@}

/def -i qfile = /quote -S %{-L-%{qfile_prefix-:|}} '%{L-@}

/def -i qtf = \
    %{qtf_prefix-:|}` %*%; /quote -s0 -S %{qtf_prefix-:|} `%*

/def -i qsh = %{qsh_prefix-:|}! %*%; /quote -S %{qsh_prefix-:|} !%*

/def -i qmud = \
    /let _opts=%; \
    /while ( {1} =/ "-[^- ]*" ) \
        /let _opts=%_opts %1%; \
        /shift%; \
    /done%; \
    /let _dest=${world_name}%; \
    /def %{_opts} -iqp5000 -msimple -t"%{outputprefix}" -1 -aGg qmud_pre = \
        /send -w%{_dest} %{qmud_prefix-:|} $${world_name}> %*%%; \
        /def %{_opts} -iqp5001 -mglob -t"*" -aGg qmud_all = \
            /send -w%{_dest} %{qmud_prefix-:|} %%%*%%; \
        /def %{_opts} -iqp5002 -msimple -t"%{outputsuffix}" -1 -aGg qmud_suf = \            /undef qmud_all%; \
    /pcmd %{_opts} %*

