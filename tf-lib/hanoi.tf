;;;; Towers of Hanoi
; syntax:  /hanoi <n>
; Solves the classic Towers of Hanoi problem for <n> disks.
; Not very useful except as an example of recursion, and demonstrating
; tf's similarity to a shell scripting language.

/def hanoi = /do_hanoi %{1-0} 1 3 2

/def do_hanoi = \
    /if /@test {1} > 0%; /then \
        /do_hanoi $[{1} - 1] %2 %4 %3%;\
        :moves a disk from post %2 to %3%;\
        /do_hanoi $[{1} - 1] %4 %3 %2%;\
    /endif

;;; For comparison, here's the same algorithm in Bash:

; hanoi() { do_hanoi ${1-0} 1 3 2; }
;
; do_hanoi() {
;     if test $1 -gt 0; then
;         do_hanoi $[$1 - 1] $2 $4 $3
;         echo You move a disk from post $2 to $3
;         do_hanoi $[$1 - 1] $4 $3 $2
;     fi
; }

