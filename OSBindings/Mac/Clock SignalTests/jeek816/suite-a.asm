!cpu 65816

; 2017-12-13 J.E. Klasek j+816 AT klasek DOT at



videoram = $0400
colorram = $d800

;-------------------------------------------------------------------------------
	*=$07ff
	!word $0801
	!word bend
	!word 10
	!byte $9e
	!text "2061", 0
bend:       !word 0
;-------------------------------------------------------------------------------

	sei
	lda #$17
	sta $d018
	lda #$35
	sta $01
	lda #$7f
	sta $dc0d
	sta $dd0d
	lda $dc0d
	lda $dd0d
	ldx #0
-
	lda #$20
	sta videoram,x
	sta videoram+$0100,x
	sta videoram+$0200,x
	sta videoram+$0300,x
	lda #1
	sta colorram,x
	sta colorram+$0100,x
	sta colorram+$0200,x
	sta colorram+$0300,x
	inx
	bne -

	jmp start
theend:
	sep #$30                        ; 8-bit for X/Y and A/M
	!as
	!rs
	lda $040210
	cmp #$ff
	bne error
	
       	lda #5
	sta $d020
       	ldx #0 ; success
	stx $d7ff
	jmp *
error
	sta $0400
	lda $040211			; failure map (which test failed)
	sta $0401
	lda #10
	sta $d020
	ldx #$ff ; failure
	stx $d7ff
	jmp *

;-------------------------------------------------------------------------------

	* = $1000
start:
; EXPECTED FINAL RESULTS: $0210 = FF
; (any other number will be the 
;  test that failed)

; initialize:
	lda #$00
	sta $040210
	sta $040211
	

test00:

; setup cpu
	clc
	xce                             ; native mode
	rep #$30                        ; 16-bit for X/Y and A/M
	!al
	!rl

; setup registers
	lda #$0404                      ; Data Bank Register register
	pha                             ; akku in 16 bit
	plb                             ; pull DBR twice
	plb
	ldy #$8888                      ; change marker
	tyx
	tya

; setup memory
	lda #$5555                      ; wrap marker
	sta $048887                     ; into bank 4, for LDX/LDY
	lda #$7777                      ; no-wrap marker
	sta $058887			; into bank 5, for LDX/LDY

;---------------------------------------------------------------------

	stz $0000			; init wrap marker
	lda #$7777                      ; no-wrap marker
	sta $050000                     ; to start of bank 5

	sty $ffff                       ; high byte of Y is where?
	lda $0000
	bne +
	lda $ffff			; fetch, does not wrap
	cmp #$8888
	bne +
	lda $050000
	cmp #$7788			; write to bank 5
	beq ++
+	inc $0210			; fail counter
	clc
++	
	rol $0211			; update failure map
;---------------------------------------------------------------------

	stz $0000			; init wrap marker
	lda #$7777                      ; no-wrap marker
	sta $050000                     ; to start of bank 5

	tyx                             ; change marker
	stx $ffff                       ; high byte of Y is where?
	lda $0000
	bne +
	lda $ffff			; fetch, does not wrap
	cmp #$8888
	bne +
	lda $050000
	cmp #$7788			; write to bank 5
	beq ++
+	inc $0210			; fail counter
	clc
++	
	rol $0211			; update failure map
;---------------------------------------------------------------------

	ldy $ffff,x     ; Y=5555  Y=7777  value for Y comes from which bank?
	cpy #$7777
	beq +
	inc $0210			; fail counter
	clc
+
	rol $0211			; update failure map
;---------------------------------------------------------------------

	txy				; reinitialize y
	ldx $ffff,y     ; X=5555  X=7777  value for X comes from which bank?
	cpx #$7777
	beq +
	inc $0210			; fail counter
	clc
+
	rol $0211			; update failure map
;---------------------------------------------------------------------

	stz $0000			; init wrap marker
	lda #$7777                      ; no-wrap marker
	sta $050000                     ; to start of bank 5

	lda #$7788
	inc $0000			; $0000 = 1
	trb $ffff                       ; 88 77 & ^(88 77) -> 00 00
	lda $0000
	cmp #$0001			; $0000 not reset by trb (does not wrap)
	bne +
	lda $050000
	cmp #$7700			; $050001 reset by trb
	beq ++
+	inc $0210			; fail counter
	clc
++
	rol $0211			; update failure map
;---------------------------------------------------------------------

	lda #$7788
	sta $050000			; 00 88 | 88 77 -> 88 ff
	tsb $ffff                       ; set bits (which are already cleared)
	lda $0000
	cmp #$0001			; $0000 not set by tsb (does not wrap!)
	bne +
	lda $050000
	cmp #$77ff			; $050001 all bits set by tsb
	beq ++
+	inc $0210			; fail counter
	clc
++
	rol $0211			; update failure map
;---------------------------------------------------------------------
	

test00pass:
	lda $0210
	eor #%0011111100000000		; invert failure map
	sta $0210
	bne +
	dec $0210			; 0 -> FF
+	

	lda #$0000
	pha
	plb
	plb				; program bank = 0
	sec
	xce				; emulation mode
	sep #$30			; a/m, x/y 8 bit

        jmp theend
