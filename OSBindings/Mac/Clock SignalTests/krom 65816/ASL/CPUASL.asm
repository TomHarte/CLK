// SNES 65816 CPU Test ASL (Arithmetic Shift Left) demo by krom (Peter Lemon):
arch snes.cpu
output "CPUASL.sfc", create

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
  PrintText(Title, $F882, 26) // Load Text To VRAM Lo Bytes

  // Print Page Break Text
  PrintText(PageBreak, $F8C2, 30) // Load Text To VRAM Lo Bytes

  // Print Syntax/Opcode Text
  PrintText(ASLA, $F902, 26) // Load Text To VRAM Lo Bytes

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
  clv // Clear Overflow Flag

  // Run Test
  lda.b #$80 // A = $80
  asl // A <<= 1

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
  cmp.w ASLResultCheckA
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
  clc // Clear Carry Flag

  // Run Test
  lda.b #$7F // A = $7F
  asl // A <<= 1

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
  cmp.w ASLResultCheckB
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
  lda.w #$8000 // A = $8000
  asl // A <<= 1

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
  cpx.w ASLResultCheckC
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
  clc // Clear Carry Flag

  // Run Test
  lda.w #$7FFF // A = $7FFF
  asl // A <<= 1

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
  cpx.w ASLResultCheckD
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
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $80, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(ASLAddr, $F902, 26) // Load Text To VRAM Lo Bytes
  
  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag
  clv // Clear Overflow Flag

  // Run Test
  lda.b #$80 // A = $80
  sta.b AbsoluteData // Store Absolute Data
  asl.w AbsoluteData // Memory <<= 1

  // Store Processor Status Flag Data
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(AbsoluteData, $FA12, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FA24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b AbsoluteData // A = Result Data
  cmp.w ASLResultCheckA
  beq Pass5
  Fail5:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail5
  Pass5:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckA
    bne Fail5
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA42, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.b #$7F // A = $7F
  sta.b AbsoluteData // Store Absolute Data
  asl.w AbsoluteData // Memory <<= 1

  // Store Processor Status Flag Data
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(AbsoluteData, $FA52, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FA64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b AbsoluteData // A = Result Data
  cmp.w ASLResultCheckB
  beq Pass6
  Fail6:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail6
  Pass6:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckB
    bne Fail6
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FA82, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.w #$8000 // A = $8000
  sta.b AbsoluteData // Store Absolute Data
  asl.w AbsoluteData // Memory <<= 1

  // Store Processor Status Flag Data
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(AbsoluteData, $FA92, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FAA4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b AbsoluteData // X = Result Data
  cpx.w ASLResultCheckC
  beq Pass7
  Fail7:
    PrintText(Fail, $FAB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail7
  Pass7:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckC
    bne Fail7
    PrintText(Pass, $FAB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FAC2, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.w #$7FFF // A = $7FFF
  sta.b AbsoluteData // Store Absolute Data
  asl.w AbsoluteData // Memory <<= 1

  // Store Processor Status Flag Data
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(AbsoluteData, $FAD2, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FAE4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b AbsoluteData // X = Result Data
  cpx.w ASLResultCheckD
  beq Pass8
  Fail8:
    PrintText(Fail, $FAF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail8
  Pass8:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckD
    bne Fail8
    PrintText(Pass, $FAF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $80, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(ASLDP, $F902, 26) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag
  clv // Clear Overflow Flag

  // Run Test
  lda.b #$80 // A = $80
  sta.b AbsoluteData // Store Absolute Data
  asl.b AbsoluteData // Memory <<= 1

  // Store Processor Status Flag Data
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(AbsoluteData, $FA12, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FA24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b AbsoluteData // A = Result Data
  cmp.w ASLResultCheckA
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
  clc // Clear Carry Flag

  // Run Test
  lda.b #$7F // A = $7F
  sta.b AbsoluteData // Store Absolute Data
  asl.b AbsoluteData // Memory <<= 1

  // Store Processor Status Flag Data
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(AbsoluteData, $FA52, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FA64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b AbsoluteData // A = Result Data
  cmp.w ASLResultCheckB
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
  lda.w #$8000 // A = $8000
  sta.b AbsoluteData // Store Absolute Data
  asl.b AbsoluteData // Memory <<= 1

  // Store Processor Status Flag Data
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(AbsoluteData, $FA92, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FAA4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b AbsoluteData // X = Result Data
  cpx.w ASLResultCheckC
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
  clc // Clear Carry Flag

  // Run Test
  lda.w #$7FFF // A = $7FFF
  sta.b AbsoluteData // Store Absolute Data
  asl.b AbsoluteData // Memory <<= 1

  // Store Processor Status Flag Data
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(AbsoluteData, $FAD2, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FAE4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b AbsoluteData // X = Result Data
  cpx.w ASLResultCheckD
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
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $80, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(ASLAddrX, $F902, 26) // Load Text To VRAM Lo Bytes
  
  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag
  clv // Clear Overflow Flag

  // Run Test
  lda.b #$80 // A = $80
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #0 // X = 0
  asl.w AbsoluteData,x // Memory <<= 1

  // Store Processor Status Flag Data
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(AbsoluteData, $FA12, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FA24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b AbsoluteData // A = Result Data
  cmp.w ASLResultCheckA
  beq Pass13
  Fail13:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail13
  Pass13:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckA
    bne Fail13
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA42, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.b #$7F // A = $7F
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #0 // X = 0
  asl.w AbsoluteData,x // Memory <<= 1

  // Store Processor Status Flag Data
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(AbsoluteData, $FA52, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FA64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b AbsoluteData // A = Result Data
  cmp.w ASLResultCheckB
  beq Pass14
  Fail14:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail14
  Pass14:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckB
    bne Fail14
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FA82, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.w #$8000 // A = $8000
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #0 // X = 0
  asl.w AbsoluteData,x // Memory <<= 1

  // Store Processor Status Flag Data
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(AbsoluteData, $FA92, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FAA4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b AbsoluteData // X = Result Data
  cpx.w ASLResultCheckC
  beq Pass15
  Fail15:
    PrintText(Fail, $FAB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail15
  Pass15:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckC
    bne Fail15
    PrintText(Pass, $FAB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FAC2, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.w #$7FFF // A = $7FFF
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #0 // X = 0
  asl.w AbsoluteData,x // Memory <<= 1

  // Store Processor Status Flag Data
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(AbsoluteData, $FAD2, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FAE4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b AbsoluteData // X = Result Data
  cpx.w ASLResultCheckD
  beq Pass16
  Fail16:
    PrintText(Fail, $FAF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail16
  Pass16:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckD
    bne Fail16
    PrintText(Pass, $FAF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $80, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(ASLDPX, $F902, 26) // Load Text To VRAM Lo Bytes
  
  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag
  clv // Clear Overflow Flag

  // Run Test
  lda.b #$80 // A = $80
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #0 // X = 0
  asl.b AbsoluteData,x // Memory <<= 1

  // Store Processor Status Flag Data
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(AbsoluteData, $FA12, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FA24) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b AbsoluteData // A = Result Data
  cmp.w ASLResultCheckA
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
  clc // Clear Carry Flag

  // Run Test
  lda.b #$7F // A = $7F
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #0 // X = 0
  asl.b AbsoluteData,x // Memory <<= 1

  // Store Processor Status Flag Data
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(AbsoluteData, $FA52, 1) // Print Result Data
  PrintPSR(PSRFlagData, $FA64) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  lda.b AbsoluteData // A = Result Data
  cmp.w ASLResultCheckB
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
  lda.w #$8000 // A = $8000
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #0 // X = 0
  asl.b AbsoluteData,x // Memory <<= 1

  // Store Processor Status Flag Data
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(AbsoluteData, $FA92, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FAA4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b AbsoluteData // X = Result Data
  cpx.w ASLResultCheckC
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
  clc // Clear Carry Flag

  // Run Test
  lda.w #$7FFF // A = $7FFF
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #0 // X = 0
  asl.b AbsoluteData,x // Memory <<= 1

  // Store Processor Status Flag Data
  php // Push Processor Status Register To Stack
  sep #$20 // Set 8-Bit Accumulator
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Result & Processor Status Flag Data
  PrintValue(AbsoluteData, $FAD2, 2) // Print Result Data
  PrintPSR(PSRFlagData, $FAE4) // Print Processor Status Flag Data

  // Check Result & Processor Status Flag Data
  ldx.b AbsoluteData // X = Result Data
  cpx.w ASLResultCheckD
  beq Pass20
  Fail20:
    PrintText(Fail, $FAF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail20
  Pass20:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckD
    bne Fail20
    PrintText(Pass, $FAF2, 4) // Load Text To VRAM Lo Bytes

Loop:
  jmp Loop

Title:
  db "CPU Test ASL (Shift Left):"

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

ASLA:
  db "ASL A        (Opcode: $0A)"
ASLAddr:
  db "ASL addr     (Opcode: $0E)"
ASLDP:
  db "ASL dp       (Opcode: $06)"
ASLAddrX:
  db "ASL addr,X   (Opcode: $1E)"
ASLDPX:
  db "ASL dp,X     (Opcode: $16)"

ASLResultCheckA:
  db $00
PSRResultCheckA:
  db $27

ASLResultCheckB:
  db $FE
PSRResultCheckB:
  db $A4

ASLResultCheckC:
  dw $0000
PSRResultCheckC:
  db $07

ASLResultCheckD:
  dw $FFFE
PSRResultCheckD:
  db $84

BGCHR:
  include "Font8x8.asm" // Include BG 1BPP 8x8 Tile Font Character Data (1016 Bytes)
BGPAL:
  dw $7800, $7FFF // Blue / White Palette (4 Bytes)
BGCLEAR:
  dw $0020 // BG Clear Character Space " " Fixed Word