// SNES 65816 CPU Test PSR (PSR Flags) demo by krom (Peter Lemon):
arch snes.cpu
output "CPUPSR.sfc", create

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

  lda.b #%00001000 // A = Decimal Flag Bit
  jsr {#}PSRFlagTest // Test PSR Flag Data

  lda.b #%00000100 // A = Interrupt Flag Bit
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
  PrintText(Title, $F882, 25) // Load Text To VRAM Lo Bytes

  // Print Page Break Text
  PrintText(PageBreak, $F8C2, 30) // Load Text To VRAM Lo Bytes

  // Print Key Text
  PrintText(Key, $F942, 30) // Load Text To VRAM Lo Bytes

  // Print Page Break Text
  PrintText(PageBreak, $F982, 30) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Type/Opcode Text
  PrintText(CLCPSR, $F9C2, 9) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$CF // Set NVDIZC Flags

  // Run Test
  clc // Clear Carry Flag

  // Store Processor Status Flag Data
  php // Push Processor Status Register To Stack
  cld // Clear Decimal Flag
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Processor Status Flag Data
  PrintPSR(PSRFlagData, $F9E0) // Print Processor Status Flag Data

  // Check Processor Status Flag Data
  lda.b PSRFlagData // A = Processor Status Flag Data
  cmp.w PSRResultCheckA
  beq Pass1
  Fail1:
    PrintText(Fail, $F9F2, 4) // Load Text To VRAM Lo Bytes
    bra Fail1
  Pass1:
    PrintText(Pass, $F9F2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Type/Opcode Text
  PrintText(CLDPSR, $FA02, 9) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$CF // Set NVDIZC Flags

  // Run Test
  cld // Clear Decimal Flag

  // Store Processor Status Flag Data
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Processor Status Flag Data
  PrintPSR(PSRFlagData, $FA20) // Print Processor Status Flag Data

  // Check Processor Status Flag Data
  lda.b PSRFlagData // A = Processor Status Flag Data
  cmp.w PSRResultCheckB
  beq Pass2
  Fail2:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail2
  Pass2:
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Type/Opcode Text
  PrintText(CLIPSR, $FA42, 9) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$CF // Set NVDIZC Flags

  // Run Test
  cli // Clear Interrupt Flag

  // Store Processor Status Flag Data
  php // Push Processor Status Register To Stack
  cld // Clear Decimal Flag
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Processor Status Flag Data
  PrintPSR(PSRFlagData, $FA60) // Print Processor Status Flag Data

  // Check Processor Status Flag Data
  lda.b PSRFlagData // A = Processor Status Flag Data
  cmp.w PSRResultCheckC
  beq Pass3
  Fail3:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail3
  Pass3:
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Type/Opcode Text
  PrintText(CLVPSR, $FA82, 9) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$CF // Set NVDIZC Flags

  // Run Test
  clv // Clear Overflow Flag

  // Store Processor Status Flag Data
  php // Push Processor Status Register To Stack
  cld // Clear Decimal Flag
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Processor Status Flag Data
  PrintPSR(PSRFlagData, $FAA0) // Print Processor Status Flag Data

  // Check Processor Status Flag Data
  lda.b PSRFlagData // A = Processor Status Flag Data
  cmp.w PSRResultCheckD
  beq Pass4
  Fail4:
    PrintText(Fail, $FAB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail4
  Pass4:
    PrintText(Pass, $FAB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Type/Opcode Text
  PrintText(REPPSR, $FAC2, 9) // Load Text To VRAM Lo Bytes

  // Setup Flags
  sep #$CF // Set NVDIZC Flags

  // Run Test
  rep #$CF // Reset NVDIZC Flags

  // Store Processor Status Flag Data
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Processor Status Flag Data
  PrintPSR(PSRFlagData, $FAE0) // Print Processor Status Flag Data

  // Check Processor Status Flag Data
  lda.b PSRFlagData // A = Processor Status Flag Data
  cmp.w PSRResultCheckE
  beq Pass5
  Fail5:
    PrintText(Fail, $FAF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail5
  Pass5:
    PrintText(Pass, $FAF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Type/Opcode Text
  PrintText(SECPSR, $FB02, 9) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$CF // Reset NVDIZC Flags

  // Run Test
  sec // Set Carry Flag

  // Store Processor Status Flag Data
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Processor Status Flag Data
  PrintPSR(PSRFlagData, $FB20) // Print Processor Status Flag Data

  // Check Processor Status Flag Data
  lda.b PSRFlagData // A = Processor Status Flag Data
  cmp.w PSRResultCheckF
  beq Pass6
  Fail6:
    PrintText(Fail, $FB32, 4) // Load Text To VRAM Lo Bytes
    bra Fail6
  Pass6:
    PrintText(Pass, $FB32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Type/Opcode Text
  PrintText(SEDPSR, $FB42, 9) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$CF // Reset NVDIZC Flags

  // Run Test
  sed // Set Decimal Flag

  // Store Processor Status Flag Data
  php // Push Processor Status Register To Stack
  cld // Clear Decimal Flag
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Processor Status Flag Data
  PrintPSR(PSRFlagData, $FB60) // Print Processor Status Flag Data

  // Check Processor Status Flag Data
  lda.b PSRFlagData // A = Processor Status Flag Data
  cmp.w PSRResultCheckG
  beq Pass7
  Fail7:
    PrintText(Fail, $FB72, 4) // Load Text To VRAM Lo Bytes
    bra Fail7
  Pass7:
    PrintText(Pass, $FB72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Type/Opcode Text
  PrintText(SEIPSR, $FB82, 9) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$CF // Reset NVDIZC Flags

  // Run Test
  sei // Set Interrupt Flag

  // Store Processor Status Flag Data
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Processor Status Flag Data
  PrintPSR(PSRFlagData, $FBA0) // Print Processor Status Flag Data

  // Check Processor Status Flag Data
  lda.b PSRFlagData // A = Processor Status Flag Data
  cmp.w PSRResultCheckH
  beq Pass8
  Fail8:
    PrintText(Fail, $FBB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail8
  Pass8:
    PrintText(Pass, $FBB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Type/Opcode Text
  PrintText(SEPPSR, $FBC2, 9) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$CF // Reset NVDIZC Flags

  // Run Test
  sep #$CF // Set NVDIZC Flags

  // Store Processor Status Flag Data
  php // Push Processor Status Register To Stack
  cld // Clear Decimal Flag
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Processor Status Flag Data
  PrintPSR(PSRFlagData, $FBE0) // Print Processor Status Flag Data

  // Check Processor Status Flag Data
  lda.b PSRFlagData // A = Processor Status Flag Data
  cmp.w PSRResultCheckI
  beq Pass9
  Fail9:
    PrintText(Fail, $FBF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail9
  Pass9:
    PrintText(Pass, $FBF2, 4) // Load Text To VRAM Lo Bytes

Loop:
  jmp Loop

Title:
  db "CPU Test PSR (PSR Flags):"

PageBreak:
  db "------------------------------"

Key:
  db "Type| Opcode | NVDIZC | Test |"
Fail:
  db "FAIL"
Pass:
  db "PASS"

CLCPSR:
  db "CLC   $18"
CLDPSR:
  db "CLD   $D8"
CLIPSR:
  db "CLI   $58"
CLVPSR:
  db "CLV   $B8"
REPPSR:
  db "REP   $C2"
SECPSR:
  db "SEC   $38"
SEDPSR:
  db "SED   $F8"
SEIPSR:
  db "SEI   $78"
SEPPSR:
  db "SEP   $E2"

PSRResultCheckA:
  db $EE
PSRResultCheckB:
  db $E7
PSRResultCheckC:
  db $EB
PSRResultCheckD:
  db $AF
PSRResultCheckE:
  db $20
PSRResultCheckF:
  db $21
PSRResultCheckG:
  db $28
PSRResultCheckH:
  db $24
PSRResultCheckI:
  db $EF

BGCHR:
  include "Font8x8.asm" // Include BG 1BPP 8x8 Tile Font Character Data (1016 Bytes)
BGPAL:
  dw $7800, $7FFF // Blue / White Palette (4 Bytes)
BGCLEAR:
  dw $0020 // BG Clear Character Space " " Fixed Word