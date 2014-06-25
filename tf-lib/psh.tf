;;; preferred shell
;; /psh [<command>]
;; Like /sh, but uses ${SHELL} instead of /bin/sh to execute <command>.
;; bug:  barfs on suspend (^Z) because SHELL is really a child of /bin/sh,
;; which doesn't have job control.  Workaround:  don't ^Z during /psh.

/loaded __TFLIB__/psh.tf

/def -i psh = \
    /if ( ({#} > 0) & (SHELL !~ "") & (SHELL !~ "/bin/sh") ) \
        /setenv ARGS=%*%;\
        /def -i -hSHELL -1 -agG ~psh_hook = \
            /echo %% Executing %{SHELL} command: %%{ARGS}%;\
        /@sh exec %{SHELL} -i -c "$$ARGS"%;\
    /else \
        /@sh %*%;\
    /endif
