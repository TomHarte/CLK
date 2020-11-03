// SNES 65816 CPU Test PHL (Push Pull) demo by krom (Peter Lemon):
arch snes.cpu
output "CPUPHL.sfc", create

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
  PrintText(Title, $F882, 25) // Load Text To VRAM Lo Bytes

  // Print Page Break Text
  PrintText(PageBreak, $F8C2, 30) // Load Text To VRAM Lo Bytes

  // Print Syntax/Opcode Text
  PrintText(PEAPSH, $F902, 26) // Load Text To VRAM Lo Bytes

  // Print Key Text
  PrintText(Key, $F982, 30) // Load Text To VRAM Lo Bytes

  // Print Page Break Text
  PrintText(PageBreak, $F9C2, 30) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$C3 // Reset NVZC Flags
  sep #$20 // Set 8-Bit Accumulator
  sep #$10 // Set 8-Bit X/Y

  // Run Test
  pea $DEAD // Stack = $DEAD

  // Store Result & Processor Status Flag Data
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory
  rep #$10 // Set 16-Bit X/Y
  plx // Pull X Register From Stack
  stx.b ResultData // Store Result To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA12, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FA24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w PSHResultCheckA
  beq Pass1
  Fail1:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail1
  Pass1:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSHPSRResultCheckA
    bne Fail1
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FA42, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$C3 // Reset NVZC Flags
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y

  // Run Test
  pea $BEEF // Stack = $BEEF

  // Store Result & Processor Status Flag Data
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory
  plx // Pull X Register From Stack
  stx.b ResultData // Store Result To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA52, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FA64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w PSHResultCheckB
  beq Pass2
  Fail2:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail2
  Pass2:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSHPSRResultCheckB
    bne Fail2
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $40, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(PEIPSH, $F902, 26) // Load Text To VRAM Lo Bytes
  
  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Load Test Data
  ldx.w #$DEAD // X = $DEAD
  stx.b AbsoluteData // Store Absolute Data

  // Setup Flags
  rep #$C3 // Reset NVZC Flags
  sep #$20 // Set 8-Bit Accumulator
  sep #$10 // Set 8-Bit X/Y

  // Run Test
  pei (AbsoluteData) // Stack = $DEAD

  // Store Result & Processor Status Flag Data
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory
  rep #$10 // Set 16-Bit X/Y
  plx // Pull X Register From Stack
  stx.b ResultData // Store Result To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA12, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FA24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w PSHResultCheckC
  beq Pass3
  Fail3:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail3
  Pass3:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSHPSRResultCheckC
    bne Fail3
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FA42, 6) // Load Text To VRAM Lo Bytes

  // Load Test Data
  ldx.w #$BEEF // X = $BEEF
  stx.b AbsoluteData // Store Absolute Data

  // Setup Flags
  rep #$C3 // Reset NVZC Flags
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y

  // Run Test
  pei (AbsoluteData) // Stack = $BEEF

  // Store Result & Processor Status Flag Data
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory
  plx // Pull X Register From Stack
  stx.b ResultData // Store Result To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA52, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FA64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w PSHResultCheckD
  beq Pass4
  Fail4:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail4
  Pass4:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSHPSRResultCheckD
    bne Fail4
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $40, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(PERPSH, $F902, 26) // Load Text To VRAM Lo Bytes
  
  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$C3 // Reset NVZC Flags
  sep #$20 // Set 8-Bit Accumulator
  sep #$10 // Set 8-Bit X/Y

  // Run Test
  per AbsoluteData // Stack = PC Relative Indirect Address

  // Store Result & Processor Status Flag Data
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory
  rep #$10 // Set 16-Bit X/Y
  plx // Pull X Register From Stack
  stx.b ResultData // Store Result To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA12, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FA24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w PSHResultCheckE
  beq Pass5
  Fail5:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail5
  Pass5:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSHPSRResultCheckE
    bne Fail5
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FA42, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$C3 // Reset NVZC Flags
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y

  // Run Test
  per AbsoluteData // Stack = PC Relative Indirect Address

  // Store Result & Processor Status Flag Data
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory
  plx // Pull X Register From Stack
  stx.b ResultData // Store Result To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA52, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FA64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w PSHResultCheckF
  beq Pass6
  Fail6:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail6
  Pass6:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSHPSRResultCheckF
    bne Fail6
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $40, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(PHAPSH, $F902, 26) // Load Text To VRAM Lo Bytes
  
  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$C3 // Reset NVZC Flags
  sep #$20 // Set 8-Bit Accumulator

  // Run Test
  lda.b #$7F // A = $7F
  pha // Stack = A

  // Store Result & Processor Status Flag Data
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory
  pla // Pull A Register From Stack
  sta.b ResultData // Store Result To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA12, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FA24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w PSHResultCheckG
  beq Pass7
  Fail7:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail7
  Pass7:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSHPSRResultCheckG
    bne Fail7
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FA42, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$C3 // Reset NVZC Flags
  rep #$20 // Set 16-Bit Accumulator

  // Run Test
  lda.w #$7FFF // A = $7FFF
  pha // Stack = A

  // Store Result & Processor Status Flag Data
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory
  plx // Pull X Register From Stack
  stx.b ResultData // Store Result To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA52, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FA64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w PSHResultCheckH
  beq Pass8
  Fail8:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail8
  Pass8:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSHPSRResultCheckH
    bne Fail8
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $40, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(PHBPSH, $F902, 26) // Load Text To VRAM Lo Bytes
  
  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$C3 // Reset NVZC Flags
  sep #$20 // Set 8-Bit Accumulator
  sep #$10 // Set 8-Bit X/Y

  // Run Test
  phb // Stack = Bank Register

  // Store Result & Processor Status Flag Data
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory
  rep #$10 // Set 16-Bit X/Y
  pla // Pull A Register From Stack
  sta.b ResultData // Store Result To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA12, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FA24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w PSHResultCheckI
  beq Pass9
  Fail9:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail9
  Pass9:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSHPSRResultCheckI
    bne Fail9
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FA42, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$C3 // Reset NVZC Flags
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y

  // Run Test
  phb // Stack = Bank Register

  // Store Result & Processor Status Flag Data
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory
  pla // Pull A Register From Stack
  sta.b ResultData // Store Result To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA52, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FA64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w PSHResultCheckJ
  beq Pass10
  Fail10:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail10
  Pass10:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSHPSRResultCheckJ
    bne Fail10
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $40, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(PHDPSH, $F902, 26) // Load Text To VRAM Lo Bytes
  
  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$C3 // Reset NVZC Flags
  sep #$20 // Set 8-Bit Accumulator
  sep #$10 // Set 8-Bit X/Y

  // Run Test
  phd // Stack = Direct Page Register

  // Store Result & Processor Status Flag Data
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory
  rep #$10 // Set 16-Bit X/Y
  plx // Pull X Register From Stack
  stx.b ResultData // Store Result To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA12, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FA24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w PSHResultCheckK
  beq Pass11
  Fail11:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail11
  Pass11:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSHPSRResultCheckK
    bne Fail11
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FA42, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$C3 // Reset NVZC Flags
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y

  // Run Test
  phd // Stack = Direct Page Register

  // Store Result & Processor Status Flag Data
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory
  plx // Pull X Register From Stack
  stx.b ResultData // Store Result To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA52, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FA64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w PSHResultCheckL
  beq Pass12
  Fail12:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail12
  Pass12:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSHPSRResultCheckL
    bne Fail12
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $40, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(PHKPSH, $F902, 26) // Load Text To VRAM Lo Bytes
  
  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$C3 // Reset NVZC Flags
  sep #$20 // Set 8-Bit Accumulator
  sep #$10 // Set 8-Bit X/Y

  // Run Test
  phk // Stack = Program Bank Register

  // Store Result & Processor Status Flag Data
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory
  rep #$10 // Set 16-Bit X/Y
  pla // Pull A Register From Stack
  sta.b ResultData // Store Result To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA12, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FA24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w PSHResultCheckM
  beq Pass13
  Fail13:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail13
  Pass13:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSHPSRResultCheckM
    bne Fail13
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FA42, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$C3 // Reset NVZC Flags
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y

  // Run Test
  phk // Stack = Program Bank Register

  // Store Result & Processor Status Flag Data
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory
  pla // Pull A Register From Stack
  sta.b ResultData // Store Result To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA52, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FA64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w PSHResultCheckN
  beq Pass14
  Fail14:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail14
  Pass14:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSHPSRResultCheckN
    bne Fail14
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $40, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(PHPPSH, $F902, 26) // Load Text To VRAM Lo Bytes
  
  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$C3 // Reset NVZC Flags
  sep #$20 // Set 8-Bit Accumulator
  sep #$10 // Set 8-Bit X/Y

  // Run Test
  php // Stack = Push Processor Status Register

  // Store Result & Processor Status Flag Data
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory
  rep #$10 // Set 16-Bit X/Y
  pla // Pull A Register From Stack
  sta.b ResultData // Store Result To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA12, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FA24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w PSHResultCheckO
  beq Pass15
  Fail15:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail15
  Pass15:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSHPSRResultCheckO
    bne Fail15
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FA42, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$C3 // Reset NVZC Flags
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y

  // Run Test
  php // Stack = Push Processor Status Register

  // Store Result & Processor Status Flag Data
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory
  pla // Pull A Register From Stack
  sta.b ResultData // Store Result To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA52, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FA64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w PSHResultCheckP
  beq Pass16
  Fail16:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail16
  Pass16:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSHPSRResultCheckP
    bne Fail16
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $40, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(PHXPSH, $F902, 26) // Load Text To VRAM Lo Bytes
  
  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$C3 // Reset NVZC Flags
  sep #$10 // Set 8-Bit X/Y

  // Run Test
  ldx.b #$7F // X = $7F
  phx // Stack = X

  // Store Result & Processor Status Flag Data
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory
  rep #$10 // Set 16-Bit X/Y
  pla // Pull A Register From Stack
  sta.b ResultData // Store Result To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA12, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FA24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w PSHResultCheckQ
  beq Pass17
  Fail17:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail17
  Pass17:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSHPSRResultCheckQ
    bne Fail17
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FA42, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$C3 // Reset NVZC Flags
  rep #$10 // Set 16-Bit X/Y

  // Run Test
  ldx.w #$7FFF // X = $7FFF
  phx // Stack = X

  // Store Result & Processor Status Flag Data
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory
  plx // Pull X Register From Stack
  stx.b ResultData // Store Result To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA52, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FA64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w PSHResultCheckR
  beq Pass18
  Fail18:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail18
  Pass18:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSHPSRResultCheckR
    bne Fail18
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $40, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(PHYPSH, $F902, 26) // Load Text To VRAM Lo Bytes
  
  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$C3 // Reset NVZC Flags
  sep #$10 // Set 8-Bit X/Y

  // Run Test
  ldy.b #$7F // Y = $7F
  phy // Stack = Y

  // Store Result & Processor Status Flag Data
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory
  rep #$10 // Set 16-Bit X/Y
  pla // Pull A Register From Stack
  sta.b ResultData // Store Result To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA12, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FA24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w PSHResultCheckS
  beq Pass19
  Fail19:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail19
  Pass19:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSHPSRResultCheckS
    bne Fail19
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FA42, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$C3 // Reset NVZC Flags
  rep #$10 // Set 16-Bit X/Y

  // Run Test
  ldy.w #$7FFF // Y = $7FFF
  phy // Stack = Y

  // Store Result & Processor Status Flag Data
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory
  plx // Pull X Register From Stack
  stx.b ResultData // Store Result To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA52, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FA64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w PSHResultCheckT
  beq Pass20
  Fail20:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail20
  Pass20:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSHPSRResultCheckT
    bne Fail20
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $40, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(PLAPUL, $F902, 26) // Load Text To VRAM Lo Bytes
  
  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Store Data
  lda.b #$00 // A = $00
  pha // Push A To Stack

  // Setup Flags
  rep #$C3 // Reset NVZC Flags
  sep #$20 // Set 8-Bit Accumulator

  // Run Test
  pla // A = Stack

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
  cmp.w PULResultCheckA
  beq Pass21
  Fail21:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail21
  Pass21:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PULPSRResultCheckA
    bne Fail21
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA42, 5) // Load Text To VRAM Lo Bytes

  // Store Data
  lda.b #$FF // A = $FF
  pha // Push A To Stack

  // Setup Flags
  rep #$C3 // Reset NVZC Flags
  sep #$20 // Set 8-Bit Accumulator

  // Run Test
  pla // A = Stack

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
  cmp.w PULResultCheckB
  beq Pass22
  Fail22:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail22
  Pass22:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PULPSRResultCheckB
    bne Fail22
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FA82, 6) // Load Text To VRAM Lo Bytes

  // Store Data
  ldx.w #$0000 // X = $0000
  phx // Push X To Stack

  // Setup Flags
  rep #$C3 // Reset NVZC Flags
  rep #$20 // Set 16-Bit Accumulator

  // Run Test
  pla // A = Stack

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
  cpx.w PULResultCheckC
  beq Pass23
  Fail23:
    PrintText(Fail, $FAB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail23
  Pass23:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PULPSRResultCheckC
    bne Fail23
    PrintText(Pass, $FAB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FAC2, 6) // Load Text To VRAM Lo Bytes

  // Store Data
  ldx.w #$FFFF // X = $FFFF
  phx // Push X To Stack

  // Setup Flags
  rep #$C3 // Reset NVZC Flags
  rep #$20 // Set 16-Bit Accumulator

  // Run Test
  pla // A = Stack

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
  cpx.w PULResultCheckD
  beq Pass24
  Fail24:
    PrintText(Fail, $FAF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail24
  Pass24:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PULPSRResultCheckD
    bne Fail24
    PrintText(Pass, $FAF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $80, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(PLBPUL, $F902, 26) // Load Text To VRAM Lo Bytes
  
  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Store Data
  phb // Push Data Bank Register To Stack

  // Setup Flags
  rep #$C3 // Reset NVZC Flags
  sep #$20 // Set 8-Bit Accumulator
  sep #$10 // Set 8-Bit X/Y

  // Run Test
  plb // Data Bank Register = Stack

  // Store Result & Processor Status Flag Data
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory
  phb // Push Data Bank Register To Stack
  rep #$10 // Set 16-Bit X/Y
  pla // Pull A From Stack
  sta.b ResultData // Store Result To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA12, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FA24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w PULResultCheckE
  beq Pass25
  Fail25:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail25
  Pass25:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PULPSRResultCheckE
    bne Fail25
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FA42, 6) // Load Text To VRAM Lo Bytes

  // Store Data
  phb // Push Data Bank Register To Stack

  // Setup Flags
  rep #$C3 // Reset NVZC Flags
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y

  // Run Test
  plb // Data Bank Register = Stack

  // Store Result & Processor Status Flag Data
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory
  phb // Push Data Bank Register To Stack
  pla // Pull A From Stack
  sta.b ResultData // Store Result To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA52, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FA64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w PULResultCheckF
  beq Pass26
  Fail26:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail26
  Pass26:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PULPSRResultCheckF
    bne Fail26
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $40, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(PLDPUL, $F902, 26) // Load Text To VRAM Lo Bytes
  
  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Store Data
  phd // Push Direct Page Register To Stack

  // Setup Flags
  rep #$C3 // Reset NVZC Flags
  sep #$20 // Set 8-Bit Accumulator
  sep #$10 // Set 8-Bit X/Y

  // Run Test
  pld // Direct Page Register = Stack

  // Store Result & Processor Status Flag Data
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory
  phd // Push Data Bank Register To Stack
  rep #$10 // Set 16-Bit X/Y
  plx // Pull X From Stack
  stx.b ResultData // Store Result To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA12, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FA24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w PULResultCheckG
  beq Pass27
  Fail27:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail27
  Pass27:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PULPSRResultCheckG
    bne Fail27
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FA42, 6) // Load Text To VRAM Lo Bytes

  // Store Data
  phd // Push Direct Page Register To Stack

  // Setup Flags
  rep #$C3 // Reset NVZC Flags
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y

  // Run Test
  pld // Direct Page Register = Stack

  // Store Result & Processor Status Flag Data
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory
  phd // Push Data Bank Register To Stack
  plx // Pull X From Stack
  stx.b ResultData // Store Result To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA52, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FA64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w PULResultCheckH
  beq Pass28
  Fail28:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail28
  Pass28:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PULPSRResultCheckH
    bne Fail28
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $40, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(PLPPUL, $F902, 26) // Load Text To VRAM Lo Bytes
  
  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Store Data
  php // Push Processor Status Register To Stack

  // Setup Flags
  rep #$C3 // Reset NVZC Flags
  sep #$20 // Set 8-Bit Accumulator
  sep #$10 // Set 8-Bit X/Y

  // Run Test
  plp // Processor Status Register = Stack

  // Store Result & Processor Status Flag Data
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory
  php // Push Processor Status Register To Stack
  rep #$10 // Set 16-Bit X/Y
  pla // Pull A From Stack
  sta.b ResultData // Store Result To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA12, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FA24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w PULResultCheckI
  beq Pass29
  Fail29:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail29
  Pass29:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PULPSRResultCheckI
    bne Fail29
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FA42, 6) // Load Text To VRAM Lo Bytes

  // Store Data
  php // Push Processor Status Register To Stack

  // Setup Flags
  rep #$C3 // Reset NVZC Flags
  rep #$20 // Set 16-Bit Accumulator
  rep #$10 // Set 16-Bit X/Y

  // Run Test
  plp // Processor Status Register = Stack

  // Store Result & Processor Status Flag Data
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory
  php // Push Processor Status Register To Stack
  pla // Pull A From Stack
  sta.b ResultData // Store Result To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA52, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FA64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b ResultData // A = Result Data
  cmp.w PULResultCheckJ
  beq Pass30
  Fail30:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail30
  Pass30:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PULPSRResultCheckJ
    bne Fail30
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $40, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(PLXPUL, $F902, 26) // Load Text To VRAM Lo Bytes
  
  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Store Data
  lda.b #$00 // A = $00
  pha // Push A To Stack

  // Setup Flags
  rep #$C3 // Reset NVZC Flags
  sep #$10 // Set 8-Bit X/Y

  // Run Test
  plx // X = Stack

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
  cmp.w PULResultCheckK
  beq Pass31
  Fail31:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail31
  Pass31:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PULPSRResultCheckK
    bne Fail31
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA42, 5) // Load Text To VRAM Lo Bytes

  // Store Data
  lda.b #$FF // A = $FF
  pha // Push A To Stack

  // Setup Flags
  rep #$C3 // Reset NVZC Flags
  sep #$10 // Set 8-Bit X/Y

  // Run Test
  plx // X = Stack

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
  cmp.w PULResultCheckL
  beq Pass32
  Fail32:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail32
  Pass32:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PULPSRResultCheckL
    bne Fail32
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FA82, 6) // Load Text To VRAM Lo Bytes

  // Store Data
  ldx.w #$0000 // X = $0000
  phx // Push X To Stack

  // Setup Flags
  rep #$C3 // Reset NVZC Flags
  rep #$10 // Set 16-Bit X/Y

  // Run Test
  plx // X = Stack

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
  cpx.w PULResultCheckM
  beq Pass33
  Fail33:
    PrintText(Fail, $FAB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail33
  Pass33:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PULPSRResultCheckM
    bne Fail33
    PrintText(Pass, $FAB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FAC2, 6) // Load Text To VRAM Lo Bytes

  // Store Data
  ldx.w #$FFFF // X = $FFFF
  phx // Push X To Stack

  // Setup Flags
  rep #$C3 // Reset NVZC Flags
  rep #$10 // Set 16-Bit X/Y

  // Run Test
  plx // X = Stack

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
  cpx.w PULResultCheckN
  beq Pass34
  Fail34:
    PrintText(Fail, $FAF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail34
  Pass34:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PULPSRResultCheckN
    bne Fail34
    PrintText(Pass, $FAF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $80, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(PLYPUL, $F902, 26) // Load Text To VRAM Lo Bytes
  
  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Store Data
  lda.b #$00 // A = $00
  pha // Push A To Stack

  // Setup Flags
  rep #$C3 // Reset NVZC Flags
  sep #$10 // Set 8-Bit X/Y

  // Run Test
  ply // Y = Stack

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
  cmp.w PULResultCheckO
  beq Pass35
  Fail35:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail35
  Pass35:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PULPSRResultCheckO
    bne Fail35
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA42, 5) // Load Text To VRAM Lo Bytes

  // Store Data
  lda.b #$FF // A = $FF
  pha // Push A To Stack

  // Setup Flags
  rep #$C3 // Reset NVZC Flags
  sep #$10 // Set 8-Bit X/Y

  // Run Test
  ply // Y = Stack

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
  cmp.w PULResultCheckP
  beq Pass36
  Fail36:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail36
  Pass36:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PULPSRResultCheckP
    bne Fail36
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FA82, 6) // Load Text To VRAM Lo Bytes

  // Store Data
  ldx.w #$0000 // X = $0000
  phx // Push X To Stack

  // Setup Flags
  rep #$C3 // Reset NVZC Flags
  rep #$10 // Set 16-Bit X/Y

  // Run Test
  ply // Y = Stack

  // Store Result & Processor Status Flag Data
  sty.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FA92, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FAA4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w PULResultCheckQ
  beq Pass37
  Fail37:
    PrintText(Fail, $FAB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail37
  Pass37:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PULPSRResultCheckQ
    bne Fail37
    PrintText(Pass, $FAB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FAC2, 6) // Load Text To VRAM Lo Bytes

  // Store Data
  ldx.w #$FFFF // X = $FFFF
  phx // Push X To Stack

  // Setup Flags
  rep #$C3 // Reset NVZC Flags
  rep #$10 // Set 16-Bit X/Y

  // Run Test
  ply // Y = Stack

  // Store Result & Processor Status Flag Data
  sty.b ResultData // Store Result To Memory
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(ResultData, $FAD2, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FAE4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b ResultData // X = Result Data
  cpx.w PULResultCheckR
  beq Pass38
  Fail38:
    PrintText(Fail, $FAF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail38
  Pass38:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PULPSRResultCheckR
    bne Fail38
    PrintText(Pass, $FAF2, 4) // Load Text To VRAM Lo Bytes

Loop:
  jmp Loop

Title:
  db "CPU Test PHL (Push Pull):"

PageBreak:
  db "------------------------------"

Key:
  db "Modes | Result | NVZC | Test |"
Binary8Bit:
  db "BIN,8"
Binary16Bit:
  db "BIN,16"
Fail:
  db "FAIL"
Pass:
  db "PASS"

PEAPSH:
  db "PEA addr     (Opcode: $F4)"
PEIPSH:
  db "PEI (dp)     (Opcode: $D4)"
PERPSH:
  db "PER label    (Opcode: $62)"
PHAPSH:
  db "PHA          (Opcode: $48)"
PHBPSH:
  db "PHB          (Opcode: $8B)"
PHDPSH:
  db "PHD          (Opcode: $0B)"
PHKPSH:
  db "PHK          (Opcode: $4B)"
PHPPSH:
  db "PHP          (Opcode: $08)"
PHXPSH:
  db "PHX          (Opcode: $DA)"
PHYPSH:
  db "PHY          (Opcode: $5A)"
PLAPUL:
  db "PLA          (Opcode: $68)"
PLBPUL:
  db "PLB          (Opcode: $AB)"
PLDPUL:
  db "PLD          (Opcode: $2B)"
PLPPUL:
  db "PLP          (Opcode: $28)"
PLXPUL:
  db "PLX          (Opcode: $FA)"
PLYPUL:
  db "PLY          (Opcode: $7A)"

PSHResultCheckA:
  dw $DEAD
PSHPSRResultCheckA:
  db $34

PSHResultCheckB:
  dw $BEEF
PSHPSRResultCheckB:
  db $04

PSHResultCheckC:
  dw $DEAD
PSHPSRResultCheckC:
  db $34

PSHResultCheckD:
  dw $BEEF
PSHPSRResultCheckD:
  db $04

PSHResultCheckE:
  dw $86EF
PSHPSRResultCheckE:
  db $34

PSHResultCheckF:
  dw $87C2
PSHPSRResultCheckF:
  db $04

PSHResultCheckG:
  db $7F
PSHPSRResultCheckG:
  db $24

PSHResultCheckH:
  dw $7FFF
PSHPSRResultCheckH:
  db $04

PSHResultCheckI:
  db $00
PSHPSRResultCheckI:
  db $34

PSHResultCheckJ:
  db $00
PSHPSRResultCheckJ:
  db $04

PSHResultCheckK:
  dw $0000
PSHPSRResultCheckK:
  db $34

PSHResultCheckL:
  dw $0000
PSHPSRResultCheckL:
  db $04

PSHResultCheckM:
  db $00
PSHPSRResultCheckM:
  db $34

PSHResultCheckN:
  db $00
PSHPSRResultCheckN:
  db $04

PSHResultCheckO:
  db $34
PSHPSRResultCheckO:
  db $34

PSHResultCheckP:
  db $04
PSHPSRResultCheckP:
  db $04

PSHResultCheckQ:
  db $7F
PSHPSRResultCheckQ:
  db $34

PSHResultCheckR:
  dw $7FFF
PSHPSRResultCheckR:
  db $24

PSHResultCheckS:
  db $7F
PSHPSRResultCheckS:
  db $34

PSHResultCheckT:
  dw $7FFF
PSHPSRResultCheckT:
  db $24

PULResultCheckA:
  db $00
PULPSRResultCheckA:
  db $26

PULResultCheckB:
  db $FF
PULPSRResultCheckB:
  db $A4

PULResultCheckC:
  dw $0000
PULPSRResultCheckC:
  db $06

PULResultCheckD:
  dw $FFFF
PULPSRResultCheckD:
  db $84

PULResultCheckE:
  db $00
PULPSRResultCheckE:
  db $36

PULResultCheckF:
  db $00
PULPSRResultCheckF:
  db $06

PULResultCheckG:
  dw $0000
PULPSRResultCheckG:
  db $36

PULResultCheckH:
  dw $0000
PULPSRResultCheckH:
  db $06

PULResultCheckI:
  db $65
PULPSRResultCheckI:
  db $67

PULResultCheckJ:
  db $65
PULPSRResultCheckJ:
  db $67

PULResultCheckK:
  db $00
PULPSRResultCheckK:
  db $36

PULResultCheckL:
  db $FF
PULPSRResultCheckL:
  db $B4

PULResultCheckM:
  dw $0000
PULPSRResultCheckM:
  db $26

PULResultCheckN:
  dw $FFFF
PULPSRResultCheckN:
  db $A4

PULResultCheckO:
  db $00
PULPSRResultCheckO:
  db $36

PULResultCheckP:
  db $FF
PULPSRResultCheckP:
  db $B4

PULResultCheckQ:
  dw $0000
PULPSRResultCheckQ:
  db $26

PULResultCheckR:
  dw $FFFF
PULPSRResultCheckR:
  db $A4

BGCHR:
  include "Font8x8.asm" // Include BG 1BPP 8x8 Tile Font Character Data (1016 Bytes)
BGPAL:
  dw $7800, $7FFF // Blue / White Palette (4 Bytes)
BGCLEAR:
  dw $0020 // BG Clear Character Space " " Fixed Word