// SNES 65816 CPU Test MOV (Block Move) demo by krom (Peter Lemon):
arch snes.cpu
output "CPUMOV.sfc", create

macro seek(variable offset) {
  origin ((offset & $7F0000) >> 1) | (offset & $7FFF)
  base offset
}

macro PrintText(SRC, DEST, SIZE) { // Print Text Characters To VRAM
  stz.w REG_VMAIN    // Set Increment VRAM Address After Accessing Lo Byte ($2115: Video Port Control)
  ldx.w #{DEST} >> 1 // Set VRAM Destination
  stx.w REG_VMADDL   // $2116: VRAM

  ldx.w #0 // X = 0      Number Of Text Characters To Print
  {#}LoopText:
    lda.w {SRC},x // A = Text Data
    sta.w REG_VMDATAL // Store Text To VRAM Lo Byte
    inx // X++
    cpx.w #{SIZE}
    bne {#}LoopText // IF (X != 0) Loop Text Characters
}

macro PrintValue(SRC, DEST, SIZE) { // Print HEX Characters To VRAM
  stz.w REG_VMAIN    // Set Increment VRAM Address After Accessing Lo Byte ($2115: Video Port Control)
  ldx.w #{DEST} >> 1 // Set VRAM Destination
  stx.w REG_VMADDL   // $2116: VRAM Address

  lda.b #$24 // A = "$"
  sta.w REG_VMDATAL // Store Text To VRAM Lo Byte

  ldx.w #{SIZE} // X = Number Of Hex Characters To Print

  {#}LoopHEX:
    dex // X--
    ldy.w #0002 // Y = 2 (Char Count)

    lda.w {SRC},x // A = Result Data
    lsr // A >>= 4
    lsr
    lsr
    lsr // A = Result Hi Nibble

    {#}LoopChar:
      cmp.b #10 // Compare Hi Nibble To 9
      clc // Clear Carry Flag
      bpl {#}HexLetter
      adc.b #$30 // Add Hi Nibble To ASCII Numbers
      sta.w REG_VMDATAL // Store Text To VRAM Lo Byte
      bra {#}HexEnd
      {#}HexLetter:
      adc.b #$37 // Add Hi Nibble To ASCII Letters
      sta.w REG_VMDATAL // Store Text To VRAM Lo Byte
      {#}HexEnd:
  
      lda.w {SRC},x // A = Result Data
      and.b #$F // A = Result Lo Nibble
      dey // Y--
      bne {#}LoopChar // IF (Char Count != 0) Loop Char

    cpx.w #0 // Compare X To 0
    bne {#}LoopHEX // IF (X != 0) Loop Hex Characters
}

macro PrintData(SRC, DEST, SIZE) { // Print HEX Characters To VRAM
  stz.w REG_VMAIN    // Set Increment VRAM Address After Accessing Lo Byte ($2115: Video Port Control)
  ldx.w #{DEST} >> 1 // Set VRAM Destination
  stx.w REG_VMADDL   // $2116: VRAM Address

  lda.b #$24 // A = "$"
  sta.w REG_VMDATAL // Store Text To VRAM Lo Byte

  ldx.w #$0000 // X = Number Of Hex Characters To Print

  {#}LoopHEX:
    ldy.w #0002 // Y = 2 (Char Count)

    lda.w {SRC},x // A = Result Data
    lsr // A >>= 4
    lsr
    lsr
    lsr // A = Result Hi Nibble

    {#}LoopChar:
      cmp.b #10 // Compare Hi Nibble To 9
      clc // Clear Carry Flag
      bpl {#}HexLetter
      adc.b #$30 // Add Hi Nibble To ASCII Numbers
      sta.w REG_VMDATAL // Store Text To VRAM Lo Byte
      bra {#}HexEnd
      {#}HexLetter:
      adc.b #$37 // Add Hi Nibble To ASCII Letters
      sta.w REG_VMDATAL // Store Text To VRAM Lo Byte
      {#}HexEnd:
  
      lda.w {SRC},x // A = Result Data
      and.b #$F // A = Result Lo Nibble
      dey // Y--
      bne {#}LoopChar // IF (Char Count != 0) Loop Char

    inx // X++
    cpx.w #{SIZE} // Compare X To SIZE
    bne {#}LoopHEX // IF (X != 0) Loop Hex Characters
}

seek($8000); fill $8000 // Fill Upto $7FFF (Bank 0) With Zero Bytes
include "LIB/SNES.INC"        // Include SNES Definitions
include "LIB/SNES_HEADER.ASM" // Include Header & Vector Table
include "LIB/SNES_GFX.INC"    // Include Graphics Macros

// Variable Data
seek(WRAM) // 8Kb WRAM Mirror ($0000..$1FFF)
ResultDataA:
  dw 0 // Result Data Word
ResultDataX:
  dw 0 // Result Data Word
ResultDataY:
  dw 0 // Result Data Word
ResultDataMVN:
  fill 64 // Result Data (64 Byte Block)
ResultDataMVP:
  fill 64 // Result Data (64 Byte Block)

seek($8000); Start:
  SNES_INIT(SLOWROM) // Run SNES Initialisation Routine

  LoadPAL(BGPAL, $00, 4, 0) // Load BG Palette Data
  LoadLOVRAM(BGCHR, $0000, $3F8, 0) // Load 1BPP Tiles To VRAM Lo Bytes (Converts To 2BPP Tiles)
  ClearVRAM(BGCLEAR, $F800, $400, 0) // Clear VRAM Map To Fixed Tile Word

  // Setup Video
  lda.b #%00001000 // DCBAPMMM: M = Mode, P = Priority, ABCD = BG1,2,3,4 Tile Size
  sta.w REG_BGMODE // $2105: BG Mode 0, Priority 1, BG1 8x8 Tiles

  // Setup BG1 256 Color Background
  lda.b #%11111100  // AAAAAASS: S = BG Map Size, A = BG Map Address
  sta.w REG_BG1SC   // $2108: BG1 32x32, BG1 Map Address = $3F (VRAM Address / $400)
  lda.b #%00000000  // BBBBAAAA: A = BG1 Tile Address, B = BG2 Tile Address
  sta.w REG_BG12NBA // $210B: BG1 Tile Address = $0 (VRAM Address / $1000)

  lda.b #%00000001 // Enable BG1
  sta.w REG_TM // $212C: BG1 To Main Screen Designation

  stz.w REG_BG1HOFS // Store Zero To BG1 Horizontal Scroll Pos Low Byte
  stz.w REG_BG1HOFS // Store Zero To BG1 Horizontal Scroll Pos High Byte
  stz.w REG_BG1VOFS // Store Zero To BG1 Vertical Scroll Pos Low Byte
  stz.w REG_BG1VOFS // Store Zero To BG1 Vertical Pos High Byte

  lda.b #$F // Turn On Screen, Maximum Brightness
  sta.w REG_INIDISP // $2100: Screen Display

  WaitNMI() // Wait For VSync

  // Print Title Text
  PrintText(Title, $F882, 26) // Load Text To VRAM Lo Bytes

  // Print Page Break Text
  PrintText(PageBreak, $F8C2, 30) // Load Text To VRAM Lo Bytes

  // Print Syntax/Opcode Text
  PrintText(MOVMVN, $F902, 26) // Load Text To VRAM Lo Bytes

  // Print Key Text
  PrintText(Key, $F982, 30) // Load Text To VRAM Lo Bytes

  // Print Page Break Text
  PrintText(PageBreak, $F9C2, 30) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y

  // Run Test
  lda.w #$003F // A = Length (64 Bytes)
  ldx.w #MOVSRCData // X = Source
  ldy.w #ResultDataMVN // Y = Destination
  mvn $00=$00 // Block Move 64 Bytes 

  // Store Result Data
  sta.b ResultDataA // Store Result To Memory
  stx.b ResultDataX // Store Result To Memory
  sty.b ResultDataY // Store Result To Memory
  sep #$20 // Set 8-Bit Accumulator

  // Print Result
  PrintValue(ResultDataA, $FA24, 2) // Print Result Data

  // Check Result Data
  ldx.b ResultDataA // X = Result Data
  cpx.w MOVResultCheckA
  beq Pass1
  Fail1:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail1
  Pass1:
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  // Print Result
  PrintValue(ResultDataX, $FA64, 2) // Print Result Data

  // Check Result Data
  ldx.b ResultDataX // X = Result Data
  cpx.w MOVResultCheckB
  beq Pass2
  Fail2:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail2
  Pass2:
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  // Print Result
  PrintValue(ResultDataY, $FAA4, 2) // Print Result Data

  // Check Result Data
  ldx.b ResultDataY // X = Result Data
  cpx.w MOVResultCheckC
  beq Pass3
  Fail3:
    PrintText(Fail, $FAB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail3
  Pass3:
    PrintText(Pass, $FAB2, 4) // Load Text To VRAM Lo Bytes

  WaitNMI() // Wait For VSync

  // Print Result
  PrintData(ResultDataMVN, $FB02, 8)    // Print Result Data
  PrintData(ResultDataMVN+8, $FB42, 8)  // Print Result Data
  PrintData(ResultDataMVN+16, $FB82, 8) // Print Result Data
  PrintData(ResultDataMVN+24, $FBC2, 8) // Print Result Data
  PrintData(ResultDataMVN+32, $FC02, 8) // Print Result Data
  PrintData(ResultDataMVN+40, $FC42, 8) // Print Result Data
  PrintData(ResultDataMVN+48, $FC82, 8) // Print Result Data
  PrintData(ResultDataMVN+56, $FCC2, 8) // Print Result Data

  // Check Result Data
  ldx.w #$0000 // X = Result Data Index
  LoopResult4:
    lda.b ResultDataMVN,x // A = Result Data
    cmp.w MOVSRCData,x
    bne Fail4
    inx // X++
    cpx.w #$0040 // Compare Result Data Index to 64
    bne LoopResult4 // IF (Result Data Index != 64) Loop Result
    beq Pass4
  Fail4:
    PrintText(Fail, $FCF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail4
  Pass4:
    PrintText(Pass, $FCF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $180, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(MOVMVP, $F902, 26) // Load Text To VRAM Lo Bytes
  
  /////////////////////////////////////////////////////////////////
  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y

  // Run Test
  lda.w #$003F // A = Length (64 Bytes)
  ldx.w #MOVSRCData+$3F // X = Source
  ldy.w #ResultDataMVP+$3F // Y = Destination
  mvp $00=$00 // Block Move 64 Bytes 

  // Store Result Data
  sta.b ResultDataA // Store Result To Memory
  stx.b ResultDataX // Store Result To Memory
  sty.b ResultDataY // Store Result To Memory
  sep #$20 // Set 8-Bit Accumulator

  // Print Result
  PrintValue(ResultDataA, $FA24, 2) // Print Result Data

  // Check Result Data
  ldx.b ResultDataA // X = Result Data
  cpx.w MOVResultCheckA
  beq Pass5
  Fail5:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail5
  Pass5:
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  // Print Result
  PrintValue(ResultDataX, $FA64, 2) // Print Result Data

  // Check Result Data
  ldx.b ResultDataX // X = Result Data
  cpx.w MOVResultCheckE
  beq Pass6
  Fail6:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail6
  Pass6:
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  // Print Result
  PrintValue(ResultDataY, $FAA4, 2) // Print Result Data

  // Check Result Data
  ldx.b ResultDataY // X = Result Data
  cpx.w MOVResultCheckF
  beq Pass7
  Fail7:
    PrintText(Fail, $FAB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail7
  Pass7:
    PrintText(Pass, $FAB2, 4) // Load Text To VRAM Lo Bytes

  WaitNMI() // Wait For VSync

  // Print Result
  PrintData(ResultDataMVP, $FB02, 8)    // Print Result Data
  PrintData(ResultDataMVP+8, $FB42, 8)  // Print Result Data
  PrintData(ResultDataMVP+16, $FB82, 8) // Print Result Data
  PrintData(ResultDataMVP+24, $FBC2, 8) // Print Result Data
  PrintData(ResultDataMVP+32, $FC02, 8) // Print Result Data
  PrintData(ResultDataMVP+40, $FC42, 8) // Print Result Data
  PrintData(ResultDataMVP+48, $FC82, 8) // Print Result Data
  PrintData(ResultDataMVP+56, $FCC2, 8) // Print Result Data

  // Check Result Data
  ldx.w #$0000 // X = Result Data Index
  LoopResult8:
    lda.b ResultDataMVP,x // A = Result Data
    cmp.w MOVSRCData,x
    bne Fail8
    inx // X++
    cpx.w #$0040 // Compare Result Data Index to 64
    bne LoopResult8 // IF (Result Data Index != 64) Loop Result
    beq Pass8
  Fail8:
    PrintText(Fail, $FCF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail8
  Pass8:
    PrintText(Pass, $FCF2, 4) // Load Text To VRAM Lo Bytes

Loop:
  jmp Loop

Title:
  db "CPU Test MOV (Block Move):"

PageBreak:
  db "------------------------------"

Key:
  db "Result Block    | AXY | Test |"

Fail:
  db "FAIL"
Pass:
  db "PASS"

MOVMVN:
  db "MVN src,dest (Opcode: $54)"
MOVMVP:
  db "MVP src,dest (Opcode: $44)"

MOVResultCheckA:
  dw $FFFF
MOVResultCheckB:
  dw $8B04
MOVResultCheckC:
  dw $0046
MOVResultCheckD:
  dw $0086
MOVResultCheckE:
  dw $8AC3
MOVResultCheckF:
  dw $0045

MOVSRCData:
  db $01,$02,$03,$04,$05,$06,$07,$08
  db $09,$10,$11,$12,$13,$14,$15,$16
  db $17,$18,$19,$20,$21,$22,$23,$24
  db $25,$26,$27,$28,$29,$30,$31,$32
  db $33,$34,$35,$36,$37,$38,$39,$40
  db $41,$42,$43,$44,$45,$46,$47,$48
  db $49,$50,$51,$52,$53,$54,$55,$56
  db $57,$58,$59,$60,$61,$62,$63,$64

BGCHR:
  include "Font8x8.asm" // Include BG 1BPP 8x8 Tile Font Character Data (1016 Bytes)
BGPAL:
  dw $7800, $7FFF // Blue / White Palette (4 Bytes)
BGCLEAR:
  dw $0020 // BG Clear Character Space " " Fixed Word