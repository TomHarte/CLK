// SNES 65816 CPU Test TRN (Transfer) demo by krom (Peter Lemon):
arch snes.cpu
output "CPUTRN.sfc", create

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

macro PrintPSR(SRC, DEST) { // Print Processor Status Flags To VRAM
  stz.w REG_VMAIN    // Set Increment VRAM Address After Accessing Lo Byte ($2115: Video Port Control)
  ldx.w #{DEST} >> 1 // Set VRAM Destination
  stx.w REG_VMADDL   // $2116: VRAM Address

  lda.b #%10000000 // A = Negative Flag Bit
  jsr {#}PSRFlagTest // Test PSR Flag Data

  lda.b #%01000000 // A = Overflow Flag Bit
  jsr {#}PSRFlagTest // Test PSR Flag Data

  lda.b #%00000010 // A = Zero Flag Bit
  jsr {#}PSRFlagTest // Test PSR Flag Data

  lda.b #%00000001 // A = Carry Flag Bit
  jsr {#}PSRFlagTest // Test PSR Flag Data

  bra {#}PSREnd

  {#}PSRFlagTest:
    bit.b {SRC} // Test Processor Status Flag Data Bit
    bne {#}PSRFlagSet
    lda.b #$30 // A = "0"
    sta.w REG_VMDATAL // Store Text To VRAM Lo Byte
    rts // Return From Subroutine
    {#}PSRFlagSet:
    lda.b #$31 // A = "1"
    sta.w REG_VMDATAL // Store Text To VRAM Lo Byte
    rts // Return From Subroutine

  {#}PSREnd:
}

seek($8000); fill $8000 // Fill Upto $7FFF (Bank 0) With Zero Bytes
include "LIB/SNES.INC"        // Include SNES Definitions
include "LIB/SNES_HEADER.ASM" // Include Header & Vector Table
include "LIB/SNES_GFX.INC"    // Include Graphics Macros

// Variable Data
seek(WRAM) // 8Kb WRAM Mirror ($0000..$1FFF)
ResultData:
  dw 0 // Result Data Word
PSRFlagData:
  db 0 // Processor Status Register Flag Data Byte

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
  PrintText(Title, $F882, 24) // Load Text To VRAM Lo Bytes

  // Print Page Break Text
  PrintText(PageBreak, $F8C2, 30) // Load Text To VRAM Lo Bytes

  // Print Syntax/Opcode Text
  PrintText(TRNTAX, $F902, 26) // Load Text To VRAM Lo Bytes

  // Print Key Text
  PrintText(Key, $F982, 30) // Load Text To VRAM Lo Bytes

  // Print Page Break Text
  PrintText(PageBreak, $F9C2, 30) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M8BitX8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  sep #$10 // Set 8-Bit X/Y
  clc // Clear Carry Flag
  clv // Clear Overflow Flag

  // Run Test
  lda.b #$00 // A = $00
  tax // X = A

  // Store Result & Processor Status Flag Data
  stx.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory
  rep #$10 // Set 16-Bit X/Y

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA12, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FA24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w TRNResultCheckA
  beq Pass1
  Fail1:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail1
  Pass1:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckA
    bne Fail1
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M8BitX8Bit, $FA42, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  sep #$10 // Set 8-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  lda.b #$FF // A = $FF
  tax // X = A

  // Store Result & Processor Status Flag Data
  stx.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory
  rep #$10 // Set 16-Bit X/Y

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA52, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FA64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w TRNResultCheckB
  beq Pass2
  Fail2:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail2
  Pass2:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckB
    bne Fail2
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M8BitX16Bit, $FA82, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  lda.w #$0000 // A = $0000
  sep #$20 // Set 8-Bit Accumulator
  tax // X = A

  // Store Result & Processor Status Flag Data
  stx.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA92, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FAA4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w TRNResultCheckC
  beq Pass3
  Fail3:
    PrintText(Fail, $FAB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail3
  Pass3:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckC
    bne Fail3
    PrintText(Pass, $FAB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M8BitX16Bit, $FAC2, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  lda.w #$FFFF // A = $FFFF
  sep #$20 // Set 8-Bit Accumulator
  tax // X = A

  // Store Result & Processor Status Flag Data
  stx.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FAD2, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FAE4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w TRNResultCheckD
  beq Pass4
  Fail4:
    PrintText(Fail, $FAF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail4
  Pass4:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckD
    bne Fail4
    PrintText(Pass, $FAF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M16BitX8Bit, $FB02, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  sep #$10 // Set 8-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  lda.w #$0000 // A = $0000
  tax // X = A

  // Store Result & Processor Status Flag Data
  stx.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory
  rep #$10 // Set 16-Bit X/Y

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB12, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FB24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w TRNResultCheckE
  beq Pass5
  Fail5:
    PrintText(Fail, $FB32, 4) // Load Text To VRAM Lo Bytes
    bra Fail5
  Pass5:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckE
    bne Fail5
    PrintText(Pass, $FB32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M16BitX8Bit, $FB42, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  sep #$10 // Set 8-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  lda.w #$FFFF // A = $FFFF
  tax // X = A

  // Store Result & Processor Status Flag Data
  stx.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory
  rep #$10 // Set 16-Bit X/Y

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB52, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FB64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w TRNResultCheckF
  beq Pass6
  Fail6:
    PrintText(Fail, $FB72, 4) // Load Text To VRAM Lo Bytes
    bra Fail6
  Pass6:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckF
    bne Fail6
    PrintText(Pass, $FB72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M16BitX16Bit, $FB82, 7) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  lda.w #$0000 // A = $0000
  tax // X = A

  // Store Result & Processor Status Flag Data
  stx.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB92, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FBA4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w TRNResultCheckG
  beq Pass7
  Fail7:
    PrintText(Fail, $FBB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail7
  Pass7:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckG
    bne Fail7
    PrintText(Pass, $FBB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M16BitX16Bit, $FBC2, 7) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  lda.w #$FFFF // A = $FFFF
  tax // X = A

  // Store Result & Processor Status Flag Data
  stx.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FBD2, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FBE4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w TRNResultCheckH
  beq Pass8
  Fail8:
    PrintText(Fail, $FBF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail8
  Pass8:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckH
    bne Fail8
    PrintText(Pass, $FBF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $100, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(TRNTAY, $F902, 26) // Load Text To VRAM Lo Bytes
  
  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M8BitX8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  sep #$10 // Set 8-Bit X/Y
  clc // Clear Carry Flag
  clv // Clear Overflow Flag

  // Run Test
  lda.b #$00 // A = $00
  tay // Y = A

  // Store Result & Processor Status Flag Data
  sty.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory
  rep #$10 // Set 16-Bit X/Y

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA12, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FA24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w TRNResultCheckA
  beq Pass9
  Fail9:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail9
  Pass9:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckA
    bne Fail9
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M8BitX8Bit, $FA42, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  sep #$10 // Set 8-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  lda.b #$FF // A = $FF
  tay // Y = A

  // Store Result & Processor Status Flag Data
  sty.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory
  rep #$10 // Set 16-Bit X/Y

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA52, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FA64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w TRNResultCheckB
  beq Pass10
  Fail10:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail10
  Pass10:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckB
    bne Fail10
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M8BitX16Bit, $FA82, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  lda.w #$0000 // A = $0000
  sep #$20 // Set 8-Bit Accumulator
  tay // Y = A

  // Store Result & Processor Status Flag Data
  sty.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA92, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FAA4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w TRNResultCheckC
  beq Pass11
  Fail11:
    PrintText(Fail, $FAB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail11
  Pass11:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckC
    bne Fail11
    PrintText(Pass, $FAB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M8BitX16Bit, $FAC2, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  lda.w #$FFFF // A = $FFFF
  sep #$20 // Set 8-Bit Accumulator
  tay // Y = A

  // Store Result & Processor Status Flag Data
  sty.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FAD2, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FAE4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w TRNResultCheckD
  beq Pass12
  Fail12:
    PrintText(Fail, $FAF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail12
  Pass12:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckD
    bne Fail12
    PrintText(Pass, $FAF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M16BitX8Bit, $FB02, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  sep #$10 // Set 8-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  lda.w #$0000 // A = $0000
  tay // Y = A

  // Store Result & Processor Status Flag Data
  sty.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory
  rep #$10 // Set 16-Bit X/Y

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB12, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FB24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w TRNResultCheckE
  beq Pass13
  Fail13:
    PrintText(Fail, $FB32, 4) // Load Text To VRAM Lo Bytes
    bra Fail13
  Pass13:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckE
    bne Fail13
    PrintText(Pass, $FB32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M16BitX8Bit, $FB42, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  sep #$10 // Set 8-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  lda.w #$FFFF // A = $FFFF
  tay // Y = A

  // Store Result & Processor Status Flag Data
  sty.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory
  rep #$10 // Set 16-Bit X/Y

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB52, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FB64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w TRNResultCheckF
  beq Pass14
  Fail14:
    PrintText(Fail, $FB72, 4) // Load Text To VRAM Lo Bytes
    bra Fail14
  Pass14:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckF
    bne Fail14
    PrintText(Pass, $FB72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M16BitX16Bit, $FB82, 7) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  lda.w #$0000 // A = $0000
  tay // Y = A

  // Store Result & Processor Status Flag Data
  sty.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB92, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FBA4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w TRNResultCheckG
  beq Pass15
  Fail15:
    PrintText(Fail, $FBB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail15
  Pass15:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckG
    bne Fail15
    PrintText(Pass, $FBB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M16BitX16Bit, $FBC2, 7) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  lda.w #$FFFF // A = $FFFF
  tay // Y = A

  // Store Result & Processor Status Flag Data
  sty.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FBD2, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FBE4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w TRNResultCheckH
  beq Pass16
  Fail16:
    PrintText(Fail, $FBF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail16
  Pass16:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckH
    bne Fail16
    PrintText(Pass, $FBF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $100, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(TRNTCD, $F902, 26) // Load Text To VRAM Lo Bytes
  
  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M8BitX16Bit, $FA02, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag
  clv // Clear Overflow Flag

  // Run Test
  lda.w #$0000 // A = $0000
  sep #$20 // Set 8-Bit Accumulator
  tcd // Direct Page Register = A

  // Store Result & Processor Status Flag Data
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory
  phd // Push Direct Page Register To Stack
  plx // Pull X From Stack
  stx.b ResultData // Store Result To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA12, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FA24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w TRNResultCheckC
  beq Pass17
  Fail17:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail17
  Pass17:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckC
    bne Fail17
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M8BitX16Bit, $FA42, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  lda.w #$FFFF // A = $FFFF
  sep #$20 // Set 8-Bit Accumulator
  tcd // Direct Page Register = A

  // Store Result & Processor Status Flag Data
  php // Push Processor Status Register To Stack
  phd // Push Direct Page Register To Stack
  rep #$20 // Set 16-Bit Accumulator
  lda.w #$0000 // A = $0000
  sep #$20 // Set 8-Bit Accumulator
  tcd // Direct Page Register = A
  plx // Pull X From Stack
  stx.b ResultData // Store Result To Memory
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA52, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FA64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w TRNResultCheckD
  beq Pass18
  Fail18:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail18
  Pass18:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckD
    bne Fail18
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M16BitX16Bit, $FA82, 7) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  lda.w #$0000 // A = $0000
  tcd // Direct Page Register = A

  // Store Result & Processor Status Flag Data
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory
  phd // Push Direct Page Register To Stack
  plx // Pull X From Stack
  stx.b ResultData // Store Result To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA92, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FAA4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w TRNResultCheckG
  beq Pass19
  Fail19:
    PrintText(Fail, $FAB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail19
  Pass19:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckG
    bne Fail19
    PrintText(Pass, $FAB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M16BitX16Bit, $FAC2, 7) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  lda.w #$FFFF // A = $FFFF
  tcd // Direct Page Register = A

  // Store Result & Processor Status Flag Data
  php // Push Processor Status Register To Stack
  phd // Push Direct Page Register To Stack
  lda.w #$0000 // A = $0000
  sep #$20 // Set 8-Bit Accumulator
  tcd // Direct Page Register = A
  plx // Pull X From Stack
  stx.b ResultData // Store Result To Memory
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FAD2, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FAE4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w TRNResultCheckH
  beq Pass20
  Fail20:
    PrintText(Fail, $FAF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail20
  Pass20:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckH
    bne Fail20
    PrintText(Pass, $FAF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $80, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(TRNTCS, $F902, 26) // Load Text To VRAM Lo Bytes
  
  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M8BitX16Bit, $FA02, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag
  clv // Clear Overflow Flag

  // Run Test
  lda.w #$0000 // A = $0000
  sep #$20 // Set 8-Bit Accumulator
  tas // Stack Pointer = A
  tsx // X = Stack Pointer
  rep #$20 // Set 16-Bit Accumulator
  lda.w #$1FFF // A = $1FFF
  sep #$20 // Set 8-Bit Accumulator
  tas // Stack Pointer = A

  // Store Result & Processor Status Flag Data
  stx.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA12, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FA24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w TRNResultCheckI
  beq Pass21
  Fail21:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail21
  Pass21:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckI
    bne Fail21
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M8BitX16Bit, $FA42, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  lda.w #$FFFF // A = $FFFF
  sep #$20 // Set 8-Bit Accumulator
  tas // Stack Pointer = A
  tsx // X = Stack Pointer
  rep #$20 // Set 16-Bit Accumulator
  lda.w #$1FFF // A = $1FFF
  sep #$20 // Set 8-Bit Accumulator
  tas // Stack Pointer = A

  // Store Result & Processor Status Flag Data
  stx.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA52, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FA64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w TRNResultCheckJ
  beq Pass22
  Fail22:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail22
  Pass22:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckJ
    bne Fail22
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M16BitX16Bit, $FA82, 7) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  lda.w #$0000 // A = $0000
  tas // Stack Pointer = A
  tsx // X = Stack Pointer
  lda.w #$1FFF // A = $1FFF
  tas // Stack Pointer = A

  // Store Result & Processor Status Flag Data
  stx.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA92, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FAA4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w TRNResultCheckK
  beq Pass23
  Fail23:
    PrintText(Fail, $FAB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail23
  Pass23:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckK
    bne Fail23
    PrintText(Pass, $FAB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M16BitX16Bit, $FAC2, 7) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  lda.w #$FFFF // A = $FFFF
  tas // Stack Pointer = A
  tsx // X = Stack Pointer
  lda.w #$1FFF // A = $1FFF
  tas // Stack Pointer = A

  // Store Result & Processor Status Flag Data
  stx.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FAD2, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FAE4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w TRNResultCheckL
  beq Pass24
  Fail24:
    PrintText(Fail, $FAF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail24
  Pass24:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckL
    bne Fail24
    PrintText(Pass, $FAF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $80, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(TRNTDC, $F902, 26) // Load Text To VRAM Lo Bytes
  
  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M8BitX16Bit, $FA02, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag
  clv // Clear Overflow Flag

  // Run Test
  lda.w #$0000 // A = $0000
  sep #$20 // Set 8-Bit Accumulator
  tcd // Direct Page Register = A
  tdc // A = Direct Page Register

  // Store Result & Processor Status Flag Data
  rep #$20 // Set 16-Bit Accumulator
  sta.b ResultData // Store Result To Memory
  sep #$20 // Set 8-Bit Accumulator
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA12, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FA24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w TRNResultCheckC
  beq Pass25
  Fail25:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail25
  Pass25:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckC
    bne Fail25
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M8BitX16Bit, $FA42, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  lda.w #$FFFF // A = $FFFF
  sep #$20 // Set 8-Bit Accumulator
  tcd // Direct Page Register = A
  tdc // A = Direct Page Register
  rep #$20 // Set 16-Bit Accumulator
  tax // X = A

  // Store Result & Processor Status Flag Data
  sep #$20 // Set 8-Bit Accumulator
  php // Push Processor Status Register To Stack
  rep #$20 // Set 16-Bit Accumulator
  lda.w #$0000 // A = $0000
  sep #$20 // Set 8-Bit Accumulator
  tcd // Direct Page Register = A
  stx.b ResultData // Store Result To Memory
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA52, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FA64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w TRNResultCheckD
  beq Pass26
  Fail26:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail26
  Pass26:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckD
    bne Fail26
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M16BitX16Bit, $FA82, 7) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  lda.w #$0000 // A = $0000
  tcd // Direct Page Register = A
  tdc // A = Direct Page Register

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA92, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FAA4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w TRNResultCheckG
  beq Pass27
  Fail27:
    PrintText(Fail, $FAB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail27
  Pass27:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckG
    bne Fail27
    PrintText(Pass, $FAB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M16BitX16Bit, $FAC2, 7) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  lda.w #$FFFF // A = $FFFF
  tcd // Direct Page Register = A
  tdc // A = Direct Page Register
  tax // X = A

  // Store Result & Processor Status Flag Data
  php // Push Processor Status Register To Stack
  lda.w #$0000 // A = $0000
  sep #$20 // Set 8-Bit Accumulator
  tcd // Direct Page Register = A
  stx.b ResultData // Store Result To Memory
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FAD2, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FAE4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w TRNResultCheckH
  beq Pass28
  Fail28:
    PrintText(Fail, $FAF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail28
  Pass28:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckH
    bne Fail28
    PrintText(Pass, $FAF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $80, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(TRNTSC, $F902, 26) // Load Text To VRAM Lo Bytes
  
  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M8BitX16Bit, $FA02, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag
  clv // Clear Overflow Flag

  // Run Test
  ldx.w #$1FFF // X = $1FFF
  lda.w #$0000 // A = $0000
  sep #$20 // Set 8-Bit Accumulator
  tas // Stack Pointer = A
  tsa // A = Stack Pointer
  txs // Stack Pointer = X

  // Store Result & Processor Status Flag Data
  rep #$20 // Set 16-Bit Accumulator
  sta.b ResultData // Store Result To Memory
  sep #$20 // Set 8-Bit Accumulator
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA12, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FA24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w TRNResultCheckC
  beq Pass29
  Fail29:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail29
  Pass29:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckC
    bne Fail29
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M8BitX16Bit, $FA42, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  ldx.w #$1FFF // X = $1FFF
  lda.w #$FFFF // A = $FFFF
  sep #$20 // Set 8-Bit Accumulator
  tas // Stack Pointer = A
  tsa // A = Stack Pointer
  txs // Stack Pointer = X

  // Store Result & Processor Status Flag Data
  rep #$20 // Set 16-Bit Accumulator
  sta.b ResultData // Store Result To Memory
  sep #$20 // Set 8-Bit Accumulator
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA52, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FA64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w TRNResultCheckD
  beq Pass30
  Fail30:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail30
  Pass30:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckD
    bne Fail30
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M16BitX16Bit, $FA82, 7) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  ldx.w #$1FFF // X = $1FFF
  lda.w #$0000 // A = $0000
  tas // Stack Pointer = A
  tsa // A = Stack Pointer
  txs // Stack Pointer = X

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA92, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FAA4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w TRNResultCheckG
  beq Pass31
  Fail31:
    PrintText(Fail, $FAB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail31
  Pass31:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckG
    bne Fail31
    PrintText(Pass, $FAB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M16BitX16Bit, $FAC2, 7) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  ldx.w #$1FFF // X = $1FFF
  lda.w #$FFFF // A = $FFFF
  tas // Stack Pointer = A
  tsa // A = Stack Pointer
  txs // Stack Pointer = X

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FAD2, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FAE4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w TRNResultCheckH
  beq Pass32
  Fail32:
    PrintText(Fail, $FAF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail32
  Pass32:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckH
    bne Fail32
    PrintText(Pass, $FAF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $80, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(TRNTSX, $F902, 26) // Load Text To VRAM Lo Bytes
  
  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M16BitX8Bit, $FA02, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag
  clv // Clear Overflow Flag

  // Run Test
  lda.w #$1FFF // A = $1FFF
  ldx.w #$0000 // X = $0000
  txs // Stack Pointer = X
  sep #$10 // Set 8-Bit X/Y
  tsx // X = Stack Pointer
  tas // Stack Pointer = A

  // Store Result & Processor Status Flag Data
  stx.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA12, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FA24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w TRNResultCheckE
  beq Pass33
  Fail33:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail33
  Pass33:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckE
    bne Fail33
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M16BitX8Bit, $FA42, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  lda.w #$1FFF // A = $1FFF
  ldx.w #$FFFF // X = $FFFF
  txs // Stack Pointer = X
  sep #$10 // Set 8-Bit X/Y
  tsx // X = Stack Pointer
  tas // Stack Pointer = A

  // Store Result & Processor Status Flag Data
  stx.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA52, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FA64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w TRNResultCheckF
  beq Pass34
  Fail34:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail34
  Pass34:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckF
    bne Fail34
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M16BitX16Bit, $FA82, 7) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  lda.w #$1FFF // A = $1FFF
  ldx.w #$0000 // X = $0000
  txs // Stack Pointer = X
  tsx // X = Stack Pointer
  tas // Stack Pointer = A

  // Store Result & Processor Status Flag Data
  stx.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA92, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FAA4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w TRNResultCheckG
  beq Pass35
  Fail35:
    PrintText(Fail, $FAB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail35
  Pass35:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckG
    bne Fail35
    PrintText(Pass, $FAB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M16BitX16Bit, $FAC2, 7) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  lda.w #$1FFF // A = $1FFF
  ldx.w #$FFFF // X = $FFFF
  txs // Stack Pointer = X
  tsx // X = Stack Pointer
  tas // Stack Pointer = A

  // Store Result & Processor Status Flag Data
  stx.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FAD2, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FAE4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w TRNResultCheckH
  beq Pass36
  Fail36:
    PrintText(Fail, $FAF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail36
  Pass36:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckH
    bne Fail36
    PrintText(Pass, $FAF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $80, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(TRNTXA, $F902, 26) // Load Text To VRAM Lo Bytes
  
  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M8BitX8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  sep #$10 // Set 8-Bit X/Y
  clc // Clear Carry Flag
  clv // Clear Overflow Flag

  // Run Test
  ldx.b #$00 // X = $00
  txa // A = X

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory
  rep #$10 // Set 16-Bit X/Y

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA12, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FA24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w TRNResultCheckA
  beq Pass37
  Fail37:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail37
  Pass37:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckA
    bne Fail37
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M8BitX8Bit, $FA42, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  sep #$10 // Set 8-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  ldx.b #$FF // X = $FF
  txa // A = X

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory
  rep #$10 // Set 16-Bit X/Y

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA52, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FA64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w TRNResultCheckB
  beq Pass38
  Fail38:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail38
  Pass38:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckB
    bne Fail38
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M8BitX16Bit, $FA82, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  ldx.w #$0000 // X = $0000
  sep #$20 // Set 8-Bit Accumulator
  txa // A = X

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA92, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FAA4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w TRNResultCheckM
  beq Pass39
  Fail39:
    PrintText(Fail, $FAB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail39
  Pass39:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckM
    bne Fail39
    PrintText(Pass, $FAB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M8BitX16Bit, $FAC2, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  ldx.w #$FFFF // X = $FFFF
  sep #$20 // Set 8-Bit Accumulator
  txa // A = X

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FAD2, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FAE4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w TRNResultCheckN
  beq Pass40
  Fail40:
    PrintText(Fail, $FAF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail40
  Pass40:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckN
    bne Fail40
    PrintText(Pass, $FAF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M16BitX8Bit, $FB02, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  sep #$10 // Set 8-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  ldx.b #$00 // X = $00
  txa // A = X

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory
  rep #$10 // Set 16-Bit X/Y

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB12, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FB24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w TRNResultCheckO
  beq Pass41
  Fail41:
    PrintText(Fail, $FB32, 4) // Load Text To VRAM Lo Bytes
    bra Fail41
  Pass41:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckO
    bne Fail41
    PrintText(Pass, $FB32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M16BitX8Bit, $FB42, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  sep #$10 // Set 8-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  ldx.b #$FF // X = $FF
  txa // A = X

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory
  rep #$10 // Set 16-Bit X/Y

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB52, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FB64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w TRNResultCheckP
  beq Pass42
  Fail42:
    PrintText(Fail, $FB72, 4) // Load Text To VRAM Lo Bytes
    bra Fail42
  Pass42:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckP
    bne Fail42
    PrintText(Pass, $FB72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M16BitX16Bit, $FB82, 7) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  ldx.w #$0000 // X = $0000
  txa // A = X

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB92, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FBA4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w TRNResultCheckG
  beq Pass43
  Fail43:
    PrintText(Fail, $FBB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail43
  Pass43:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckG
    bne Fail43
    PrintText(Pass, $FBB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M16BitX16Bit, $FBC2, 7) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  ldx.w #$FFFF // X = $FFFF
  txa // A = X

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FBD2, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FBE4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w TRNResultCheckH
  beq Pass44
  Fail44:
    PrintText(Fail, $FBF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail44
  Pass44:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckH
    bne Fail44
    PrintText(Pass, $FBF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $100, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(TRNTXS, $F902, 26) // Load Text To VRAM Lo Bytes
  
  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M8BitX8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag
  clv // Clear Overflow Flag

  // Run Test
  ldx.w #$0000 // X = $0000
  sep #$10 // Set 8-Bit X/Y
  txs // Stack Pointer = X
  tsx // X = Stack Pointer
  rep #$20 // Set 16-Bit Accumulator
  lda.w #$1FFF // A = $1FFF
  sep #$20 // Set 8-Bit Accumulator
  tas // Stack Pointer = A

  // Store Result & Processor Status Flag Data
  stx.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$10 // Set 16-Bit X/Y
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA12, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FA24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w TRNResultCheckQ
  beq Pass45
  Fail45:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail45
  Pass45:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckQ
    bne Fail45
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M8BitX8Bit, $FA42, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  ldx.w #$FFFF // X = $FFFF
  sep #$10 // Set 8-Bit X/Y
  txs // Stack Pointer = X
  tsx // X = Stack Pointer
  rep #$20 // Set 16-Bit Accumulator
  lda.w #$1FFF // A = $1FFF
  sep #$20 // Set 8-Bit Accumulator
  tas // Stack Pointer = A

  // Store Result & Processor Status Flag Data
  stx.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$10 // Set 16-Bit X/Y
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA52, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FA64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w TRNResultCheckR
  beq Pass46
  Fail46:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail46
  Pass46:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckR
    bne Fail46
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M8BitX16Bit, $FA82, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  ldx.w #$0000 // X = $0000
  txs // Stack Pointer = X
  tsx // X = Stack Pointer
  rep #$20 // Set 16-Bit Accumulator
  lda.w #$1FFF // A = $1FFF
  sep #$20 // Set 8-Bit Accumulator
  tas // Stack Pointer = A

  // Store Result & Processor Status Flag Data
  stx.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA92, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FAA4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w TRNResultCheckI
  beq Pass47
  Fail47:
    PrintText(Fail, $FAB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail47
  Pass47:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckI
    bne Fail47
    PrintText(Pass, $FAB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M8BitX16Bit, $FAC2, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  ldx.w #$FFFF // X = $FFFF
  txs // Stack Pointer = X
  tsx // X = Stack Pointer
  rep #$20 // Set 16-Bit Accumulator
  lda.w #$1FFF // A = $1FFF
  sep #$20 // Set 8-Bit Accumulator
  tas // Stack Pointer = A

  // Store Result & Processor Status Flag Data
  stx.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FAD2, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FAE4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w TRNResultCheckJ
  beq Pass48
  Fail48:
    PrintText(Fail, $FAF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail48
  Pass48:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckJ
    bne Fail48
    PrintText(Pass, $FAF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $80, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(TRNTXY, $F902, 26) // Load Text To VRAM Lo Bytes
  
  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M8BitX8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  sep #$10 // Set 8-Bit X/Y
  clc // Clear Carry Flag
  clv // Clear Overflow Flag

  // Run Test
  ldx.b #$00 // X = $00
  txy // Y = X

  // Store Result & Processor Status Flag Data
  sty.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$10 // Set 16-Bit X/Y
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA12, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FA24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w TRNResultCheckA
  beq Pass49
  Fail49:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail49
  Pass49:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckA
    bne Fail49
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M8BitX8Bit, $FA42, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  sep #$10 // Set 8-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  ldx.b #$FF // X = $FF
  txy // Y = X

  // Store Result & Processor Status Flag Data
  sty.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$10 // Set 16-Bit X/Y
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA52, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FA64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w TRNResultCheckB
  beq Pass50
  Fail50:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail50
  Pass50:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckB
    bne Fail50
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M8BitX16Bit, $FA82, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  ldx.w #$0000 // X = $0000
  txy // Y = X

  // Store Result & Processor Status Flag Data
  sty.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA92, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FAA4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w TRNResultCheckC
  beq Pass51
  Fail51:
    PrintText(Fail, $FAB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail51
  Pass51:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckC
    bne Fail51
    PrintText(Pass, $FAB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M8BitX16Bit, $FAC2, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  ldx.w #$FFFF // X = $FFFF
  txy // Y = X

  // Store Result & Processor Status Flag Data
  sty.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FAD2, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FAE4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w TRNResultCheckD
  beq Pass52
  Fail52:
    PrintText(Fail, $FAF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail52
  Pass52:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckD
    bne Fail52
    PrintText(Pass, $FAF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $80, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(TRNTYA, $F902, 26) // Load Text To VRAM Lo Bytes
  
  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M8BitX8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  sep #$10 // Set 8-Bit X/Y
  clc // Clear Carry Flag
  clv // Clear Overflow Flag

  // Run Test
  ldy.b #$00 // Y = $00
  tya // A = Y

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory
  rep #$10 // Set 16-Bit X/Y

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA12, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FA24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w TRNResultCheckA
  beq Pass53
  Fail53:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail53
  Pass53:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckA
    bne Fail53
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M8BitX8Bit, $FA42, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  sep #$10 // Set 8-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  ldy.b #$FF // Y = $FF
  tya // A = Y

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory
  rep #$10 // Set 16-Bit X/Y

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA52, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FA64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w TRNResultCheckB
  beq Pass54
  Fail54:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail54
  Pass54:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckB
    bne Fail54
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M8BitX16Bit, $FA82, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  ldy.w #$0000 // Y = $0000
  sep #$20 // Set 8-Bit Accumulator
  tya // A = Y

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA92, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FAA4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w TRNResultCheckM
  beq Pass55
  Fail55:
    PrintText(Fail, $FAB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail55
  Pass55:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckM
    bne Fail55
    PrintText(Pass, $FAB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M8BitX16Bit, $FAC2, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  ldy.w #$FFFF // Y = $FFFF
  sep #$20 // Set 8-Bit Accumulator
  tya // A = Y

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FAD2, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FAE4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w TRNResultCheckN
  beq Pass56
  Fail56:
    PrintText(Fail, $FAF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail56
  Pass56:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckN
    bne Fail56
    PrintText(Pass, $FAF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M16BitX8Bit, $FB02, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  sep #$10 // Set 8-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  ldy.b #$00 // Y = $00
  tya // A = Y

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory
  rep #$10 // Set 16-Bit X/Y

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB12, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FB24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w TRNResultCheckO
  beq Pass57
  Fail57:
    PrintText(Fail, $FB32, 4) // Load Text To VRAM Lo Bytes
    bra Fail57
  Pass57:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckO
    bne Fail57
    PrintText(Pass, $FB32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M16BitX8Bit, $FB42, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  sep #$10 // Set 8-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  ldy.b #$FF // Y = $FF
  tya // A = Y

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory
  rep #$10 // Set 16-Bit X/Y

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB52, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FB64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w TRNResultCheckP
  beq Pass58
  Fail58:
    PrintText(Fail, $FB72, 4) // Load Text To VRAM Lo Bytes
    bra Fail58
  Pass58:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckP
    bne Fail58
    PrintText(Pass, $FB72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M16BitX16Bit, $FB82, 7) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  ldy.w #$0000 // Y = $0000
  tya // A = Y

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB92, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FBA4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w TRNResultCheckG
  beq Pass59
  Fail59:
    PrintText(Fail, $FBB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail59
  Pass59:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckG
    bne Fail59
    PrintText(Pass, $FBB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M16BitX16Bit, $FBC2, 7) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  ldy.w #$FFFF // Y = $FFFF
  tya // A = Y

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FBD2, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FBE4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w TRNResultCheckH
  beq Pass60
  Fail60:
    PrintText(Fail, $FBF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail60
  Pass60:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckH
    bne Fail60
    PrintText(Pass, $FBF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $100, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(TRNTYX, $F902, 26) // Load Text To VRAM Lo Bytes
  
  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M8BitX8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  sep #$10 // Set 8-Bit X/Y
  clc // Clear Carry Flag
  clv // Clear Overflow Flag

  // Run Test
  ldy.b #$00 // Y = $00
  tyx // X = Y

  // Store Result & Processor Status Flag Data
  stx.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$10 // Set 16-Bit X/Y
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA12, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FA24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w TRNResultCheckA
  beq Pass61
  Fail61:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail61
  Pass61:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckA
    bne Fail61
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M8BitX8Bit, $FA42, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  sep #$10 // Set 8-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  ldy.b #$FF // Y = $FF
  tyx // X = Y

  // Store Result & Processor Status Flag Data
  stx.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$10 // Set 16-Bit X/Y
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA52, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FA64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w TRNResultCheckB
  beq Pass62
  Fail62:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail62
  Pass62:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckB
    bne Fail62
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M8BitX16Bit, $FA82, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  ldy.w #$0000 // Y = $0000
  tyx // X = Y

  // Store Result & Processor Status Flag Data
  stx.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA92, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FAA4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w TRNResultCheckC
  beq Pass63
  Fail63:
    PrintText(Fail, $FAB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail63
  Pass63:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckC
    bne Fail63
    PrintText(Pass, $FAB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M8BitX16Bit, $FAC2, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  ldy.w #$FFFF // Y = $FFFF
  tyx // X = Y

  // Store Result & Processor Status Flag Data
  stx.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FAD2, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FAE4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w TRNResultCheckD
  beq Pass64
  Fail64:
    PrintText(Fail, $FAF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail64
  Pass64:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckD
    bne Fail64
    PrintText(Pass, $FAF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $80, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(TRNXBA, $F902, 26) // Load Text To VRAM Lo Bytes
  
  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M8BitX16Bit, $FA02, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag
  clv // Clear Overflow Flag

  // Run Test
  lda.w #$00FF // A = $00FF
  sep #$20 // Set 8-Bit Accumulator
  xba // A = B

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA12, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FA24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w TRNResultCheckM
  beq Pass65
  Fail65:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail65
  Pass65:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckM
    bne Fail65
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M8BitX16Bit, $FA42, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  lda.w #$FF00 // A = $FF00
  sep #$20 // Set 8-Bit Accumulator
  xba // A = B

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA52, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FA64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w TRNResultCheckN
  beq Pass66
  Fail66:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail66
  Pass66:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckN
    bne Fail66
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M16BitX16Bit, $FA82, 7) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  lda.w #$00FF // A = $00FF
  xba // A = B

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA92, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FAA4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w TRNResultCheckS
  beq Pass67
  Fail67:
    PrintText(Fail, $FAB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail67
  Pass67:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckS
    bne Fail67
    PrintText(Pass, $FAB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M16BitX16Bit, $FAC2, 7) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  lda.w #$FF00 // A = $FF00
  xba // A = B

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FAD2, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FAE4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w TRNResultCheckT
  beq Pass68
  Fail68:
    PrintText(Fail, $FAF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail68
  Pass68:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckT
    bne Fail68
    PrintText(Pass, $FAF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $80, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(TRNXCE, $F902, 26) // Load Text To VRAM Lo Bytes
  
  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M8BitX16Bit, $FA02, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag
  clv // Clear Overflow Flag

  // Run Test
  ldx.w #$FFFF // X = $FFFF
  sec // Set Carry Flag
  xce // E = C
  xce // E = C

  // Store Result & Processor Status Flag Data
  rep #$10 // Set 16-Bit X/Y
  stx.b ResultData // Store Result To Memory
  ldx.w #$1FFF // X = $1FFF
  txs // Stack = X
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA12, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FA24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w TRNResultCheckU
  beq Pass69
  Fail69:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail69
  Pass69:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckU
    bne Fail69
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M8BitX16Bit, $FA42, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  ldy.w #$FFFF // Y = $FFFF
  sec // Set Carry Flag
  xce // E = C
  xce // E = C

  // Store Result & Processor Status Flag Data
  rep #$10 // Set 16-Bit X/Y
  sty.b ResultData // Store Result To Memory
  ldx.w #$1FFF // X = $1FFF
  txs // Stack = X
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA52, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FA64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w TRNResultCheckU
  beq Pass70
  Fail70:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail70
  Pass70:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckU
    bne Fail70
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(M16BitX16Bit, $FA82, 7) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  clc // Clear Carry Flag

  // Run Test
  lda.w #$FFFF // A = $FFFF
  sec // Set Carry Flag
  xce // E = C
  xce // E = C

  // Store Result & Processor Status Flag Data
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y
  sta.b ResultData // Store Result To Memory
  ldx.w #$1FFF // X = $1FFF
  txs // Stack = X
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA92, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FAA4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w TRNResultCheckV
  beq Pass71
  Fail71:
    PrintText(Fail, $FAB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail71
  Pass71:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckV
    bne Fail71
    PrintText(Pass, $FAB2, 4) // Load Text To VRAM Lo Bytes

Loop:
  jmp Loop

Title:
  db "CPU Test TRN (Transfer):"

PageBreak:
  db "------------------------------"

Key:
  db "Modes | Result | NVZC | Test |"
M8BitX8Bit:
  db "M8,X8"
M8BitX16Bit:
  db "M8,X16"
M16BitX8Bit:
  db "M16,X8"
M16BitX16Bit:
  db "M16,X16"
Fail:
  db "FAIL"
Pass:
  db "PASS"

TRNTAX:
  db "TAX          (Opcode: $AA)"
TRNTAY:
  db "TAY          (Opcode: $A8)"
TRNTCD:
  db "TCD/TAD      (Opcode: $5B)"
TRNTCS:
  db "TCS/TAS      (Opcode: $1B)"
TRNTDC:
  db "TDC/TDA      (Opcode: $7B)"
TRNTSC:
  db "TSC/TSA      (Opcode: $3B)"
TRNTSX:
  db "TSX          (Opcode: $BA)"
TRNTXA:
  db "TXA          (Opcode: $8A)"
TRNTXS:
  db "TXS          (Opcode: $9A)"
TRNTXY:
  db "TXY          (Opcode: $9B)"
TRNTYA:
  db "TYA          (Opcode: $98)"
TRNTYX:
  db "TYX          (Opcode: $BB)"
TRNXBA:
  db "XBA          (Opcode: $EB)"
TRNXCE:
  db "XCE          (Opcode: $FB)"

TRNResultCheckA:
  db $00
PSRResultCheckA:
  db $36

TRNResultCheckB:
  db $FF
PSRResultCheckB:
  db $B4

TRNResultCheckC:
  dw $0000
PSRResultCheckC:
  db $26

TRNResultCheckD:
  dw $FFFF
PSRResultCheckD:
  db $A4

TRNResultCheckE:
  db $00
PSRResultCheckE:
  db $16

TRNResultCheckF:
  db $FF
PSRResultCheckF:
  db $94

TRNResultCheckG:
  dw $0000
PSRResultCheckG:
  db $06

TRNResultCheckH:
  dw $FFFF
PSRResultCheckH:
  db $84

TRNResultCheckI:
  dw $0000
PSRResultCheckI:
  db $24

TRNResultCheckJ:
  dw $FFFF
PSRResultCheckJ:
  db $24

TRNResultCheckK:
  dw $0000
PSRResultCheckK:
  db $04

TRNResultCheckL:
  dw $FFFF
PSRResultCheckL:
  db $04

TRNResultCheckM:
  db $00
PSRResultCheckM:
  db $26

TRNResultCheckN:
  db $FF
PSRResultCheckN:
  db $A4

TRNResultCheckO:
  dw $0000
PSRResultCheckO:
  db $16

TRNResultCheckP:
  dw $00FF
PSRResultCheckP:
  db $14

TRNResultCheckQ:
  db $00
PSRResultCheckQ:
  db $34

TRNResultCheckR:
  db $FF
PSRResultCheckR:
  db $34

TRNResultCheckS:
  dw $FF00
PSRResultCheckS:
  db $06

TRNResultCheckT:
  dw $00FF
PSRResultCheckT:
  db $84

TRNResultCheckU:
  dw $00FF
PSRResultCheckU:
  db $25

TRNResultCheckV:
  dw $FFFF
PSRResultCheckV:
  db $05

BGCHR:
  include "Font8x8.asm" // Include BG 1BPP 8x8 Tile Font Character Data (1016 Bytes)
BGPAL:
  dw $7800, $7FFF // Blue / White Palette (4 Bytes)
BGCLEAR:
  dw $0020 // BG Clear Character Space " " Fixed Word