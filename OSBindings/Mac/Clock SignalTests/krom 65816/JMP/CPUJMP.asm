// SNES 65816 CPU Test JMP (Jump) demo by krom (Peter Lemon):
arch snes.cpu
output "CPUJMP.sfc", create

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

seek($8000); fill $8000 // Fill Upto $7FFF (Bank 0) With Zero Bytes
include "LIB/SNES.INC"        // Include SNES Definitions
include "LIB/SNES_HEADER.ASM" // Include Header & Vector Table
include "LIB/SNES_GFX.INC"    // Include Graphics Macros

// Variable Data
seek(WRAM) // 8Kb WRAM Mirror ($0000..$1FFF)
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
  PrintText(Title, $F882, 20) // Load Text To VRAM Lo Bytes

  // Print Page Break Text
  PrintText(PageBreak, $F8C2, 30) // Load Text To VRAM Lo Bytes

  // Print Key Text
  PrintText(Key, $F942, 30) // Load Text To VRAM Lo Bytes

  // Print Page Break Text
  PrintText(PageBreak, $F982, 30) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Type/Opcode Text
  PrintText(JMPAddr, $F9C2, 18) // Load Text To VRAM Lo Bytes

  // Run Test
  jmp Pass1 // PC = Address
  Fail1:
    PrintText(Fail, $F9F2, 4) // Load Text To VRAM Lo Bytes
    bra Fail1
  Pass1:
    PrintText(Pass, $F9F2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Type/Opcode Text
  PrintText(JMPIndirect, $FA02, 18) // Load Text To VRAM Lo Bytes

  // Run Test
  ldx.w #Pass2 // X = Pass2
  stx.b AbsoluteData // Store Absolute Data
  jmp (AbsoluteData) // PC = Address
  Fail2:
    PrintText(Fail, $FA32, 4) // Load Text To VRAM Lo Bytes
    bra Fail2
  Pass2:
    PrintText(Pass, $FA32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Type/Opcode Text
  PrintText(JMPIndirectX, $FA42, 18) // Load Text To VRAM Lo Bytes

  // Run Test
  ldx.w #Pass3 // X = Pass3
  stx.b AbsoluteData // Store Absolute Data
  ldx.w #$0000 // X = $0000
  jmp (AbsoluteData,x) // PC = Address
  Fail3:
    PrintText(Fail, $FA72, 4) // Load Text To VRAM Lo Bytes
    bra Fail3
  Pass3:
    PrintText(Pass, $FA72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Type/Opcode Text
  PrintText(JMPLong, $FA82, 18) // Load Text To VRAM Lo Bytes

  // Run Test
  jml Pass4 // PC = Address
  Fail4:
    PrintText(Fail, $FAB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail4
  Pass4:
    PrintText(Pass, $FAB2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Type/Opcode Text
  PrintText(JMPIndirectLong, $FAC2, 18) // Load Text To VRAM Lo Bytes

  // Run Test
  ldx.w #Pass5 // X = Pass5
  stx.b IndirectData // Store Indirect Data
  jmp [IndirectData] // PC = Address
  Fail5:
    PrintText(Fail, $FAF2, 4) // Load Text To VRAM Lo Bytes
    bra Fail5
  Pass5:
    PrintText(Pass, $FAF2, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Type/Opcode Text
  PrintText(JSRAddr, $FB02, 18) // Load Text To VRAM Lo Bytes

  // Run Test
  jsr JSRA // Stack = PC, PC = Address
  bra Pass6 // Branch After Return From Subroutine
  Fail6:
    PrintText(Fail, $FB32, 4) // Load Text To VRAM Lo Bytes
    bra Fail6
  Pass6:
    PrintText(Pass, $FB32, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Type/Opcode Text
  PrintText(JSRIndirectX, $FB42, 18) // Load Text To VRAM Lo Bytes

  // Run Test
  ldx.w #JSRB // X = JSRB
  stx.b AbsoluteData // Store Absolute Data
  ldx.w #$0000 // X = $0000
  jsr (AbsoluteData,x) // Stack = PC, PC = Address
  bra Pass7 // Branch After Return From Subroutine
  Fail7:
    PrintText(Fail, $FB72, 4) // Load Text To VRAM Lo Bytes
    bra Fail7
  Pass7:
    PrintText(Pass, $FB72, 4) // Load Text To VRAM Lo Bytes

  /////////////////////////////////////////////////////////////////
  // Print Type/Opcode Text
  PrintText(JSRLong, $FB82, 18) // Load Text To VRAM Lo Bytes

  // Run Test
  jsl JSRC // Stack = PC, PC = Address
  bra Pass8 // Branch After Return From Subroutine
  Fail8:
    PrintText(Fail, $FBB2, 4) // Load Text To VRAM Lo Bytes
    bra Fail8
  Pass8:
    PrintText(Pass, $FBB2, 4) // Load Text To VRAM Lo Bytes

Loop:
  jmp Loop

JSRA:
  rts // Return From Subroutine
JSRB:
  rts // Return From Subroutine
JSRC:
  rtl // Return From Subroutine

Title:
  db "CPU Test JMP (Jump):"

PageBreak:
  db "------------------------------"

Key:
  db "Syntax       | Opcode | Test |"
Fail:
  db "FAIL"
Pass:
  db "PASS"

JMPAddr:
  db "JMP addr       $4C"
JMPIndirect:
  db "JMP (addr)     $6C"
JMPIndirectX:
  db "JMP (addr,X)   $7C"
JMPLong:
  db "JMP/JML long   $5C"
JMPIndirectLong:
  db "JMP/JML [addr] $DC"
JSRAddr:
  db "JSR addr       $20"
JSRIndirectX:
  db "JSR (addr,X)   $FC"
JSRLong:
  db "JSR/JSL long   $22"

BGCHR:
  include "Font8x8.asm" // Include BG 1BPP 8x8 Tile Font Character Data (1016 Bytes)
BGPAL:
  dw $7800, $7FFF // Blue / White Palette (4 Bytes)
BGCLEAR:
  dw $0020 // BG Clear Character Space " " Fixed Word