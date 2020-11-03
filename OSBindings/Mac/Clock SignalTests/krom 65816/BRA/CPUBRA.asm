// SNES 65816 CPU Test BRA (Branch) demo by krom (Peter Lemon):
arch snes.cpu
output "CPUBRA.sfc", create

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
  PrintText(Title, $F882, 22) // Load Text To VRAM Lo Bytes

  // Print Page Break Text
  PrintText(PageBreak, $F8C2, 30) // Load Text To VRAM Lo Bytes

  // Print Key Text
  PrintText(Key, $F942, 30) // Load Text To VRAM Lo Bytes

  // Print Page Break Text
  PrintText(PageBreak, $F982, 30) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Type/Opcode Text
  PrintText(BCCBranch, $F9C2, 11) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$C3 // Set NVZC Flags
  clc // Clear Carry Flag

  // Store Processor Status Flag Data
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Processor Status Flag Data
  PrintPSR(PSRFlagData, $F9E4) // Print Processor Status Flag Data

  // Run Test
  clc // Clear Carry Flag
  bcc Pass1
  Fail1:
    PrintText(Fail, $F9F2, 4) // Load Text To VRAM Lo Bytes
    bra Fail1
  Pass1:
    PrintText(Pass, $F9F2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Type/Opcode Text
  PrintText(BCSBranch, $FA02, 11) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$C3 // Reset NVZC Flags
  sec // Set Carry Flag

  // Store Processor Status Flag Data
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Processor Status Flag Data
  PrintPSR(PSRFlagData, $FA24) // Print Processor Status Flag Data

  // Run Test
  sec // Set Carry Flag
  bcs Pass2
  Fail2:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail2
  Pass2:
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Type/Opcode Text
  PrintText(BNEBranch, $FA42, 11) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$C3 // Set NVZC Flags
  rep #$02 // Reset Zero Flag

  // Store Processor Status Flag Data
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Processor Status Flag Data
  PrintPSR(PSRFlagData, $FA64) // Print Processor Status Flag Data

  // Run Test
  rep #$02 // Reset Zero Flag
  bne Pass3
  Fail3:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail3
  Pass3:
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Type/Opcode Text
  PrintText(BEQBranch, $FA82, 11) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$C3 // Reset NVZC Flags
  sep #$02 // Set Zero Flag

  // Store Processor Status Flag Data
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Processor Status Flag Data
  PrintPSR(PSRFlagData, $FAA4) // Print Processor Status Flag Data

  // Run Test
  sep #$02 // Set Zero Flag
  beq Pass4
  Fail4:
    PrintText(Fail, $FAB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail4
  Pass4:
    PrintText(Pass, $FAB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Type/Opcode Text
  PrintText(BVCBranch, $FAC2, 11) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$C3 // Set NVZC Flags
  clv // Clear Overflow Flag

  // Store Processor Status Flag Data
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Processor Status Flag Data
  PrintPSR(PSRFlagData, $FAE4) // Print Processor Status Flag Data

  // Run Test
  clv // Clear Overflow Flag
  bvc Pass5
  Fail5:
    PrintText(Fail, $FAF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail5
  Pass5:
    PrintText(Pass, $FAF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Type/Opcode Text
  PrintText(BVSBranch, $FB02, 11) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$C3 // Reset NVZC Flags
  sep #$40 // Set Overflow Flag

  // Store Processor Status Flag Data
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Processor Status Flag Data
  PrintPSR(PSRFlagData, $FB24) // Print Processor Status Flag Data

  // Run Test
  sep #$40 // Set Overflow Flag
  bvs Pass6
  Fail6:
    PrintText(Fail, $FB32, 4) // Load Text To VRAM Lo Bytes
    bra Fail6
  Pass6:
    PrintText(Pass, $FB32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Type/Opcode Text
  PrintText(BPLBranch, $FB42, 11) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$C3 // Set NVZC Flags
  rep #$80 // Reset Negative Flag

  // Store Processor Status Flag Data
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Processor Status Flag Data
  PrintPSR(PSRFlagData, $FB64) // Print Processor Status Flag Data

  // Run Test
  rep #$80 // Reset Negative Flag
  bpl Pass7
  Fail7:
    PrintText(Fail, $FB72, 4) // Load Text To VRAM Lo Bytes
    bra Fail7
  Pass7:
    PrintText(Pass, $FB72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Type/Opcode Text
  PrintText(BMIBranch, $FB82, 11) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$C3 // Reset NVZC Flags
  sep #$80 // Set Negative Flag

  // Store Processor Status Flag Data
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Processor Status Flag Data
  PrintPSR(PSRFlagData, $FBA4) // Print Processor Status Flag Data

  // Run Test
  sep #$80 // Set Negative Flag
  bmi Pass8
  Fail8:
    PrintText(Fail, $FBB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail8
  Pass8:
    PrintText(Pass, $FBB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Type/Opcode Text
  PrintText(BRABranch, $FBC2, 11) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$C3 // Reset NVZC Flags

  // Store Processor Status Flag Data
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Processor Status Flag Data
  PrintPSR(PSRFlagData, $FBE4) // Print Processor Status Flag Data

  // Run Test
  bra Pass9
  Fail9:
    PrintText(Fail, $FBF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail9
  Pass9:
    PrintText(Pass, $FBF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Type/Opcode Text
  PrintText(BRLBranch, $FC02, 11) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$C3 // Reset NVZC Flags

  // Store Processor Status Flag Data
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Processor Status Flag Data
  PrintPSR(PSRFlagData, $FC24) // Print Processor Status Flag Data

  // Run Test
  brl Pass10
  Fail10:
    PrintText(Fail, $FC32, 4) // Load Text To VRAM Lo Bytes
    bra Fail10
  Pass10:
    PrintText(Pass, $FC32, 4) // Load Text To VRAM Lo Bytes

Loop:
  jmp Loop

Title:
  db "CPU Test BRA (Branch):"

PageBreak:
  db "------------------------------"

Key:
  db "Type  | Opcode | NVZC | Test |"
Fail:
  db "FAIL"
Pass:
  db "PASS"

BCCBranch:
  db "BCC     $90"
BCSBranch:
  db "BCS     $B0"
BNEBranch:
  db "BNE     $D0"
BEQBranch:
  db "BEQ     $F0"
BVCBranch:
  db "BVC     $50"
BVSBranch:
  db "BVS     $70"
BPLBranch:
  db "BPL     $10"
BMIBranch:
  db "BMI     $30"
BRABranch:
  db "BRA     $80"
BRLBranch:
  db "BRL     $82"

BGCHR:
  include "Font8x8.asm" // Include BG 1BPP 8x8 Tile Font Character Data (1016 Bytes)
BGPAL:
  dw $7800, $7FFF // Blue / White Palette (4 Bytes)
BGCLEAR:
  dw $0020 // BG Clear Character Space " " Fixed Word