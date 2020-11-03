// SNES 65816 CPU Test SBC (Subtract With Borrow) demo by krom (Peter Lemon):
arch snes.cpu
output "CPUSBC.sfc", create

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
AbsoluteData:
  dw 0 // Absolute Data Word
IndirectData:
  dl 0 // Indirect Data Long

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
  PrintText(Title, $F882, 31) // Load Text To VRAM Lo Bytes

  // Print Page Break Text
  PrintText(PageBreak, $F8C2, 30) // Load Text To VRAM Lo Bytes

  // Print Syntax/Opcode Text
  PrintText(SBCConst, $F902, 26) // Load Text To VRAM Lo Bytes

  // Print Key Text
  PrintText(Key, $F982, 30) // Load Text To VRAM Lo Bytes

  // Print Page Break Text
  PrintText(PageBreak, $F9C2, 30) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.b #$7F // A = $7F
  sbc.b #$7E // A -= $7E

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
  cmp.w SBCResultCheckA
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
  PrintText(Binary8Bit, $FA42, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.b #$7F // A = $7F
  sbc.b #$80 // A -= $80

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
  cmp.w SBCResultCheckB
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
  PrintText(Binary16Bit, $FA82, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.w #$7FFF // A = $7FFF
  sbc.w #$7FFE // A -= $7FFE

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
  cpx.w SBCResultCheckC
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
  PrintText(Binary16Bit, $FAC2, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.w #$7FFF // A = $7FFF
  sbc.w #$8000 // A -= $8000

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
  cpx.w SBCResultCheckD
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
  PrintText(Decimal8Bit, $FB02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.b #$49 // A = $49
  sbc.b #$48 // A -= $48

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB12, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FB24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w SBCResultCheckE
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
  PrintText(Decimal8Bit, $FB42, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.b #$49 // A = $49
  sbc.b #$50 // A -= $50

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB52, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FB64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w SBCResultCheckF
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
  PrintText(Decimal16Bit, $FB82, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.w #$4999 // A = $4999
  sbc.w #$4998 // A -= $4998

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB92, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FBA4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w SBCResultCheckG
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
  PrintText(Decimal16Bit, $FBC2, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.w #$4999 // A = $4999
  sbc.w #$5000 // A -= $5000

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FBD2, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FBE4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w SBCResultCheckH
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
  PrintText(SBCAddr, $F902, 26) // Load Text To VRAM Lo Bytes
  
  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.b #$7E // A = $7E
  sta.b AbsoluteData // Store Absolute Data
  lda.b #$7F // A = $7F
  sbc.w AbsoluteData // A -= $7E

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
  cmp.w SBCResultCheckA
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
  PrintText(Binary8Bit, $FA42, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.b #$80 // A = $80
  sta.b AbsoluteData // Store Absolute Data
  lda.b #$7F // A = $7F
  sbc.w AbsoluteData // A -= $80

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
  cmp.w SBCResultCheckB
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
  PrintText(Binary16Bit, $FA82, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.w #$7FFE // A = $7FFE
  sta.b AbsoluteData // Store Absolute Data
  lda.w #$7FFF // A = $7FFF
  sbc.w AbsoluteData // A -= $7FFE

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
  cpx.w SBCResultCheckC
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
  PrintText(Binary16Bit, $FAC2, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.w #$8000 // A = $8000
  sta.b AbsoluteData // Store Absolute Data
  lda.w #$7FFF // A = $7FFF
  sbc.w AbsoluteData // A -= $8000

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
  cpx.w SBCResultCheckD
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
  PrintText(Decimal8Bit, $FB02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.b #$48 // A = $48
  sta.b AbsoluteData // Store Absolute Data
  lda.b #$49 // A = $49
  sbc.w AbsoluteData // A -= $48

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB12, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FB24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w SBCResultCheckE
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
  PrintText(Decimal8Bit, $FB42, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.b #$50 // A = $50
  sta.b AbsoluteData // Store Absolute Data
  lda.b #$49 // A = $49
  sbc.w AbsoluteData // A -= $50

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB52, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FB64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w SBCResultCheckF
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
  PrintText(Decimal16Bit, $FB82, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.w #$4998 // A = $4998
  sta.b AbsoluteData // Store Absolute Data
  lda.w #$4999 // A = $4999
  sbc.w AbsoluteData // A -= $4998

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB92, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FBA4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w SBCResultCheckG
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
  PrintText(Decimal16Bit, $FBC2, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.w #$5000 // A = $5000
  sta.b AbsoluteData // Store Absolute Data
  lda.w #$4999 // A = $4999
  sbc.w AbsoluteData // A -= $5000

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FBD2, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FBE4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w SBCResultCheckH
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
  PrintText(SBCLong, $F902, 26) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.b #$7E // A = $7E
  sta.b AbsoluteData // Store Absolute Data
  lda.b #$7F // A = $7F
  sbc.l AbsoluteData // A -= $7E

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
  cmp.w SBCResultCheckA
  beq Pass17
  Fail17:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail17
  Pass17:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckA
    bne Fail17
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA42, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.b #$80 // A = $80
  sta.b AbsoluteData // Store Absolute Data
  lda.b #$7F // A = $7F
  sbc.l AbsoluteData // A -= $80

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
  cmp.w SBCResultCheckB
  beq Pass18
  Fail18:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail18
  Pass18:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckB
    bne Fail18
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FA82, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.w #$7FFE // A = $7FFE
  sta.b AbsoluteData // Store Absolute Data
  lda.w #$7FFF // A = $7FFF
  sbc.l AbsoluteData // A -= $7FFE

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
  cpx.w SBCResultCheckC
  beq Pass19
  Fail19:
    PrintText(Fail, $FAB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail19
  Pass19:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckC
    bne Fail19
    PrintText(Pass, $FAB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FAC2, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.w #$8000 // A = $8000
  sta.b AbsoluteData // Store Absolute Data
  lda.w #$7FFF // A = $7FFF
  sbc.l AbsoluteData // A -= $8000

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
  cpx.w SBCResultCheckD
  beq Pass20
  Fail20:
    PrintText(Fail, $FAF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail20
  Pass20:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckD
    bne Fail20
    PrintText(Pass, $FAF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal8Bit, $FB02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.b #$48 // A = $48
  sta.b AbsoluteData // Store Absolute Data
  lda.b #$49 // A = $49
  sbc.l AbsoluteData // A -= $48

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB12, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FB24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w SBCResultCheckE
  beq Pass21
  Fail21:
    PrintText(Fail, $FB32, 4) // Load Text To VRAM Lo Bytes
    bra Fail21
  Pass21:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckE
    bne Fail21
    PrintText(Pass, $FB32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal8Bit, $FB42, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.b #$50 // A = $50
  sta.b AbsoluteData // Store Absolute Data
  lda.b #$49 // A = $49
  sbc.l AbsoluteData // A -= $50

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB52, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FB64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w SBCResultCheckF
  beq Pass22
  Fail22:
    PrintText(Fail, $FB72, 4) // Load Text To VRAM Lo Bytes
    bra Fail22
  Pass22:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckF
    bne Fail22
    PrintText(Pass, $FB72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal16Bit, $FB82, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.w #$4998 // A = $4998
  sta.b AbsoluteData // Store Absolute Data
  lda.w #$4999 // A = $4999
  sbc.l AbsoluteData // A -= $4998

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB92, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FBA4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w SBCResultCheckG
  beq Pass23
  Fail23:
    PrintText(Fail, $FBB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail23
  Pass23:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckG
    bne Fail23
    PrintText(Pass, $FBB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal16Bit, $FBC2, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.w #$5000 // A = $5000
  sta.b AbsoluteData // Store Absolute Data
  lda.w #$4999 // A = $4999
  sbc.l AbsoluteData // A -= $5000

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FBD2, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FBE4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w SBCResultCheckH
  beq Pass24
  Fail24:
    PrintText(Fail, $FBF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail24
  Pass24:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckH
    bne Fail24
    PrintText(Pass, $FBF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $100, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(SBCDP, $F902, 26) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.b #$7E // A = $7E
  sta.b AbsoluteData // Store Absolute Data
  lda.b #$7F // A = $7F
  sbc.b AbsoluteData // A -= $7E

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
  cmp.w SBCResultCheckA
  beq Pass25
  Fail25:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail25
  Pass25:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckA
    bne Fail25
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA42, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.b #$80 // A = $80
  sta.b AbsoluteData // Store Absolute Data
  lda.b #$7F // A = $7F
  sbc.b AbsoluteData // A -= $80

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
  cmp.w SBCResultCheckB
  beq Pass26
  Fail26:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail26
  Pass26:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckB
    bne Fail26
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FA82, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.w #$7FFE // A = $7FFE
  sta.b AbsoluteData // Store Absolute Data
  lda.w #$7FFF // A = $7FFF
  sbc.b AbsoluteData // A -= $7FFE

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
  cpx.w SBCResultCheckC
  beq Pass27
  Fail27:
    PrintText(Fail, $FAB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail27
  Pass27:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckC
    bne Fail27
    PrintText(Pass, $FAB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FAC2, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.w #$8000 // A = $8000
  sta.b AbsoluteData // Store Absolute Data
  lda.w #$7FFF // A = $7FFF
  sbc.b AbsoluteData // A -= $8000

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
  cpx.w SBCResultCheckD
  beq Pass28
  Fail28:
    PrintText(Fail, $FAF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail28
  Pass28:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckD
    bne Fail28
    PrintText(Pass, $FAF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal8Bit, $FB02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.b #$48 // A = $48
  sta.b AbsoluteData // Store Absolute Data
  lda.b #$49 // A = $49
  sbc.b AbsoluteData // A -= $48

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB12, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FB24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w SBCResultCheckE
  beq Pass29
  Fail29:
    PrintText(Fail, $FB32, 4) // Load Text To VRAM Lo Bytes
    bra Fail29
  Pass29:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckE
    bne Fail29
    PrintText(Pass, $FB32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal8Bit, $FB42, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.b #$50 // A = $50
  sta.b AbsoluteData // Store Absolute Data
  lda.b #$49 // A = $49
  sbc.b AbsoluteData // A -= $50

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB52, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FB64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w SBCResultCheckF
  beq Pass30
  Fail30:
    PrintText(Fail, $FB72, 4) // Load Text To VRAM Lo Bytes
    bra Fail30
  Pass30:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckF
    bne Fail30
    PrintText(Pass, $FB72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal16Bit, $FB82, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.w #$4998 // A = $4998
  sta.b AbsoluteData // Store Absolute Data
  lda.w #$4999 // A = $4999
  sbc.b AbsoluteData // A -= $4998

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB92, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FBA4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w SBCResultCheckG
  beq Pass31
  Fail31:
    PrintText(Fail, $FBB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail31
  Pass31:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckG
    bne Fail31
    PrintText(Pass, $FBB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal16Bit, $FBC2, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.w #$5000 // A = $5000
  sta.b AbsoluteData // Store Absolute Data
  lda.w #$4999 // A = $4999
  sbc.b AbsoluteData // A -= $5000

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FBD2, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FBE4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w SBCResultCheckH
  beq Pass32
  Fail32:
    PrintText(Fail, $FBF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail32
  Pass32:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckH
    bne Fail32
    PrintText(Pass, $FBF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $100, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(SBCDPIndirect, $F902, 26) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.b #$7E // A = $7E
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  lda.b #$7F // A = $7F
  sbc (IndirectData) // A -= $7E

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
  cmp.w SBCResultCheckA
  beq Pass33
  Fail33:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail33
  Pass33:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckA
    bne Fail33
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA42, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.b #$80 // A = $80
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  lda.b #$7F // A = $7F
  sbc (IndirectData) // A -= $80

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
  cmp.w SBCResultCheckB
  beq Pass34
  Fail34:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail34
  Pass34:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckB
    bne Fail34
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FA82, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.w #$7FFE // A = $7FFE
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  lda.w #$7FFF // A = $7FFF
  sbc (IndirectData) // A -= $7FFE

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
  cpx.w SBCResultCheckC
  beq Pass35
  Fail35:
    PrintText(Fail, $FAB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail35
  Pass35:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckC
    bne Fail35
    PrintText(Pass, $FAB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  // Print Modes Text
  PrintText(Binary16Bit, $FAC2, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.w #$8000 // A = $8000
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  lda.w #$7FFF // A = $7FFF
  sbc (IndirectData) // A -= $8000

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
  cpx.w SBCResultCheckD
  beq Pass36
  Fail36:
    PrintText(Fail, $FAF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail36
  Pass36:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckD
    bne Fail36
    PrintText(Pass, $FAF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal8Bit, $FB02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.b #$48 // A = $48
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  lda.b #$49 // A = $49
  sbc (IndirectData) // A -= $48

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB12, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FB24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w SBCResultCheckE
  beq Pass37
  Fail37:
    PrintText(Fail, $FB32, 4) // Load Text To VRAM Lo Bytes
    bra Fail37
  Pass37:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckE
    bne Fail37
    PrintText(Pass, $FB32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal8Bit, $FB42, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.b #$50 // A = $50
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  lda.b #$49 // A = $49
  sbc (IndirectData) // A -= $50

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB52, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FB64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w SBCResultCheckF
  beq Pass38
  Fail38:
    PrintText(Fail, $FB72, 4) // Load Text To VRAM Lo Bytes
    bra Fail38
  Pass38:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckF
    bne Fail38
    PrintText(Pass, $FB72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal16Bit, $FB82, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.w #$4998 // A = $4998
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  lda.w #$4999 // A = $4999
  sbc (IndirectData) // A -= $4998

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB92, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FBA4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w SBCResultCheckG
  beq Pass39
  Fail39:
    PrintText(Fail, $FBB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail39
  Pass39:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckG
    bne Fail39
    PrintText(Pass, $FBB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal16Bit, $FBC2, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.w #$5000 // A = $5000
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  lda.w #$4999 // A = $4999
  sbc (IndirectData) // A -= $5000

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FBD2, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FBE4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w SBCResultCheckH
  beq Pass40
  Fail40:
    PrintText(Fail, $FBF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail40
  Pass40:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckH
    bne Fail40
    PrintText(Pass, $FBF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $100, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(SBCDPIndirectLong, $F902, 26) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.b #$7E // A = $7E
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  lda.b #$7F // A = $7F
  sbc [IndirectData] // A -= $7E

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
  cmp.w SBCResultCheckA
  beq Pass41
  Fail41:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail41
  Pass41:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckA
    bne Fail41
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA42, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.b #$80 // A = $80
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  lda.b #$7F // A = $7F
  sbc [IndirectData] // A -= $80

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
  cmp.w SBCResultCheckB
  beq Pass42
  Fail42:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail42
  Pass42:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckB
    bne Fail42
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FA82, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.w #$7FFE // A = $7FFE
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  lda.w #$7FFF // A = $7FFF
  sbc [IndirectData] // A -= $7FFE

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
  cpx.w SBCResultCheckC
  beq Pass43
  Fail43:
    PrintText(Fail, $FAB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail43
  Pass43:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckC
    bne Fail43
    PrintText(Pass, $FAB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FAC2, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.w #$8000 // A = $8000
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  lda.w #$7FFF // A = $7FFF
  sbc [IndirectData] // A -= $8000

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
  cpx.w SBCResultCheckD
  beq Pass44
  Fail44:
    PrintText(Fail, $FAF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail44
  Pass44:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckD
    bne Fail44
    PrintText(Pass, $FAF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal8Bit, $FB02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.b #$48 // A = $48
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  lda.b #$49 // A = $49
  sbc [IndirectData] // A -= $48

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB12, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FB24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w SBCResultCheckE
  beq Pass45
  Fail45:
    PrintText(Fail, $FB32, 4) // Load Text To VRAM Lo Bytes
    bra Fail45
  Pass45:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckE
    bne Fail45
    PrintText(Pass, $FB32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal8Bit, $FB42, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.b #$50 // A = $50
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  lda.b #$49 // A = $49
  sbc [IndirectData] // A -= $50

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB52, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FB64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w SBCResultCheckF
  beq Pass46
  Fail46:
    PrintText(Fail, $FB72, 4) // Load Text To VRAM Lo Bytes
    bra Fail46
  Pass46:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckF
    bne Fail46
    PrintText(Pass, $FB72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal16Bit, $FB82, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.w #$4998 // A = $4998
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  lda.w #$4999 // A = $4999
  sbc [IndirectData] // A -= $4998

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB92, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FBA4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w SBCResultCheckG
  beq Pass47
  Fail47:
    PrintText(Fail, $FBB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail47
  Pass47:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckG
    bne Fail47
    PrintText(Pass, $FBB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  // Print Modes Text
  PrintText(Decimal16Bit, $FBC2, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.w #$5000 // A = $5000
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  lda.w #$4999 // A = $4999
  sbc [IndirectData] // A -= $5000

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FBD2, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FBE4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w SBCResultCheckH
  beq Pass48
  Fail48:
    PrintText(Fail, $FBF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail48
  Pass48:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckH
    bne Fail48
    PrintText(Pass, $FBF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $100, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(SBCAddrX, $F902, 26) // Load Text To VRAM Lo Bytes
  
  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.b #$7E // A = $7E
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #0 // X = 0
  lda.b #$7F // A = $7F
  sbc.w AbsoluteData,x // A -= $7E

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
  cmp.w SBCResultCheckA
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
  PrintText(Binary8Bit, $FA42, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.b #$80 // A = $80
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #0 // X = 0
  lda.b #$7F // A = $7F
  sbc.w AbsoluteData,x // A -= $80

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
  cmp.w SBCResultCheckB
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
  PrintText(Binary16Bit, $FA82, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.w #$7FFE // A = $7FFE
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #0 // X = 0
  lda.w #$7FFF // A = $7FFF
  sbc.w AbsoluteData,x // A -= $7FFE

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
  cpx.w SBCResultCheckC
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
  PrintText(Binary16Bit, $FAC2, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.w #$8000 // A = $8000
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #0 // X = 0
  lda.w #$7FFF // A = $7FFF
  sbc.w AbsoluteData,x // A -= $8000

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
  cpx.w SBCResultCheckD
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
  // Print Modes Text
  PrintText(Decimal8Bit, $FB02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.b #$48 // A = $48
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #0 // X = 0
  lda.b #$49 // A = $49
  sbc.w AbsoluteData,x // A -= $48

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB12, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FB24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w SBCResultCheckE
  beq Pass53
  Fail53:
    PrintText(Fail, $FB32, 4) // Load Text To VRAM Lo Bytes
    bra Fail53
  Pass53:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckE
    bne Fail53
    PrintText(Pass, $FB32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal8Bit, $FB42, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.b #$50 // A = $50
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #0 // X = 0
  lda.b #$49 // A = $49
  sbc.w AbsoluteData,x // A -= $50

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB52, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FB64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w SBCResultCheckF
  beq Pass54
  Fail54:
    PrintText(Fail, $FB72, 4) // Load Text To VRAM Lo Bytes
    bra Fail54
  Pass54:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckF
    bne Fail54
    PrintText(Pass, $FB72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal16Bit, $FB82, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.w #$4998 // A = $4998
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #0 // X = 0
  lda.w #$4999 // A = $4999
  sbc.w AbsoluteData,x // A -= $4998

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB92, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FBA4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w SBCResultCheckG
  beq Pass55
  Fail55:
    PrintText(Fail, $FBB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail55
  Pass55:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckG
    bne Fail55
    PrintText(Pass, $FBB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal16Bit, $FBC2, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.w #$5000 // A = $5000
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #0 // X = 0
  lda.w #$4999 // A = $4999
  sbc.w AbsoluteData,x // A -= $5000

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FBD2, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FBE4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w SBCResultCheckH
  beq Pass56
  Fail56:
    PrintText(Fail, $FBF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail56
  Pass56:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckH
    bne Fail56
    PrintText(Pass, $FBF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $100, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(SBCLongX, $F902, 26) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.b #$7E // A = $7E
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #0 // X = 0
  lda.b #$7F // A = $7F
  sbc.l AbsoluteData,x // A -= $7E

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
  cmp.w SBCResultCheckA
  beq Pass57
  Fail57:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail57
  Pass57:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckA
    bne Fail57
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA42, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.b #$80 // A = $80
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #0 // X = 0
  lda.b #$7F // A = $7F
  sbc.l AbsoluteData,x // A -= $80

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
  cmp.w SBCResultCheckB
  beq Pass58
  Fail58:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail58
  Pass58:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckB
    bne Fail58
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FA82, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.w #$7FFE // A = $7FFE
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #0 // X = 0
  lda.w #$7FFF // A = $7FFF
  sbc.l AbsoluteData,x // A -= $7FFE

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
  cpx.w SBCResultCheckC
  beq Pass59
  Fail59:
    PrintText(Fail, $FAB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail59
  Pass59:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckC
    bne Fail59
    PrintText(Pass, $FAB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FAC2, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.w #$8000 // A = $8000
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #0 // X = 0
  lda.w #$7FFF // A = $7FFF
  sbc.l AbsoluteData,x // A -= $8000

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
  cpx.w SBCResultCheckD
  beq Pass60
  Fail60:
    PrintText(Fail, $FAF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail60
  Pass60:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckD
    bne Fail60
    PrintText(Pass, $FAF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal8Bit, $FB02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.b #$48 // A = $48
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #0 // X = 0
  lda.b #$49 // A = $49
  sbc.l AbsoluteData,x // A -= $48

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB12, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FB24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w SBCResultCheckE
  beq Pass61
  Fail61:
    PrintText(Fail, $FB32, 4) // Load Text To VRAM Lo Bytes
    bra Fail61
  Pass61:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckE
    bne Fail61
    PrintText(Pass, $FB32, 4) // Load Text To VRAM Lo Bytes

    /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal8Bit, $FB42, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.b #$50 // A = $50
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #0 // X = 0
  lda.b #$49 // A = $49
  sbc.l AbsoluteData,x // A -= $50

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB52, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FB64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w SBCResultCheckF
  beq Pass62
  Fail62:
    PrintText(Fail, $FB72, 4) // Load Text To VRAM Lo Bytes
    bra Fail62
  Pass62:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckF
    bne Fail62
    PrintText(Pass, $FB72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal16Bit, $FB82, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.w #$4998 // A = $4998
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #0 // X = 0
  lda.w #$4999 // A = $4999
  sbc.l AbsoluteData,x // A -= $4998

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB92, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FBA4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w SBCResultCheckG
  beq Pass63
  Fail63:
    PrintText(Fail, $FBB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail63
  Pass63:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckG
    bne Fail63
    PrintText(Pass, $FBB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal16Bit, $FBC2, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.w #$5000 // A = $5000
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #0 // X = 0
  lda.w #$4999 // A = $4999
  sbc.l AbsoluteData,x // A -= $5000

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FBD2, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FBE4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w SBCResultCheckH
  beq Pass64
  Fail64:
    PrintText(Fail, $FBF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail64
  Pass64:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckH
    bne Fail64
    PrintText(Pass, $FBF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $100, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(SBCAddrY, $F902, 26) // Load Text To VRAM Lo Bytes
  
  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.b #$7E // A = $7E
  sta.b AbsoluteData // Store Absolute Data
  ldy.w #0 // Y = 0
  lda.b #$7F // A = $7F
  sbc AbsoluteData,y // A -= $7E

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
  cmp.w SBCResultCheckA
  beq Pass65
  Fail65:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail65
  Pass65:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckA
    bne Fail65
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA42, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.b #$80 // A = $80
  sta.b AbsoluteData // Store Absolute Data
  ldy.w #0 // Y = 0
  lda.b #$7F // A = $7F
  sbc AbsoluteData,y // A -= $80

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
  cmp.w SBCResultCheckB
  beq Pass66
  Fail66:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail66
  Pass66:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckB
    bne Fail66
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FA82, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.w #$7FFE // A = $7FFE
  sta.b AbsoluteData // Store Absolute Data
  ldy.w #0 // Y = 0
  lda.w #$7FFF // A = $7FFF
  sbc AbsoluteData,y // A -= $7FFE

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
  cpx.w SBCResultCheckC
  beq Pass67
  Fail67:
    PrintText(Fail, $FAB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail67
  Pass67:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckC
    bne Fail67
    PrintText(Pass, $FAB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FAC2, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.w #$8000 // A = $8000
  sta.b AbsoluteData // Store Absolute Data
  ldy.w #0 // Y = 0
  lda.w #$7FFF // A = $7FFF
  sbc AbsoluteData,y // A -= $8000

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
  cpx.w SBCResultCheckD
  beq Pass68
  Fail68:
    PrintText(Fail, $FAF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail68
  Pass68:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckD
    bne Fail68
    PrintText(Pass, $FAF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal8Bit, $FB02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.b #$48 // A = $48
  sta.b AbsoluteData // Store Absolute Data
  ldy.w #0 // Y = 0
  lda.b #$49 // A = $49
  sbc AbsoluteData,y // A -= $48

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB12, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FB24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w SBCResultCheckE
  beq Pass69
  Fail69:
    PrintText(Fail, $FB32, 4) // Load Text To VRAM Lo Bytes
    bra Fail69
  Pass69:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckE
    bne Fail69
    PrintText(Pass, $FB32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal8Bit, $FB42, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.b #$50 // A = $50
  sta.b AbsoluteData // Store Absolute Data
  ldy.w #0 // Y = 0
  lda.b #$49 // A = $49
  sbc AbsoluteData,y // A -= $50

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB52, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FB64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w SBCResultCheckF
  beq Pass70
  Fail70:
    PrintText(Fail, $FB72, 4) // Load Text To VRAM Lo Bytes
    bra Fail70
  Pass70:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckF
    bne Fail70
    PrintText(Pass, $FB72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal16Bit, $FB82, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.w #$4998 // A = $4998
  sta.b AbsoluteData // Store Absolute Data
  ldy.w #0 // Y = 0
  lda.w #$4999 // A = $4999
  sbc AbsoluteData,y // A -= $4998

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB92, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FBA4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w SBCResultCheckG
  beq Pass71
  Fail71:
    PrintText(Fail, $FBB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail71
  Pass71:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckG
    bne Fail71
    PrintText(Pass, $FBB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal16Bit, $FBC2, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.w #$5000 // A = $5000
  sta.b AbsoluteData // Store Absolute Data
  ldy.w #0 // Y = 0
  lda.w #$4999 // A = $4999
  sbc AbsoluteData,y // A -= $5000

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FBD2, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FBE4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w SBCResultCheckH
  beq Pass72
  Fail72:
    PrintText(Fail, $FBF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail72
  Pass72:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckH
    bne Fail72
    PrintText(Pass, $FBF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $100, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(SBCDPX, $F902, 26) // Load Text To VRAM Lo Bytes
  
  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.b #$7E // A = $7E
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #0 // X = 0
  lda.b #$7F // A = $7F
  sbc.b AbsoluteData,x // A -= $7E

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
  cmp.w SBCResultCheckA
  beq Pass73
  Fail73:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail73
  Pass73:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckA
    bne Fail73
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  // Print Modes Text
  PrintText(Binary8Bit, $FA42, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.b #$80 // A = $80
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #0 // X = 0
  lda.b #$7F // A = $7F
  sbc.b AbsoluteData,x // A -= $80

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
  cmp.w SBCResultCheckB
  beq Pass74
  Fail74:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail74
  Pass74:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckB
    bne Fail74
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FA82, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.w #$7FFE // A = $7FFE
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #0 // X = 0
  lda.w #$7FFF // A = $7FFF
  sbc.b AbsoluteData,x // A -= $7FFE

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
  cpx.w SBCResultCheckC
  beq Pass75
  Fail75:
    PrintText(Fail, $FAB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail75
  Pass75:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckC
    bne Fail75
    PrintText(Pass, $FAB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FAC2, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.w #$8000 // A = $8000
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #0 // X = 0
  lda.w #$7FFF // A = $7FFF
  sbc.b AbsoluteData,x // A -= $8000

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
  cpx.w SBCResultCheckD
  beq Pass76
  Fail76:
    PrintText(Fail, $FAF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail76
  Pass76:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckD
    bne Fail76
    PrintText(Pass, $FAF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal8Bit, $FB02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.b #$48 // A = $48
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #0 // X = 0
  lda.b #$49 // A = $49
  sbc.b AbsoluteData,x // A -= $48

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB12, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FB24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w SBCResultCheckE
  beq Pass77
  Fail77:
    PrintText(Fail, $FB32, 4) // Load Text To VRAM Lo Bytes
    bra Fail77
  Pass77:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckE
    bne Fail77
    PrintText(Pass, $FB32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal8Bit, $FB42, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.b #$50 // A = $50
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #0 // X = 0
  lda.b #$49 // A = $49
  sbc.b AbsoluteData,x // A -= $50

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB52, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FB64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w SBCResultCheckF
  beq Pass78
  Fail78:
    PrintText(Fail, $FB72, 4) // Load Text To VRAM Lo Bytes
    bra Fail78
  Pass78:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckF
    bne Fail78
    PrintText(Pass, $FB72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal16Bit, $FB82, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.w #$4998 // A = $4998
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #0 // X = 0
  lda.w #$4999 // A = $4999
  sbc.b AbsoluteData,x // A -= $4998

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB92, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FBA4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w SBCResultCheckG
  beq Pass79
  Fail79:
    PrintText(Fail, $FBB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail79
  Pass79:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckG
    bne Fail79
    PrintText(Pass, $FBB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal16Bit, $FBC2, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.w #$5000 // A = $5000
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #0 // X = 0
  lda.w #$4999 // A = $4999
  sbc.b AbsoluteData,x // A -= $5000

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FBD2, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FBE4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w SBCResultCheckH
  beq Pass80
  Fail80:
    PrintText(Fail, $FBF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail80
  Pass80:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckH
    bne Fail80
    PrintText(Pass, $FBF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $100, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(SBCDPIndirectX, $F902, 26) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.b #$7E // A = $7E
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  ldx.w #0 // X = 0
  lda.b #$7F // A = $7F
  sbc (IndirectData,x) // A -= $7E

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
  cmp.w SBCResultCheckA
  beq Pass81
  Fail81:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail81
  Pass81:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckA
    bne Fail81
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA42, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.b #$80 // A = $80
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  ldx.w #0 // X = 0
  lda.b #$7F // A = $7F
  sbc (IndirectData,x) // A -= $80

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
  cmp.w SBCResultCheckB
  beq Pass82
  Fail82:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail82
  Pass82:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckB
    bne Fail82
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FA82, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.w #$7FFE // A = $7FFE
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  ldx.w #0 // X = 0
  lda.w #$7FFF // A = $7FFF
  sbc (IndirectData,x) // A -= $7FFE

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
  cpx.w SBCResultCheckC
  beq Pass83
  Fail83:
    PrintText(Fail, $FAB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail83
  Pass83:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckC
    bne Fail83
    PrintText(Pass, $FAB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  // Print Modes Text
  PrintText(Binary16Bit, $FAC2, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.w #$8000 // A = $8000
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  ldx.w #0 // X = 0
  lda.w #$7FFF // A = $7FFF
  sbc (IndirectData,x) // A -= $8000

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
  cpx.w SBCResultCheckD
  beq Pass84
  Fail84:
    PrintText(Fail, $FAF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail84
  Pass84:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckD
    bne Fail84
    PrintText(Pass, $FAF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal8Bit, $FB02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.b #$48 // A = $48
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  ldx.w #0 // X = 0
  lda.b #$49 // A = $49
  sbc (IndirectData,x) // A -= $48

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB12, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FB24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w SBCResultCheckE
  beq Pass85
  Fail85:
    PrintText(Fail, $FB32, 4) // Load Text To VRAM Lo Bytes
    bra Fail85
  Pass85:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckE
    bne Fail85
    PrintText(Pass, $FB32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal8Bit, $FB42, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.b #$50 // A = $50
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  ldx.w #0 // X = 0
  lda.b #$49 // A = $49
  sbc (IndirectData,x) // A -= $50

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB52, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FB64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w SBCResultCheckF
  beq Pass86
  Fail86:
    PrintText(Fail, $FB72, 4) // Load Text To VRAM Lo Bytes
    bra Fail86
  Pass86:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckF
    bne Fail86
    PrintText(Pass, $FB72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal16Bit, $FB82, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.w #$4998 // A = $4998
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  ldx.w #0 // X = 0
  lda.w #$4999 // A = $4999
  sbc (IndirectData,x) // A -= $4998

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB92, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FBA4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w SBCResultCheckG
  beq Pass87
  Fail87:
    PrintText(Fail, $FBB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail87
  Pass87:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckG
    bne Fail87
    PrintText(Pass, $FBB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal16Bit, $FBC2, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.w #$5000 // A = $5000
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  ldx.w #0 // X = 0
  lda.w #$4999 // A = $4999
  sbc (IndirectData,x) // A -= $5000

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FBD2, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FBE4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w SBCResultCheckH
  beq Pass88
  Fail88:
    PrintText(Fail, $FBF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail88
  Pass88:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckH
    bne Fail88
    PrintText(Pass, $FBF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $100, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(SBCDPIndirectY, $F902, 26) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.b #$7E // A = $7E
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  ldy.w #0 // Y = 0
  lda.b #$7F // A = $7F
  sbc (IndirectData),y // A -= $7E

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
  cmp.w SBCResultCheckA
  beq Pass89
  Fail89:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail89
  Pass89:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckA
    bne Fail89
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA42, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.b #$80 // A = $80
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  ldy.w #0 // Y = 0
  lda.b #$7F // A = $7F
  sbc (IndirectData),y // A -= $80

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
  cmp.w SBCResultCheckB
  beq Pass90
  Fail90:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail90
  Pass90:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckB
    bne Fail90
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FA82, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.w #$7FFE // A = $7FFE
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  ldy.w #0 // Y = 0
  lda.w #$7FFF // A = $7FFF
  sbc (IndirectData),y // A -= $7FFE

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
  cpx.w SBCResultCheckC
  beq Pass91
  Fail91:
    PrintText(Fail, $FAB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail91
  Pass91:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckC
    bne Fail91
    PrintText(Pass, $FAB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FAC2, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.w #$8000 // A = $8000
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  ldy.w #0 // Y = 0
  lda.w #$7FFF // A = $7FFF
  sbc (IndirectData),y // A -= $8000

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
  cpx.w SBCResultCheckD
  beq Pass92
  Fail92:
    PrintText(Fail, $FAF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail92
  Pass92:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckD
    bne Fail92
    PrintText(Pass, $FAF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal8Bit, $FB02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.b #$48 // A = $48
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  ldy.w #0 // Y = 0
  lda.b #$49 // A = $49
  sbc (IndirectData),y // A -= $48

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB12, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FB24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w SBCResultCheckE
  beq Pass93
  Fail93:
    PrintText(Fail, $FB32, 4) // Load Text To VRAM Lo Bytes
    bra Fail93
  Pass93:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckE
    bne Fail93
    PrintText(Pass, $FB32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal8Bit, $FB42, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.b #$50 // A = $50
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  ldy.w #0 // Y = 0
  lda.b #$49 // A = $49
  sbc (IndirectData),y // A -= $50

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB52, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FB64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w SBCResultCheckF
  beq Pass94
  Fail94:
    PrintText(Fail, $FB72, 4) // Load Text To VRAM Lo Bytes
    bra Fail94
  Pass94:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckF
    bne Fail94
    PrintText(Pass, $FB72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal16Bit, $FB82, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.w #$4998 // A = $4998
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  ldy.w #0 // Y = 0
  lda.w #$4999 // A = $4999
  sbc (IndirectData),y // A -= $4998

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB92, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FBA4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w SBCResultCheckG
  beq Pass95
  Fail95:
    PrintText(Fail, $FBB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail95
  Pass95:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckG
    bne Fail95
    PrintText(Pass, $FBB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal16Bit, $FBC2, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.w #$5000 // A = $5000
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  ldy.w #0 // Y = 0
  lda.w #$4999 // A = $4999
  sbc (IndirectData),y // A -= $5000

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FBD2, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FBE4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w SBCResultCheckH
  beq Pass96
  Fail96:
    PrintText(Fail, $FBF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail96
  Pass96:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckH
    bne Fail96
    PrintText(Pass, $FBF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $100, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(SBCDPIndirectLongY, $F902, 26) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.b #$7E // A = $7E
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  ldy.w #0 // Y = 0
  lda.b #$7F // A = $7F
  sbc [IndirectData],y // A -= $7E

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
  cmp.w SBCResultCheckA
  beq Pass97
  Fail97:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail97
  Pass97:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckA
    bne Fail97
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA42, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.b #$80 // A = $80
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  ldy.w #0 // Y = 0
  lda.b #$7F // A = $7F
  sbc [IndirectData],y // A -= $80

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
  cmp.w SBCResultCheckB
  beq Pass98
  Fail98:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail98
  Pass98:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckB
    bne Fail98
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FA82, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.w #$7FFE // A = $7FFE
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  ldy.w #0 // Y = 0
  lda.w #$7FFF // A = $7FFF
  sbc [IndirectData],y // A -= $7FFE

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
  cpx.w SBCResultCheckC
  beq Pass99
  Fail99:
    PrintText(Fail, $FAB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail99
  Pass99:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckC
    bne Fail99
    PrintText(Pass, $FAB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FAC2, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.w #$8000 // A = $8000
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  ldy.w #0 // Y = 0
  lda.w #$7FFF // A = $7FFF
  sbc [IndirectData],y // A -= $8000

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
  cpx.w SBCResultCheckD
  beq Pass100
  Fail100:
    PrintText(Fail, $FAF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail100
  Pass100:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckD
    bne Fail100
    PrintText(Pass, $FAF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal8Bit, $FB02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.b #$48 // A = $48
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  ldy.w #0 // Y = 0
  lda.b #$49 // A = $49
  sbc [IndirectData],y // A -= $48

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB12, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FB24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w SBCResultCheckE
  beq Pass101
  Fail101:
    PrintText(Fail, $FB32, 4) // Load Text To VRAM Lo Bytes
    bra Fail101
  Pass101:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckE
    bne Fail101
    PrintText(Pass, $FB32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal8Bit, $FB42, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.b #$50 // A = $50
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  ldy.w #0 // Y = 0
  lda.b #$49 // A = $49
  sbc [IndirectData],y // A -= $50

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB52, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FB64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w SBCResultCheckF
  beq Pass102
  Fail102:
    PrintText(Fail, $FB72, 4) // Load Text To VRAM Lo Bytes
    bra Fail102
  Pass102:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckF
    bne Fail102
    PrintText(Pass, $FB72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal16Bit, $FB82, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.w #$4998 // A = $4998
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  ldy.w #0 // Y = 0
  lda.w #$4999 // A = $4999
  sbc [IndirectData],y // A -= $4998

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB92, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FBA4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w SBCResultCheckG
  beq Pass103
  Fail103:
    PrintText(Fail, $FBB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail103
  Pass103:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckG
    bne Fail103
    PrintText(Pass, $FBB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal16Bit, $FBC2, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.w #$5000 // A = $5000
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  ldy.w #0 // Y = 0
  lda.w #$4999 // A = $4999
  sbc [IndirectData],y // A -= $5000

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FBD2, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FBE4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w SBCResultCheckH
  beq Pass104
  Fail104:
    PrintText(Fail, $FBF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail104
  Pass104:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckH
    bne Fail104
    PrintText(Pass, $FBF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $100, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(SBCSRS, $F902, 26) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.b #$7E // A = $7E
  pha // Push A To Stack
  lda.b #$7F // A = $7F
  sbc $01,s // A -= $7E

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
  cmp.w SBCResultCheckA
  beq Pass105
  Fail105:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail105
  Pass105:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckA
    bne Fail105
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA42, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.b #$80 // A = $80
  pha // Push A To Stack
  lda.b #$7F // A = $7F
  sbc $01,s // A -= $80

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
  cmp.w SBCResultCheckB
  beq Pass106
  Fail106:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail106
  Pass106:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckB
    bne Fail106
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FA82, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.w #$7FFE // A = $7FFE
  pha // Push A To Stack
  lda.w #$7FFF // A = $7FFF
  sbc $01,s // A -= $7FFE

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
  cpx.w SBCResultCheckC
  beq Pass107
  Fail107:
    PrintText(Fail, $FAB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail107
  Pass107:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckC
    bne Fail107
    PrintText(Pass, $FAB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FAC2, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.w #$8000 // A = $8000
  pha // Push A To Stack
  lda.w #$7FFF // A = $7FFF
  sbc $01,s // A -= $8000

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
  cpx.w SBCResultCheckD
  beq Pass108
  Fail108:
    PrintText(Fail, $FAF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail108
  Pass108:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckD
    bne Fail108
    PrintText(Pass, $FAF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal8Bit, $FB02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.b #$48 // A = $48
  pha // Push A To Stack
  lda.b #$49 // A = $49
  sbc $01,s // A -= $48

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB12, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FB24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w SBCResultCheckE
  beq Pass109
  Fail109:
    PrintText(Fail, $FB32, 4) // Load Text To VRAM Lo Bytes
    bra Fail109
  Pass109:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckE
    bne Fail109
    PrintText(Pass, $FB32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal8Bit, $FB42, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.b #$50 // A = $50
  pha // Push A To Stack
  lda.b #$49 // A = $49
  sbc $01,s // A -= $50

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB52, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FB64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w SBCResultCheckF
  beq Pass110
  Fail110:
    PrintText(Fail, $FB72, 4) // Load Text To VRAM Lo Bytes
    bra Fail110
  Pass110:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckF
    bne Fail110
    PrintText(Pass, $FB72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal16Bit, $FB82, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.w #$4998 // A = $4998
  pha // Push A To Stack
  lda.w #$4999 // A = $4999
  sbc $01,s // A -= $4998

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB92, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FBA4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w SBCResultCheckG
  beq Pass111
  Fail111:
    PrintText(Fail, $FBB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail111
  Pass111:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckG
    bne Fail111
    PrintText(Pass, $FBB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal16Bit, $FBC2, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.w #$5000 // A = $5000
  pha // Push A To Stack
  lda.w #$4999 // A = $4999
  sbc $01,s // A -= $5000

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FBD2, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FBE4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w SBCResultCheckH
  beq Pass112
  Fail112:
    PrintText(Fail, $FBF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail112
  Pass112:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckH
    bne Fail112
    PrintText(Pass, $FBF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $100, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(SBCSRSY, $F902, 26) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.b #$7E // A = $7E
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Indirect Address
  phx // Push X To Stack
  ldy.w #0 // Y = 0
  lda.b #$7F // A = $7F
  sbc ($01,s),y // A -= $7E

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
  cmp.w SBCResultCheckA
  beq Pass113
  Fail113:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail113
  Pass113:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckA
    bne Fail113
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA42, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.b #$80 // A = $80
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Indirect Address
  phx // Push X To Stack
  ldy.w #0 // Y = 0
  lda.b #$7F // A = $7F
  sbc ($01,s),y // A -= $80

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
  cmp.w SBCResultCheckB
  beq Pass114
  Fail114:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail114
  Pass114:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckB
    bne Fail114
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FA82, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.w #$7FFE // A = $7FFE
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Indirect Address
  phx // Push X To Stack
  ldy.w #0 // Y = 0
  lda.w #$7FFF // A = $7FFF
  sbc ($01,s),y // A -= $7FFE

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
  cpx.w SBCResultCheckC
  beq Pass115
  Fail115:
    PrintText(Fail, $FAB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail115
  Pass115:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckC
    bne Fail115
    PrintText(Pass, $FAB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FAC2, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.w #$8000 // A = $8000
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Indirect Address
  phx // Push X To Stack
  ldy.w #0 // Y = 0
  lda.w #$7FFF // A = $7FFF
  sbc ($01,s),y // A -= $8000

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
  cpx.w SBCResultCheckD
  beq Pass116
  Fail116:
    PrintText(Fail, $FAF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail116
  Pass116:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckD
    bne Fail116
    PrintText(Pass, $FAF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal8Bit, $FB02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.b #$48 // A = $48
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Indirect Address
  phx // Push X To Stack
  ldy.w #0 // Y = 0
  lda.b #$49 // A = $49
  sbc ($01,s),y // A -= $48

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB12, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FB24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w SBCResultCheckE
  beq Pass117
  Fail117:
    PrintText(Fail, $FB32, 4) // Load Text To VRAM Lo Bytes
    bra Fail117
  Pass117:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckE
    bne Fail117
    PrintText(Pass, $FB32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal8Bit, $FB42, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.b #$50 // A = $50
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Indirect Address
  phx // Push X To Stack
  ldy.w #0 // Y = 0
  lda.b #$49 // A = $49
  sbc ($01,s),y // A -= $50

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB52, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FB64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w SBCResultCheckF
  beq Pass118
  Fail118:
    PrintText(Fail, $FB72, 4) // Load Text To VRAM Lo Bytes
    bra Fail118
  Pass118:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckF
    bne Fail118
    PrintText(Pass, $FB72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal16Bit, $FB82, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.w #$4998 // A = $4998
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Indirect Address
  phx // Push X To Stack
  ldy.w #0 // Y = 0
  lda.w #$4999 // A = $4999
  sbc ($01,s),y // A -= $4998

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FB92, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FBA4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w SBCResultCheckG
  beq Pass119
  Fail119:
    PrintText(Fail, $FBB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail119
  Pass119:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckG
    bne Fail119
    PrintText(Pass, $FBB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Decimal16Bit, $FBC2, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$08 // Set Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  sec // Set Carry Flag

  // Run Test
  lda.w #$5000 // A = $5000
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Indirect Address
  phx // Push X To Stack
  ldy.w #0 // Y = 0
  lda.w #$4999 // A = $4999
  sbc ($01,s),y // A -= $5000

  // Store Result & Processor Status Flag Data
  sta.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FBD2, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FBE4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w SBCResultCheckH
  beq Pass120
  Fail120:
    PrintText(Fail, $FBF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail120
  Pass120:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckH
    bne Fail120
    PrintText(Pass, $FBF2, 4) // Load Text To VRAM Lo Bytes

Loop:
  jmp Loop

Title:
  db "CPU Test SBC (Sub With Borrow):"

PageBreak:
  db "------------------------------"

Key:
  db "Modes | Result | NVZC | Test |"
Binary8Bit:
  db "BIN,8"
Binary16Bit:
  db "BIN,16"
Decimal8Bit:
  db "BCD,8"
Decimal16Bit:
  db "BCD,16"
Fail:
  db "FAIL"
Pass:
  db "PASS"

SBCConst:
  db "SBC #const   (Opcode: $E9)"
SBCAddr:
  db "SBC addr     (Opcode: $ED)"
SBCLong:
  db "SBC long     (Opcode: $EF)"
SBCDP:
  db "SBC dp       (Opcode: $E5)"
SBCDPIndirect:
  db "SBC (dp)     (Opcode: $F2)"
SBCDPIndirectLong:
  db "SBC [dp]     (Opcode: $E7)"
SBCAddrX:
  db "SBC addr,X   (Opcode: $FD)"
SBCLongX:
  db "SBC long,X   (Opcode: $FF)"
SBCAddrY:
  db "SBC addr,Y   (Opcode: $F9)"
SBCDPX:
  db "SBC dp,X     (Opcode: $F5)"
SBCDPIndirectX:
  db "SBC (dp,X)   (Opcode: $E1)"
SBCDPIndirectY:
  db "SBC (dp),Y   (Opcode: $F1)"
SBCDPIndirectLongY:
  db "SBC [dp],Y   (Opcode: $F7)"
SBCSRS:
  db "SBC sr,S     (Opcode: $E3)"
SBCSRSY:
  db "SBC (sr,S),Y (Opcode: $F3)"

SBCResultCheckA:
  db $00
PSRResultCheckA:
  db $27

SBCResultCheckB:
  db $FF
PSRResultCheckB:
  db $E4

SBCResultCheckC:
  dw $0000
PSRResultCheckC:
  db $07

SBCResultCheckD:
  dw $FFFF
PSRResultCheckD:
  db $C4

SBCResultCheckE:
  db $00
PSRResultCheckE:
  db $2F

SBCResultCheckF:
  db $99
PSRResultCheckF:
  db $AC

SBCResultCheckG:
  dw $0000
PSRResultCheckG:
  db $0F

SBCResultCheckH:
  dw $9999
PSRResultCheckH:
  db $8C

BGCHR:
  include "Font8x8.asm" // Include BG 1BPP 8x8 Tile Font Character Data (1016 Bytes)
BGPAL:
  dw $7800, $7FFF // Blue / White Palette (4 Bytes)
BGCLEAR:
  dw $0020 // BG Clear Character Space " " Fixed Word