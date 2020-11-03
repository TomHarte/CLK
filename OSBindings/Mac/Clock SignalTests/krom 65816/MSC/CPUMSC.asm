// SNES 65816 CPU Test MSC (Miscellaneous) demo by krom (Peter Lemon):
arch snes.cpu
output "CPUMSC.sfc", create

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
  lda.b PSRFlagData // A = Processor Status Flag Data
  cmp.b #$24 // Compare A To $24
  bne Init // IF (A != $24) Init
  jmp STPCheck // IF (A = $24) STP Check

Init:
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
  PrintText(Title, $F882, 29) // Load Text To VRAM Lo Bytes

  // Print Page Break Text
  PrintText(PageBreak, $F8C2, 30) // Load Text To VRAM Lo Bytes

  // Print Reset Info Text
  PrintText(ResetInfo, $F902, 30) // Load Text To VRAM Lo Bytes

  // Print Key Text
  PrintText(Key, $F942, 30) // Load Text To VRAM Lo Bytes

  // Print Page Break Text
  PrintText(PageBreak, $F982, 30) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Type/Opcode Text
  PrintText(NOPMSC, $F9C2, 11) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$02 // Reset Zero Flag
  clc // Clear Carry Flag
  clv // Clear Overflow Flag

  // Store Processor Status Flag Data
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Processor Status Flag Data
  PrintPSR(PSRFlagData, $F9E4) // Print Processor Status Flag Data

  // Run Test
  nop // No Operation
  lda.b PSRFlagData // A = Processor Status Flag Data
  cmp.w PSRResultCheck
  beq Pass1
  PrintText(Fail, $F9F2, 4) // Load Text To VRAM Lo Bytes
  Fail1:
    bra Fail1
  Pass1:
    PrintText(Pass, $F9F2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Type/Opcode Text
  PrintText(WDMMSC, $FA02, 11) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$02 // Reset Zero Flag
  clc // Clear Carry Flag
  clv // Clear Overflow Flag

  // Store Processor Status Flag Data
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Processor Status Flag Data
  PrintPSR(PSRFlagData, $FA24) // Print Processor Status Flag Data

  // Run Test
  wdm #$00 // No Operation
  lda.b PSRFlagData // A = Processor Status Flag Data
  cmp.w PSRResultCheck
  beq Pass2
  PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
  Fail2:
    bra Fail2
  Pass2:
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Type/Opcode Text
  PrintText(BRKMSC, $FA42, 11) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$02 // Reset Zero Flag
  clc // Clear Carry Flag
  clv // Clear Overflow Flag

  // Store Processor Status Flag Data
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Processor Status Flag Data
  PrintPSR(PSRFlagData, $FA64) // Print Processor Status Flag Data

  // Run Test
  brk #$00 // Software Interrupt
  lda.b PSRFlagData // A = Processor Status Flag Data
  cmp.w PSRResultCheck
  beq Pass3
  PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
  Fail3:
    bra Fail3
  Pass3:
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Type/Opcode Text
  PrintText(COPMSC, $FA82, 11) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$02 // Reset Zero Flag
  clc // Clear Carry Flag
  clv // Clear Overflow Flag

  // Store Processor Status Flag Data
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Processor Status Flag Data
  PrintPSR(PSRFlagData, $FAA4) // Print Processor Status Flag Data

  // Run Test
  cop #$00 // Software Interrupt
  lda.b PSRFlagData // A = Processor Status Flag Data
  cmp.w PSRResultCheck
  beq Pass4
  PrintText(Fail, $FAB2, 4) // Load Text To VRAM Lo Bytes
  Fail4:
    bra Fail4
  Pass4:
    PrintText(Pass, $FAB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Type/Opcode Text
  PrintText(WAIMSC, $FAC2, 11) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$02 // Reset Zero Flag
  clc // Clear Carry Flag
  clv // Clear Overflow Flag

  // Store Processor Status Flag Data
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Processor Status Flag Data
  PrintPSR(PSRFlagData, $FAE4) // Print Processor Status Flag Data

  // Run Test
  lda.b #$80 // Set VBlank Interrupt
  sta.w REG_NMITIMEN // Store Interrupt Enable
  wai // Wait for Interrupt
  stz.w REG_NMITIMEN // Reset VBlank Interrupt
  lda.b PSRFlagData // A = Processor Status Flag Data
  cmp.w PSRResultCheck
  beq Pass5
  PrintText(Fail, $FAF2, 4) // Load Text To VRAM Lo Bytes
  Fail5:
    bra Fail5
  Pass5:
    PrintText(Pass, $FAF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Type/Opcode Text
  PrintText(STPMSC, $FB02, 11) // Load Text To VRAM Lo Bytes

  // Setup Flags
  rep #$02 // Reset Zero Flag
  clc // Clear Carry Flag
  clv // Clear Overflow Flag

  // Store Processor Status Flag Data
  php // Push Processor Status Register To Stack
  pla // Pull Accumulator Register From Stack
  sta.b PSRFlagData // Store Processor Status Flag Data To Memory

  // Print Processor Status Flag Data
  PrintPSR(PSRFlagData, $FB24) // Print Processor Status Flag Data

  // Run Test
  stp // Stop the Processor (Reset)

  STPCheck:
    sei // Disable Interrupts
    clc // Clear Carry To Switch To Native Mode
    xce // Xchange Carry & Emulation Bit (Native Mode)
    sep #$20 // Set 8-Bit A
    rep #$10 // Set 16-Bit X/Y

    // Setup Video
    lda.b #$F // Turn On Screen, Maximum Brightness
    sta.w REG_INIDISP // $2100: Screen Display

    WaitNMI() // Wait For VSync

  lda.b PSRFlagData // A = Processor Status Flag Data
  cmp.w PSRResultCheck
  beq Pass6
  PrintText(Fail, $FB32, 4) // Load Text To VRAM Lo Bytes
  Fail6:
    bra Fail6
  Pass6:
    PrintText(Pass, $FB32, 4) // Load Text To VRAM Lo Bytes

Loop:
  jmp Loop

RTIBreak:
  rti // Return From Interrupt

Title:
  db "CPU Test MSC (Miscellaneous):"

PageBreak:
  db "------------------------------"

ResetInfo:
  db "** Please Reset To PASS STP **"

Key:
  db "Type  | Opcode | NVZC | Test |"
Fail:
  db "FAIL"
Pass:
  db "PASS"

NOPMSC:
  db "NOP     $EA"
WDMMSC:
  db "WDM     $42"
BRKMSC:
  db "BRK     $00"
COPMSC:
  db "COP     $02"
WAIMSC:
  db "WAI     $CB"
STPMSC:
  db "STP     $DB"

PSRResultCheck:
  db $24

BGCHR:
  include "Font8x8.asm" // Include BG 1BPP 8x8 Tile Font Character Data (1016 Bytes)
BGPAL:
  dw $7800, $7FFF // Blue / White Palette (4 Bytes)
BGCLEAR:
  dw $0020 // BG Clear Character Space " " Fixed Word