; Z80 test - officially documented flags, flags only version.
;
; Copyright (C) 2012 Patrik Rak (patrik@raxoft.cz)
;
; This source code is released under the MIT license, see included license.txt.

            macro       testname
            db          "doc flags"
            endm

maskflags   equ         1
onlyflags   equ         1
postccf     equ         0
memptr      equ         0

            include     main.asm

; EOF ;
