;;; spelling checker
;;; This lets you type ESC-s to check the spelling of the current line.
;;; If any misspellings are found, you will be told.
;;; This requires the "spell" utility on your local system.

/loaded __TFLIB__/spell.tf

/def -i spell_line = \
    /setenv ARGS=$(/recall -i 1)%; \
    /let _errs=$(/quote -S -decho !echo $$ARGS | spell)%; \
    /if ( _errs !~ "" ) \
        /echo MISSPELLINGS: %_errs%; \
    /else \
        /echo No misspellings found.%; \
    /endif%; \
    /@test _errs =~ ""

/def -ib^[s = /spell_line

