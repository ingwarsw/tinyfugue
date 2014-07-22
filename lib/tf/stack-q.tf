;;;; Stack and queue macros.
;;;; Lets you keep stacks and queues of words.  /pop and /dequeue give their
;;;; results via /echo; use $(/pop) and $(/dequeue) to capture their results.

/loaded __TFLIB__/stack-q.tf

/require lisp.tf

;;; /push <word> [<stackname>]
; push word onto stack.

/def -i push = /@test %{2-stack} := strcat({1}, " ", %{2-stack})

;;; /pop [<stackname>]
; get and remove top word from stack.

/def -i pop = \
    /eval /car %%{%{1-stack}}%; \
    /@test %{1-stack} := $$(/cdr %%{%{1-stack}})

;;; /enqueue <word> [<queuename>]
; put word on queue.

/def -i enqueue = /@test %{2-queue} := strcat(%{2-queue}, " ", {1})

;;; /dequeue [<queuename>]
; get and remove first word from queue.

/def -i dequeue = /pop %1-queue

