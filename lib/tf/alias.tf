;;; Aliases.
; Create command aliases.  Like simple macros, but no leading '/' is required.
; syntax:  /alias <name> <command...>
; syntax:  /unalias <name>

/loaded __TFLIB__/alias.tf

/if ( alias =~ "old" ) \
    /echo -e Note: you have alias=old, so argument substitutions will follow \
        the old style, where %%1 is the alias name, %%2 is the first \
        argument, etc.%;\
/endif

/def -i alias = \
    /if ( {#} < 2 ) \
        /quote -S /~listalias `/@list -s -I -mglob ~alias_body_%{1-*}%; \
    /else \
        /def -i ~alias_body_%1 = %-1%;\
;       The ~alias_call_* macro /shifts unless [alias=~"old"] at runtime.
        /def -i -ag -mglob -h"send {%1}*" ~alias_call_%1 = \
            /shift $$[alias !~ "old"]%%; \
            /~alias_body_%1 %%*%; \
    /endif

/def -i ~listalias = /echo /alias $[substr({L}, 12)] ${%{L}}

/def -i unalias = \
    /if /ismacro ~alias_call_%1%; /then \
        /undef ~alias_call_%1%; \
        /undef ~alias_body_%1%; \
    /else \
        /echo -e - %% %0: "%1": no such alias%; \
    /endif

/def -i purgealias = \
    /purge -I ~alias_call_*%; \
    /purge -I ~alias_body_*
