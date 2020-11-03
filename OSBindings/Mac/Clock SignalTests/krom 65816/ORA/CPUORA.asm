// SNES 65816 CPU Test ORA (OR With Memory) demo by krom (Peter Lemon):
arch snes.cpu
output "CPUORA.sfc", create

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
  PrintText(Title, $F882, 30) // Load Text To VRAM Lo Bytes

  // Print Page Break Text
  PrintText(PageBreak, $F8C2, 30) // Load Text To VRAM Lo Bytes

  // Print Syntax/Opcode Text
  PrintText(ORAConst, $F902, 26) // Load Text To VRAM Lo Bytes

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
  lda.b #$00 // A = $00
  ora.b #$00 // A |= $00

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
  cmp.w ORAResultCheckA
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
  lda.b #$0F // A = $0F
  ora.b #$F0 // A |= $F0

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
  cmp.w ORAResultCheckB
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
  lda.w #$0000 // A = $0000
  ora.w #$0000 // A |= $0000

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
  cpx.w ORAResultCheckC
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
  lda.w #$00FF // A = $00FF
  ora.w #$FF00 // A |= $FF00

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
  cpx.w ORAResultCheckD
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
  PrintText(ORAAddr, $F902, 26) // Load Text To VRAM Lo Bytes
  
  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag
  clv // Clear Overflow Flag

  // Run Test
  lda.b #$00 // A = $00
  sta.b AbsoluteData // Store Absolute Data
  lda.b #$00 // A = $00
  ora.w AbsoluteData // A |= $00

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
  cmp.w ORAResultCheckA
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
  lda.b #$F0 // A = $F0
  sta.b AbsoluteData // Store Absolute Data
  lda.b #$0F // A = $0F
  ora.w AbsoluteData // A |= $F0

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
  cmp.w ORAResultCheckB
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
  lda.w #$0000 // A = $0000
  sta.b AbsoluteData // Store Absolute Data
  lda.w #$0000 // A = $0000
  ora.w AbsoluteData // A |= $0000

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
  cpx.w ORAResultCheckC
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
  lda.w #$FF00 // A = $FF00
  sta.b AbsoluteData // Store Absolute Data
  lda.w #$00FF // A = $00FF
  ora.w AbsoluteData // A |= $FF00

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
  cpx.w ORAResultCheckD
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
  PrintText(ORALong, $F902, 26) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag
  clv // Clear Overflow Flag

  // Run Test
  lda.b #$00 // A = $00
  sta.b AbsoluteData // Store Absolute Data
  lda.b #$00 // A = $00
  ora.l AbsoluteData // A |= $00

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
  cmp.w ORAResultCheckA
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
  lda.b #$F0 // A = $F0
  sta.b AbsoluteData // Store Absolute Data
  lda.b #$0F // A = $0F
  ora.l AbsoluteData // A |= $F0

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
  cmp.w ORAResultCheckB
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
  lda.w #$0000 // A = $0000
  sta.b AbsoluteData // Store Absolute Data
  lda.w #$0000 // A = $0000
  ora.l AbsoluteData // A |= $0000

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
  cpx.w ORAResultCheckC
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
  lda.w #$FF00 // A = $FF00
  sta.b AbsoluteData // Store Absolute Data
  lda.w #$00FF // A = $00FF
  ora.l AbsoluteData // A |= $FF00

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
  cpx.w ORAResultCheckD
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
  PrintText(ORADP, $F902, 26) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag
  clv // Clear Overflow Flag

  // Run Test
  lda.b #$00 // A = $00
  sta.b AbsoluteData // Store Absolute Data
  lda.b #$00 // A = $00
  ora.b AbsoluteData // A |= $00

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
  cmp.w ORAResultCheckA
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
  lda.b #$F0 // A = $F0
  sta.b AbsoluteData // Store Absolute Data
  lda.b #$0F // A = $0F
  ora.b AbsoluteData // A |= $F0

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
  cmp.w ORAResultCheckB
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
  lda.w #$0000 // A = $0000
  sta.b AbsoluteData // Store Absolute Data
  lda.w #$0000 // A = $0000
  ora.b AbsoluteData // A |= $0000

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
  cpx.w ORAResultCheckC
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
  lda.w #$FF00 // A = $FF00
  sta.b AbsoluteData // Store Absolute Data
  lda.w #$00FF // A = $00FF
  ora.b AbsoluteData // A |= $FF00

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
  cpx.w ORAResultCheckD
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
  PrintText(ORADPIndirect, $F902, 26) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag
  clv // Clear Overflow Flag

  // Run Test
  lda.b #$00 // A = $00
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  lda.b #$00 // A = $00
  ora (IndirectData) // A |= $00

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
  cmp.w ORAResultCheckA
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
  lda.b #$F0 // A = $F0
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  lda.b #$0F // A = $0F
  ora (IndirectData) // A |= $F0

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
  cmp.w ORAResultCheckB
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
  lda.w #$0000 // A = $0000
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  lda.w #$0000 // A = $0000
  ora (IndirectData) // A |= $0000

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
  cpx.w ORAResultCheckC
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
  lda.w #$FF00 // A = $FF00
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  lda.w #$00FF // A = $00FF
  ora (IndirectData) // A |= $FF00

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
  cpx.w ORAResultCheckD
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
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $80, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(ORADPIndirectLong, $F902, 26) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag
  clv // Clear Overflow Flag

  // Run Test
  lda.b #$00 // A = $00
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  lda.b #$00 // A = $00
  ora [IndirectData] // A |= $00

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
  cmp.w ORAResultCheckA
  beq Pass21
  Fail21:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail21
  Pass21:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckA
    bne Fail21
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA42, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.b #$F0 // A = $F0
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  lda.b #$0F // A = $0F
  ora [IndirectData] // A |= $F0

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
  cmp.w ORAResultCheckB
  beq Pass22
  Fail22:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail22
  Pass22:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckB
    bne Fail22
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FA82, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.w #$0000 // A = $0000
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  lda.w #$0000 // A = $0000
  ora [IndirectData] // A |= $0000

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
  cpx.w ORAResultCheckC
  beq Pass23
  Fail23:
    PrintText(Fail, $FAB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail23
  Pass23:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckC
    bne Fail23
    PrintText(Pass, $FAB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FAC2, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.w #$FF00 // A = $FF00
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  lda.w #$00FF // A = $00FF
  ora [IndirectData] // A |= $FF00

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
  cpx.w ORAResultCheckD
  beq Pass24
  Fail24:
    PrintText(Fail, $FAF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail24
  Pass24:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckD
    bne Fail24
    PrintText(Pass, $FAF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $80, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(ORAAddrX, $F902, 26) // Load Text To VRAM Lo Bytes
  
  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag
  clv // Clear Overflow Flag

  // Run Test
  lda.b #$00 // A = $00
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #0 // X = 0
  lda.b #$00 // A = $00
  ora.w AbsoluteData,x // A |= $00

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
  cmp.w ORAResultCheckA
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
  clc // Clear Carry Flag

  // Run Test
  lda.b #$F0 // A = $F0
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #0 // X = 0
  lda.b #$0F // A = $0F
  ora.w AbsoluteData,x // A |= $F0

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
  cmp.w ORAResultCheckB
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
  lda.w #$0000 // A = $0000
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #0 // X = 0
  lda.w #$0000 // A = $0000
  ora.w AbsoluteData,x // A |= $0000

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
  cpx.w ORAResultCheckC
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
  clc // Clear Carry Flag

  // Run Test
  lda.w #$FF00 // A = $FF00
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #0 // X = 0
  lda.w #$00FF // A = $00FF
  ora.w AbsoluteData,x // A |= $FF00

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
  cpx.w ORAResultCheckD
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
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $80, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(ORALongX, $F902, 26) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag
  clv // Clear Overflow Flag

  // Run Test
  lda.b #$00 // A = $00
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #0 // X = 0
  lda.b #$00 // A = $00
  ora.l AbsoluteData,x // A |= $00

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
  cmp.w ORAResultCheckA
  beq Pass29
  Fail29:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail29
  Pass29:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckA
    bne Fail29
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA42, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.b #$F0 // A = $F0
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #0 // X = 0
  lda.b #$0F // A = $0F
  ora.l AbsoluteData,x // A |= $F0

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
  cmp.w ORAResultCheckB
  beq Pass30
  Fail30:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail30
  Pass30:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckB
    bne Fail30
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FA82, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.w #$0000 // A = $0000
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #0 // X = 0
  lda.w #$0000 // A = $0000
  ora.l AbsoluteData,x // A |= $0000

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
  cpx.w ORAResultCheckC
  beq Pass31
  Fail31:
    PrintText(Fail, $FAB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail31
  Pass31:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckC
    bne Fail31
    PrintText(Pass, $FAB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FAC2, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.w #$FF00 // A = $FF00
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #0 // X = 0
  lda.w #$00FF // A = $00FF
  ora.l AbsoluteData,x // A |= $FF00

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
  cpx.w ORAResultCheckD
  beq Pass32
  Fail32:
    PrintText(Fail, $FAF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail32
  Pass32:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckD
    bne Fail32
    PrintText(Pass, $FAF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $80, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(ORAAddrY, $F902, 26) // Load Text To VRAM Lo Bytes
  
  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag
  clv // Clear Overflow Flag

  // Run Test
  lda.b #$00 // A = $00
  sta.b AbsoluteData // Store Absolute Data
  ldy.w #0 // Y = 0
  lda.b #$00 // A = $00
  ora AbsoluteData,y // A |= $00

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
  cmp.w ORAResultCheckA
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
  clc // Clear Carry Flag

  // Run Test
  lda.b #$F0 // A = $F0
  sta.b AbsoluteData // Store Absolute Data
  ldy.w #0 // Y = 0
  lda.b #$0F // A = $0F
  ora AbsoluteData,y // A |= $F0

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
  cmp.w ORAResultCheckB
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
  lda.w #$0000 // A = $0000
  sta.b AbsoluteData // Store Absolute Data
  ldy.w #0 // Y = 0
  lda.w #$0000 // A = $0000
  ora AbsoluteData,y // A |= $0000

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
  cpx.w ORAResultCheckC
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
  // Print Modes Text
  PrintText(Binary16Bit, $FAC2, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.w #$FF00 // A = $FF00
  sta.b AbsoluteData // Store Absolute Data
  ldy.w #0 // Y = 0
  lda.w #$00FF // A = $00FF
  ora AbsoluteData,y // A |= $FF00

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
  cpx.w ORAResultCheckD
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
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $80, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(ORADPX, $F902, 26) // Load Text To VRAM Lo Bytes
  
  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag
  clv // Clear Overflow Flag

  // Run Test
  lda.b #$00 // A = $00
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #0 // X = 0
  lda.b #$00 // A = $00
  ora.b AbsoluteData,x // A |= $00

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
  cmp.w ORAResultCheckA
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
  PrintText(Binary8Bit, $FA42, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.b #$F0 // A = $F0
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #0 // X = 0
  lda.b #$0F // A = $0F
  ora.b AbsoluteData,x // A |= $F0

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
  cmp.w ORAResultCheckB
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
  PrintText(Binary16Bit, $FA82, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.w #$0000 // A = $0000
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #0 // X = 0
  lda.w #$0000 // A = $0000
  ora.b AbsoluteData,x // A |= $0000

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
  cpx.w ORAResultCheckC
  beq Pass39
  Fail39:
    PrintText(Fail, $FAB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail39
  Pass39:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckC
    bne Fail39
    PrintText(Pass, $FAB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FAC2, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.w #$FF00 // A = $FF00
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #0 // X = 0
  lda.w #$00FF // A = $00FF
  ora.b AbsoluteData,x // A |= $FF00

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
  cpx.w ORAResultCheckD
  beq Pass40
  Fail40:
    PrintText(Fail, $FAF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail40
  Pass40:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckD
    bne Fail40
    PrintText(Pass, $FAF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $80, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(ORADPIndirectX, $F902, 26) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag
  clv // Clear Overflow Flag

  // Run Test
  lda.b #$00 // A = $00
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  ldx.w #0 // X = 0
  lda.b #$00 // A = $00
  ora (IndirectData,x) // A |= $00

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
  cmp.w ORAResultCheckA
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
  clc // Clear Carry Flag

  // Run Test
  lda.b #$F0 // A = $F0
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  ldx.w #0 // X = 0
  lda.b #$0F // A = $0F
  ora (IndirectData,x) // A |= $F0

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
  cmp.w ORAResultCheckB
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
  lda.w #$0000 // A = $0000
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  ldx.w #0 // X = 0
  lda.w #$0000 // A = $0000
  ora (IndirectData,x) // A |= $0000

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
  cpx.w ORAResultCheckC
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
  clc // Clear Carry Flag

  // Run Test
  lda.w #$FF00 // A = $FF00
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  ldx.w #0 // X = 0
  lda.w #$00FF // A = $00FF
  ora (IndirectData,x) // A |= $FF00

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
  cpx.w ORAResultCheckD
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
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $80, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(ORADPIndirectY, $F902, 26) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag
  clv // Clear Overflow Flag

  // Run Test
  lda.b #$00 // A = $00
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  ldy.w #0 // Y = 0
  lda.b #$00 // A = $00
  ora (IndirectData),y // A |= $00

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
  cmp.w ORAResultCheckA
  beq Pass45
  Fail45:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail45
  Pass45:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckA
    bne Fail45
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA42, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.b #$F0 // A = $F0
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  ldy.w #0 // Y = 0
  lda.b #$0F // A = $0F
  ora (IndirectData),y // A |= $F0

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
  cmp.w ORAResultCheckB
  beq Pass46
  Fail46:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail46
  Pass46:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckB
    bne Fail46
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FA82, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.w #$0000 // A = $0000
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  ldy.w #0 // Y = 0
  lda.w #$0000 // A = $0000
  ora (IndirectData),y // A |= $0000

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
  cpx.w ORAResultCheckC
  beq Pass47
  Fail47:
    PrintText(Fail, $FAB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail47
  Pass47:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckC
    bne Fail47
    PrintText(Pass, $FAB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FAC2, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.w #$FF00 // A = $FF00
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  ldy.w #0 // Y = 0
  lda.w #$00FF // A = $00FF
  ora (IndirectData),y // A |= $FF00

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
  cpx.w ORAResultCheckD
  beq Pass48
  Fail48:
    PrintText(Fail, $FAF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail48
  Pass48:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckD
    bne Fail48
    PrintText(Pass, $FAF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $80, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(ORADPIndirectLongY, $F902, 26) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag
  clv // Clear Overflow Flag

  // Run Test
  lda.b #$00 // A = $00
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  ldy.w #0 // Y = 0
  lda.b #$00 // A = $00
  ora [IndirectData],y // A |= $00

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
  cmp.w ORAResultCheckA
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
  clc // Clear Carry Flag

  // Run Test
  lda.b #$F0 // A = $F0
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  ldy.w #0 // Y = 0
  lda.b #$0F // A = $0F
  ora [IndirectData],y // A |= $F0

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
  cmp.w ORAResultCheckB
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
  lda.w #$0000 // A = $0000
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  ldy.w #0 // Y = 0
  lda.w #$0000 // A = $0000
  ora [IndirectData],y // A |= $0000

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
  cpx.w ORAResultCheckC
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
  clc // Clear Carry Flag

  // Run Test
  lda.w #$FF00 // A = $FF00
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Address Word
  stx.b IndirectData // Store Indirect Data
  ldy.w #0 // Y = 0
  lda.w #$00FF // A = $00FF
  ora [IndirectData],y // A |= $FF00

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
  cpx.w ORAResultCheckD
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
  PrintText(ORASRS, $F902, 26) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag
  clv // Clear Overflow Flag

  // Run Test
  lda.b #$00 // A = $00
  pha // Push A To Stack
  lda.b #$00 // A = $00
  ora $01,s // A |= $00

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
  cmp.w ORAResultCheckA
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
  PrintText(Binary8Bit, $FA42, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.b #$F0 // A = $F0
  pha // Push A To Stack
  lda.b #$0F // A = $0F
  ora $01,s // A |= $F0

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
  cmp.w ORAResultCheckB
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
  PrintText(Binary16Bit, $FA82, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.w #$0000 // A = $0000
  pha // Push A To Stack
  lda.w #$0000 // A = $0000
  ora $01,s // A |= $0000

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
  cpx.w ORAResultCheckC
  beq Pass55
  Fail55:
    PrintText(Fail, $FAB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail55
  Pass55:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckC
    bne Fail55
    PrintText(Pass, $FAB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary16Bit, $FAC2, 6) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  rep #$20 // Set 16-Bit Accumulator
  clc // Clear Carry Flag

  // Run Test
  lda.w #$FF00 // A = $FF00
  pha // Push A To Stack
  lda.w #$00FF // A = $00FF
  ora $01,s // A |= $FF00

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
  cpx.w ORAResultCheckD
  beq Pass56
  Fail56:
    PrintText(Fail, $FAF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail56
  Pass56:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckD
    bne Fail56
    PrintText(Pass, $FAF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  WaitNMI() // Wait For VSync

  ClearVRAM(BGCLEAR, $FA00, $80, 0) // Clear VRAM Map To Fixed Tile Word

  WaitNMI() // Wait For VSync

  // Print Syntax/Opcode Text
  PrintText(ORASRSY, $F902, 26) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Modes Text
  PrintText(Binary8Bit, $FA02, 5) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$08 // Reset Decimal Flag
  sep #$20 // Set 8-Bit Accumulator
  clc // Clear Carry Flag
  clv // Clear Overflow Flag

  // Run Test
  lda.b #$00 // A = $00
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Indirect Address
  phx // Push X To Stack
  ldy.w #0 // Y = 0
  lda.b #$00 // A = $00
  ora ($01,s),y // A |= $00

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
  cmp.w ORAResultCheckA
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
  clc // Clear Carry Flag

  // Run Test
  lda.b #$F0 // A = $F0
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Indirect Address
  phx // Push X To Stack
  ldy.w #0 // Y = 0
  lda.b #$0F // A = $0F
  ora ($01,s),y // A |= $F0

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
  cmp.w ORAResultCheckB
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
  lda.w #$0000 // A = $0000
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Indirect Address
  phx // Push X To Stack
  ldy.w #0 // Y = 0
  lda.w #$0000 // A = $0000
  ora ($01,s),y // A |= $0000

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
  cpx.w ORAResultCheckC
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
  clc // Clear Carry Flag

  // Run Test
  lda.w #$FF00 // A = $FF00
  sta.b AbsoluteData // Store Absolute Data
  ldx.w #AbsoluteData // X = Absolute Data Indirect Address
  phx // Push X To Stack
  ldy.w #0 // Y = 0
  lda.w #$00FF // A = $00FF
  ora ($01,s),y // A |= $FF00

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
  cpx.w ORAResultCheckD
  beq Pass60
  Fail60:
    PrintText(Fail, $FAF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail60
  Pass60:
    lda.b PSRFlagData // A = Processor Status Flag Data
    cmp.w PSRResultCheckD
    bne Fail60
    PrintText(Pass, $FAF2, 4) // Load Text To VRAM Lo Bytes

Loop:
  jmp Loop

Title:
  db "CPU Test ORA (OR With Memory):"

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

ORAConst:
  db "ORA #const   (Opcode: $09)"
ORAAddr:
  db "ORA addr     (Opcode: $0D)"
ORALong:
  db "ORA long     (Opcode: $0F)"
ORADP:
  db "ORA dp       (Opcode: $05)"
ORADPIndirect:
  db "ORA (dp)     (Opcode: $12)"
ORADPIndirectLong:
  db "ORA [dp]     (Opcode: $07)"
ORAAddrX:
  db "ORA addr,X   (Opcode: $1D)"
ORALongX:
  db "ORA long,X   (Opcode: $1F)"
ORAAddrY:
  db "ORA addr,Y   (Opcode: $19)"
ORADPX:
  db "ORA dp,X     (Opcode: $15)"
ORADPIndirectX:
  db "ORA (dp,X)   (Opcode: $01)"
ORADPIndirectY:
  db "ORA (dp),Y   (Opcode: $11)"
ORADPIndirectLongY:
  db "ORA [dp],Y   (Opcode: $17)"
ORASRS:
  db "ORA sr,S     (Opcode: $03)"
ORASRSY:
  db "ORA (sr,S),Y (Opcode: $13)"

ORAResultCheckA:
  db $00
PSRResultCheckA:
  db $26

ORAResultCheckB:
  db $FF
PSRResultCheckB:
  db $A4

ORAResultCheckC:
  dw $0000
PSRResultCheckC:
  db $06

ORAResultCheckD:
  dw $FFFF
PSRResultCheckD:
  db $84

BGCHR:
  include "Font8x8.asm" // Include BG 1BPP 8x8 Tile Font Character Data (1016 Bytes)
BGPAL:
  dw $7800, $7FFF // Blue / White Palette (4 Bytes)
BGCLEAR:
  dw $0020 // BG Clear Character Space " " Fixed Word