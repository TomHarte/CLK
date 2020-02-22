; Simple printing module.
;
; Copyright (C) 2012 Patrik Rak (patrik@raxoft.cz)
;
; This source code is released under the MIT license, see included license.txt.


printinit:  ld      a,2
            jp      0x1601      ; CHAN-OPEN


print:      ex      (sp),hl
            call    printhl
            ex      (sp),hl
            ret

printhl:
.loop       ld      a,(hl)
            inc     hl
            or      a
            ret     z
            call    printchr
            jr      .loop


printdeca:  ld      h,a
            ld      b,-100
            call    .digit
            ld      b,-10
            call    .digit
            ld      b,-1

.digit      ld      a,h
            ld      l,'0'-1
.loop       inc     l
            add     a,b
            jr      c,.loop
            sub     b
            ld      h,a
            ld      a,l
            jr      printchr


printcrc:   ld      b,4

printhexs:
.loop       ld      a,(hl)
            inc     hl
            call    printhexa
            djnz    .loop
            ret


printhexa:  push    af
            rrca
            rrca
            rrca
            rrca
            call    .nibble
            pop     af

.nibble     or      0xf0
            daa
            add     a,0xa0
            adc     a,0x40

printchr:   push    iy
            ld      iy,0x5c3a   ; ERR-NR
            push    de
            push    bc
            exx
            ei
            ; out     (0xff),a
            rst     0x10
            di
            exx
            pop     bc
            pop     de
            pop     iy
            ret

; EOF ;
