; Z80 test - the full version.
;
; Copyright (C) 2012 Patrik Rak (patrik@raxoft.cz)
;
; This source code is released under the MIT license, see included license.txt.

            macro       testname
            db          "full"
            endm

maskflags   equ         0
onlyflags   equ         0
postccf     equ         0
memptr      equ         0

            include     main.asm

; EOF ;
