
; ******** Source: suite-a.asm
     1                          !cpu 65816
     2                          
     3                          ; 2017-12-13 J.E. Klasek j+816 AT klasek DOT at
     4                          
     5                          
     6                          
     7                          videoram = $0400
     8                          colorram = $d800
     9                          
    10                          ;-------------------------------------------------------------------------------
    11                          	*=$07ff
    12  07ff 0108               	!word $0801
    13  0801 0b08               	!word bend
    14  0803 0a00               	!word 10
    15  0805 9e                 	!byte $9e
    16  0806 3230363100         	!text "2061", 0
    17  080b 0000               bend:       !word 0
    18                          ;-------------------------------------------------------------------------------
    19                          
    20  080d 78                 	sei
    21  080e a917               	lda #$17
    22  0810 8d18d0             	sta $d018
    23  0813 a935               	lda #$35
    24  0815 8501               	sta $01
    25  0817 a97f               	lda #$7f
    26  0819 8d0ddc             	sta $dc0d
    27  081c 8d0ddd             	sta $dd0d
    28  081f ad0ddc             	lda $dc0d
    29  0822 ad0ddd             	lda $dd0d
    30  0825 a200               	ldx #0
    31                          -
    32  0827 a920               	lda #$20
    33  0829 9d0004             	sta videoram,x
    34  082c 9d0005             	sta videoram+$0100,x
    35  082f 9d0006             	sta videoram+$0200,x
    36  0832 9d0007             	sta videoram+$0300,x
    37  0835 a901               	lda #1
    38  0837 9d00d8             	sta colorram,x
    39  083a 9d00d9             	sta colorram+$0100,x
    40  083d 9d00da             	sta colorram+$0200,x
    41  0840 9d00db             	sta colorram+$0300,x
    42  0843 e8                 	inx
    43  0844 d0e1               	bne -
    44                          
    45  0846 4c0010             	jmp start
    46                          theend:
    47  0849 e230               	sep #$30                        ; 8-bit for X/Y and A/M
    48                          	!as
    49                          	!rs
    50  084b af100204           	lda $040210
    51  084f c9ff               	cmp #$ff
    52  0851 d00d               	bne error
    53                          	
    54  0853 a905                      	lda #5
    55  0855 8d20d0             	sta $d020
    56  0858 a200                      	ldx #0 ; success
    57  085a 8effd7             	stx $d7ff
    58  085d 4c5d08             	jmp *
    59                          error
    60  0860 8d0004             	sta $0400
    61  0863 af110204           	lda $040211			; failure map (which test failed)
    62  0867 8d0104             	sta $0401
    63  086a a90a               	lda #10
    64  086c 8d20d0             	sta $d020
    65  086f a2ff               	ldx #$ff ; failure
    66  0871 8effd7             	stx $d7ff
    67  0874 4c7408             	jmp *
    68                          
    69                          ;-------------------------------------------------------------------------------
    70                          
    71                          	* = $1000
    72                          start:
    73                          ; EXPECTED FINAL RESULTS: $0210 = FF
    74                          ; (any other number will be the 
    75                          ;  test that failed)
    76                          
    77                          ; initialize:
    78  1000 a900               	lda #$00
    79  1002 8f100204           	sta $040210
    80  1006 8f110204           	sta $040211
    81                          	
    82                          
    83                          test00:
    84                          
    85                          ; setup cpu
    86  100a 18                 	clc
    87  100b fb                 	xce                             ; native mode
    88  100c c230               	rep #$30                        ; 16-bit for X/Y and A/M
    89                          	!al
    90                          	!rl
    91                          
    92                          ; setup registers
    93  100e a90404             	lda #$0404                      ; Data Bank Register register
    94  1011 48                 	pha                             ; akku in 16 bit
    95  1012 ab                 	plb                             ; pull DBR twice
    96  1013 ab                 	plb
    97  1014 a08888             	ldy #$8888                      ; change marker
    98  1017 bb                 	tyx
    99  1018 98                 	tya
   100                          
   101                          ; setup memory
   102  1019 a95555             	lda #$5555                      ; wrap marker
   103  101c 8f878804           	sta $048887                     ; into bank 4, for LDX/LDY
   104  1020 a97777             	lda #$7777                      ; no-wrap marker
   105  1023 8f878805           	sta $058887			; into bank 5, for LDX/LDY
   106                          
   107                          ;---------------------------------------------------------------------
   108                          
   109  1027 9c0000             	stz $0000			; init wrap marker
   110  102a a97777             	lda #$7777                      ; no-wrap marker
   111  102d 8f000005           	sta $050000                     ; to start of bank 5
   112                          
   113  1031 8cffff             	sty $ffff                       ; high byte of Y is where?
   114  1034 ad0000             	lda $0000
   115  1037 d011               	bne +
   116  1039 adffff             	lda $ffff			; fetch, does not wrap
   117  103c c98888             	cmp #$8888
   118  103f d009               	bne +
   119  1041 af000005           	lda $050000
   120  1045 c98877             	cmp #$7788			; write to bank 5
   121  1048 f004               	beq ++
   122  104a ee1002             +	inc $0210			; fail counter
   123  104d 18                 	clc
   124                          ++	
   125  104e 2e1102             	rol $0211			; update failure map
   126                          ;---------------------------------------------------------------------
   127                          
   128  1051 9c0000             	stz $0000			; init wrap marker
   129  1054 a97777             	lda #$7777                      ; no-wrap marker
   130  1057 8f000005           	sta $050000                     ; to start of bank 5
   131                          
   132  105b bb                 	tyx                             ; change marker
   133  105c 8effff             	stx $ffff                       ; high byte of Y is where?
   134  105f ad0000             	lda $0000
   135  1062 d011               	bne +
   136  1064 adffff             	lda $ffff			; fetch, does not wrap
   137  1067 c98888             	cmp #$8888
   138  106a d009               	bne +
   139  106c af000005           	lda $050000
   140  1070 c98877             	cmp #$7788			; write to bank 5
   141  1073 f004               	beq ++
   142  1075 ee1002             +	inc $0210			; fail counter
   143  1078 18                 	clc
   144                          ++	
   145  1079 2e1102             	rol $0211			; update failure map
   146                          ;---------------------------------------------------------------------
   147                          
   148  107c bcffff             	ldy $ffff,x     ; Y=5555  Y=7777  value for Y comes from which bank?
   149  107f c07777             	cpy #$7777
   150  1082 f004               	beq +
   151  1084 ee1002             	inc $0210			; fail counter
   152  1087 18                 	clc
   153                          +
   154  1088 2e1102             	rol $0211			; update failure map
   155                          ;---------------------------------------------------------------------
   156                          
   157  108b 9b                 	txy				; reinitialize y
   158  108c beffff             	ldx $ffff,y     ; X=5555  X=7777  value for X comes from which bank?
   159  108f e07777             	cpx #$7777
   160  1092 f004               	beq +
   161  1094 ee1002             	inc $0210			; fail counter
   162  1097 18                 	clc
   163                          +
   164  1098 2e1102             	rol $0211			; update failure map
   165                          ;---------------------------------------------------------------------
   166                          
   167  109b 9c0000             	stz $0000			; init wrap marker
   168  109e a97777             	lda #$7777                      ; no-wrap marker
   169  10a1 8f000005           	sta $050000                     ; to start of bank 5
   170                          
   171  10a5 a98877             	lda #$7788
   172  10a8 ee0000             	inc $0000			; $0000 = 1
   173  10ab 1cffff             	trb $ffff                       ; 88 77 & ^(88 77) -> 00 00
   174  10ae ad0000             	lda $0000
   175  10b1 c90100             	cmp #$0001			; $0000 not reset by trb (does not wrap)
   176  10b4 d009               	bne +
   177  10b6 af000005           	lda $050000
   178  10ba c90077             	cmp #$7700			; $050001 reset by trb
   179  10bd f004               	beq ++
   180  10bf ee1002             +	inc $0210			; fail counter
   181  10c2 18                 	clc
   182                          ++
   183  10c3 2e1102             	rol $0211			; update failure map
   184                          ;---------------------------------------------------------------------
   185                          
   186  10c6 a98877             	lda #$7788
   187  10c9 8f000005           	sta $050000			; 00 88 | 88 77 -> 88 ff
   188  10cd 0cffff             	tsb $ffff                       ; set bits (which are already cleared)
   189  10d0 ad0000             	lda $0000
   190  10d3 c90100             	cmp #$0001			; $0000 not set by tsb (does not wrap!)
   191  10d6 d009               	bne +
   192  10d8 af000005           	lda $050000
   193  10dc c9ff77             	cmp #$77ff			; $050001 all bits set by tsb
   194  10df f004               	beq ++
   195  10e1 ee1002             +	inc $0210			; fail counter
   196  10e4 18                 	clc
   197                          ++
   198  10e5 2e1102             	rol $0211			; update failure map
   199                          ;---------------------------------------------------------------------
   200                          	
   201                          
   202                          test00pass:
   203  10e8 ad1002             	lda $0210
   204  10eb 49003f             	eor #%0011111100000000		; invert failure map
   205  10ee 8d1002             	sta $0210
   206  10f1 d003               	bne +
   207  10f3 ce1002             	dec $0210			; 0 -> FF
   208                          +	
   209                          
   210  10f6 a90000             	lda #$0000
   211  10f9 48                 	pha
   212  10fa ab                 	plb
   213  10fb ab                 	plb				; program bank = 0
   214  10fc 38                 	sec
   215  10fd fb                 	xce				; emulation mode
   216  10fe e230               	sep #$30			; a/m, x/y 8 bit
   217                          
   218  1100 4c4908                     jmp theend
