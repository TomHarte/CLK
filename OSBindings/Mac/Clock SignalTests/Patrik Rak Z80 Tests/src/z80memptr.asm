; Z80 test - MEMPTR version.
;
; However note that the current set of tests was not designed to stress test MEMPTR
; particularly, so it doesn't detect much - I may eventually add such specific tests later.
;
; Copyright (C) 2012 Patrik Rak (patrik@raxoft.cz)
;
; This source code is released under the MIT license, see included license.txt.

            macro       testname
            db          "MEMPTR"
            endm

maskflags   equ         0
onlyflags   equ         1
postccf     equ         0
memptr      equ         1

            include     main.asm

; EOF ;
