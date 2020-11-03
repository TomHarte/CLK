//===============
// SNES Graphics
//===============

//=============================
// WaitNMI - Wait For NMI Flag
//=============================
macro WaitNMI() {
  -
    bit.w REG_RDNMI // $4210: Read NMI Flag Register
    bpl - // Wait For NMI Flag
}

//======================================
// WaitHV - Wait For H/V Timer IRQ Flag
//======================================
macro WaitHV() {
  -
    bit.w REG_TIMEUP // $4210: Read H/V-Timer IRQ Flag
    bpl - // Wait For H/V Timer IRQ Flag
}

//========================================
// WaitHVB - Wait For V-Blank Period Flag
//========================================
macro WaitHVB() {
  -
    bit.w REG_HVBJOY // $4212: Read H/V-Blank Flag & Joypad Busy Flag
    bpl - // Wait For V-Blank Period Flag
}

//================================================================
// FadeIN - Increment Screen Brightness From 0..15 (VSYNC Timing)
//================================================================
macro FadeIN() {
  ldx.w #$0000 // Set X To Mininmum Brightness Level
  -
    bit.w REG_RDNMI // $4210: Read NMI Flag Register
    bpl - // Wait For NMI Flag

  inx // Increments Brightness Level
  txa // Swap 16-Bit X To 8-Bit A
  sta.w REG_INIDISP // $2100: Turn Screen To Brightness Level
  cmp.b #$0F // Compare With Maximum Brightness Level (15)
  bne -      // IF (Screen != Maximum Brightness Level) Loop
}

//=================================================================
// FadeOUT - Decrement Screen Brightness From 15..0 (VSYNC Timing)
//=================================================================
macro FadeOUT() {
  ldx.w #$000F // Set X To Maximum Brightness Level
  -
    bit.w REG_RDNMI // $4210: Read NMI Flag Register
    bpl - // Wait For NMI Flag

  dex // Decrement Brightness Level
  txa // Swap 16-Bit X To 8-Bit A
  sta.w REG_INIDISP // $2100: Turn Screen To Brightness Level
  cmp.b #$00 // Compare With Minimum Brightness Level
  bne -      // IF (Screen != Minimum Brightness Level) Loop
}

//======================================
// LoadPAL - Load Palette Data To CGRAM
//======================================
//  SRC: 24-Bit Address Of Source Data
// DEST:  9-Bit CGRAM Destination Address (Color # To Start On)
// SIZE: Size Of Data (# Of Colors To Copy)
// CHAN: DMA Channel To Transfer Data (0..7)
macro LoadPAL(SRC, DEST, SIZE, CHAN) {
  lda.b #{DEST}   // Set CGRAM Destination
  sta.w REG_CGADD // $2121: CGRAM

  stz.w REG_DMAP{CHAN} // Set DMA Mode (Write Byte, Increment Source) ($43X0: DMA Control)
  lda.b #$22           // Set Destination Register ($2122: CGRAM Write)
  sta.w REG_BBAD{CHAN} // $43X1: DMA Destination
  ldx.w #{SRC}         // Set Source Offset
  stx.w REG_A1T{CHAN}L // $43X2: DMA Source
  lda.b #{SRC} >> 16   // Set Source Bank
  sta.w REG_A1B{CHAN}  // $43X4: Source Bank
  ldx.w #{SIZE}        // Set Size In Bytes To DMA Transfer (2 Bytes For Each Color)
  stx.w REG_DAS{CHAN}L // $43X5: DMA Transfer Size/HDMA

  lda.b #$01 << {CHAN} // Start DMA Transfer On Channel
  sta.w REG_MDMAEN     // $420B: DMA Enable
}

//==========================================
// UpdatePAL - Update Palette Data To CGRAM
//==========================================
//  SRC: 24-Bit Address Of Source Data
// DEST:  9-Bit CGRAM Destination Address (Color # To Start On)
// SIZE: Size Of Data (# Of Colors To Copy)
// CHAN: DMA Channel To Transfer Data (0..7)
macro UpdatePAL(SRC, DEST, SIZE, CHAN) {
  lda.b #{DEST}   // Set CGRAM Destination
  sta.w REG_CGADD // $2121: CGRAM

  stz.w REG_DMAP{CHAN} // Set DMA Mode (Write Byte, Increment Source) ($43X0: DMA Control)
  lda.b #$22           // Set Destination Register ($2122: CGRAM Write)
  sta.w REG_BBAD{CHAN} // $43X1: DMA Destination
  ldx.w #{SRC}         // Set Source Offset
  stx.w REG_A1T{CHAN}L // $43X2: DMA Source
  lda.b #{SRC} >> 16   // Set Source Bank
  sta.w REG_A1B{CHAN}  // $43X4: Source Bank
  ldx.w #{SIZE}        // Set Size In Bytes To DMA Transfer (2 Bytes For Each Color)
  stx.w REG_DAS{CHAN}L // $43X5: DMA Transfer Size/HDMA

  -
    bit.w REG_HVBJOY // $4212: Read H/V-Blank Flag & Joypad Busy Flag
    bpl - // Wait For V-Blank Period Flag

  lda.b #$01 << {CHAN} // Start DMA Transfer On Channel
  sta.w REG_MDMAEN     // $420B: DMA Enable
}

//========================================
// ClearLOVRAM - Clear VRAM Lo Fixed Byte
//========================================
//  SRC: 24-Bit Address Of Source Data
// DEST: 16-Bit VRAM Destination (WORD Address)
// SIZE: Size Of Data (BYTE Size)
// CHAN: DMA Channel To Transfer Data (0..7)
macro ClearLOVRAM(SRC, DEST, SIZE, CHAN) {
  stz.w REG_VMAIN    // Set Increment VRAM Address After Accessing Lo Byte ($2115: Video Port Control)
  ldx.w #{DEST} >> 1 // Set VRAM Destination
  stx.w REG_VMADDL   // $2116: VRAM

  lda.b #$08           // Set DMA Mode (Write Byte, Fixed Source)
  sta.w REG_DMAP{CHAN} // $43X0: DMA Control
  lda.b #$18           // Set Destination Register ($2118: VRAM Write)
  sta.w REG_BBAD{CHAN} // $43X1: DMA Destination
  ldx.w #{SRC}         // Set Source Offset (Lo Byte)
  stx.w REG_A1T{CHAN}L // $43X2: DMA Source
  lda.b #{SRC} >> 16   // Set Source Bank
  sta.w REG_A1B{CHAN}  // $43X4: Source Bank
  ldx.w #{SIZE}        // Set Size In Bytes To DMA Transfer
  stx.w REG_DAS{CHAN}L // $43X5: DMA Transfer Size/HDMA

  lda.b #$01 << {CHAN} // Start DMA Transfer On Channel
  sta.w REG_MDMAEN     // $420B: DMA Enable
}

//========================================
// ClearHIVRAM - Clear VRAM Hi Fixed Byte
//========================================
//  SRC: 24-Bit Address Of Source Data
// DEST: 16-Bit VRAM Destination (WORD Address)
// SIZE: Size Of Data (BYTE Size)
// CHAN: DMA Channel To Transfer Data (0..7)
macro ClearHIVRAM(SRC, DEST, SIZE, CHAN) {
  lda.b #$80         // Set Increment VRAM Address After Accessing Hi Byte
  sta.w REG_VMAIN    // $2115: Video Port Control
  ldx.w #{DEST} >> 1 // Set VRAM Destination
  stx.w REG_VMADDL   // $2116: VRAM

  lda.b #$08           // Set DMA Mode (Write Byte, Fixed Source)
  sta.w REG_DMAP{CHAN} // $43X0: DMA Control
  lda.b #$19           // Set Destination Register ($2119: VRAM Write)
  sta.w REG_BBAD{CHAN} // $43X1: DMA Destination
  ldx.w #{SRC}         // Set Source Offset (Lo Byte)
  stx.w REG_A1T{CHAN}L // $43X2: DMA Source
  lda.b #{SRC} >> 16   // Set Source Bank
  sta.w REG_A1B{CHAN}  // $43X4: Source Bank
  ldx.w #{SIZE}        // Set Size In Bytes To DMA Transfer
  stx.w REG_DAS{CHAN}L // $43X5: DMA Transfer Size/HDMA

  lda.b #$01 << {CHAN} // Start DMA Transfer On Channel
  sta.w REG_MDMAEN     // $420B: DMA Enable
}

//===================================
// ClearVRAM - Clear VRAM Fixed Word 
//===================================
//  SRC: 24-Bit Address Of Source Data
// DEST: 16-Bit VRAM Destination (WORD Address)
// SIZE: Size Of Data (BYTE Size)
// CHAN: DMA Channel To Transfer Data (0..7)
macro ClearVRAM(SRC, DEST, SIZE, CHAN) {
  // Transfer Lo Byte
  stz.w REG_VMAIN    // Set Increment VRAM Address After Accessing Lo Byte ($2115: Video Port Control)
  ldx.w #{DEST} >> 1 // Set VRAM Destination
  stx.w REG_VMADDL   // $2116: VRAM

  lda.b #$08           // Set DMA Mode (Write Byte, Fixed Source)
  sta.w REG_DMAP{CHAN} // $43X0: DMA Control
  lda.b #$18           // Set Destination Register ($2118: VRAM Write)
  sta.w REG_BBAD{CHAN} // $43X1: DMA Destination
  ldx.w #{SRC}         // Set Source Offset (Lo Byte)
  stx.w REG_A1T{CHAN}L // $43X2: DMA Source
  lda.b #{SRC} >> 16   // Set Source Bank
  sta.w REG_A1B{CHAN}  // $43X4: Source Bank
  ldx.w #{SIZE}        // Set Size In Bytes To DMA Transfer
  stx.w REG_DAS{CHAN}L // $43X5: DMA Transfer Size/HDMA

  lda.b #$01 << {CHAN} // Start DMA Transfer On Channel
  sta.w REG_MDMAEN     // $420B: DMA Enable

  // Transfer Hi Byte
  lda.b #$80         // Set Increment VRAM Address After Accessing Hi Byte
  sta.w REG_VMAIN    // $2115: Video Port Control
  ldx.w #{DEST} >> 1 // Set VRAM Destination
  stx.w REG_VMADDL   // $2116: VRAM

  lda.b #$19           // Set Destination Register ($2119: VRAM Write)
  sta.w REG_BBAD{CHAN} // $43X1: DMA Destination
  ldx.w #({SRC} + 1)   // Set Source Offset (Hi Byte)
  stx.w REG_A1T{CHAN}L // $43X2: DMA Source
  ldx.w #{SIZE}        // Set Size In Bytes To DMA Transfer
  stx.w REG_DAS{CHAN}L // $43X5: DMA Transfer Size/HDMA

  lda.b #$01 << {CHAN} // Start DMA Transfer On Channel
  sta.w REG_MDMAEN     // $420B: DMA Enable
}

//==================================
// LoadVRAM - Load GFX Data To VRAM
//==================================
//  SRC: 24-Bit Address Of Source Data
// DEST: 16-Bit VRAM Destination (WORD Address)
// SIZE: Size Of Data (BYTE Size)
// CHAN: DMA Channel To Transfer Data (0..7)
macro LoadVRAM(SRC, DEST, SIZE, CHAN) {
  lda.b #$80         // Set Increment VRAM Address After Accessing Hi Byte
  sta.w REG_VMAIN    // $2115: Video Port Control
  ldx.w #{DEST} >> 1 // Set VRAM Destination
  stx.w REG_VMADDL   // $2116: VRAM

  lda.b #$01           // Set DMA Mode (Write Word, Increment Source)
  sta.w REG_DMAP{CHAN} // $43X0: DMA Control
  lda.b #$18           // Set Destination Register ($2118: VRAM Write)
  sta.w REG_BBAD{CHAN} // $43X1: DMA Destination
  ldx.w #{SRC}         // Set Source Offset
  stx.w REG_A1T{CHAN}L // $43X2: DMA Source
  lda.b #{SRC} >> 16   // Set Source Bank
  sta.w REG_A1B{CHAN}  // $43X4: Source Bank
  ldx.w #{SIZE}        // Set Size In Bytes To DMA Transfer
  stx.w REG_DAS{CHAN}L // $43X5: DMA Transfer Size/HDMA

  lda.b #$01 << {CHAN} // Start DMA Transfer On Channel
  sta.w REG_MDMAEN     // $420B: DMA Enable
}

//========================================================
// LoadVRAMStride - Load GFX Data To VRAM With DMA Stride
//========================================================
//    SRC: 24-Bit Address Of Source Data
//   DEST: 16-Bit VRAM Destination (WORD Address)
//   SIZE: Size Of Data (BYTE Size)
// STRIDE: Dest Offset Stride
//  COUNT: Number Of DMA Transfers
//   CHAN: DMA Channel To Transfer Data (0..7)
macro LoadVRAMStride(SRC, DEST, SIZE, STRIDE, COUNT, CHAN) {
  lda.b #$80         // Set Increment VRAM Address After Accessing Hi Byte
  sta.w REG_VMAIN    // $2115: Video Port Control

  lda.b #$01           // Set DMA Mode (Write Word, Increment Source)
  sta.w REG_DMAP{CHAN} // $43X0: DMA Control
  lda.b #$18           // Set Destination Register ($2118: VRAM Write)
  sta.w REG_BBAD{CHAN} // $43X1: DMA Destination
  ldx.w #{SRC}         // Set Source Offset
  stx.w REG_A1T{CHAN}L // $43X2: DMA Source
  lda.b #{SRC} >> 16   // Set Source Bank
  sta.w REG_A1B{CHAN}  // $43X4: Source Bank

  ldx.w #{DEST} >> 1 // Set VRAM Destination
  -
    stx.w REG_VMADDL // $2116: VRAM

    rep #$20 // Set 16-Bit Accumulator
    txa // A = X
    clc // Clear Carry Flag
    adc.w #{STRIDE} >> 1
    tax // X = A
    lda.w #{SIZE}        // Set Size In Bytes To DMA Transfer
    sta.w REG_DAS{CHAN}L // $43X5: DMA Transfer Size/HDMA

    sep #$20 // Set 8-Bit Accumulator
    lda.b #$01 << {CHAN} // Start DMA Transfer On Channel
    sta.w REG_MDMAEN     // $420B: DMA Enable

    cpx.w #({DEST} + ({STRIDE} * {COUNT})) >> 1
    bne -
}

//======================================
// UpdateVRAM - Update GFX Data To VRAM
//======================================
//  SRC: 24-Bit Address Of Source Data
// DEST: 16-Bit VRAM Destination (WORD Address)
// SIZE: Size Of Data (BYTE Size)
// CHAN: DMA Channel To Transfer Data (0..7)
macro UpdateVRAM(SRC, DEST, SIZE, CHAN) {
  lda.b #$80         // Set Increment VRAM Address After Accessing Hi Byte
  sta.w REG_VMAIN    // $2115: Video Port Control
  ldx.w #{DEST} >> 1 // Set VRAM Destination
  stx.w REG_VMADDL   // $2116: VRAM

  lda.b #$01           // Set DMA Mode (Write Word, Increment Source)
  sta.w REG_DMAP{CHAN} // $43X0: DMA Control
  lda.b #$18           // Set Destination Register ($2118: VRAM Write)
  sta.w REG_BBAD{CHAN} // $43X1: DMA Destination
  ldx.w #{SRC}         // Set Source Offset
  stx.w REG_A1T{CHAN}L // $43X2: DMA Source
  lda.b #{SRC} >> 16   // Set Source Bank
  sta.w REG_A1B{CHAN}  // $43X4: Source Bank
  ldx.w #{SIZE}        // Set Size In Bytes To DMA Transfer
  stx.w REG_DAS{CHAN}L // $43X5: DMA Transfer Size/HDMA

  -
    bit.w REG_HVBJOY // $4212: Read H/V-Blank Flag & Joypad Busy Flag
    bpl - // Wait For V-Blank Period Flag

  lda.b #$01 << {CHAN} // Start DMA Transfer On Channel
  sta.w REG_MDMAEN     // $420B: DMA Enable
}

//===========================================
// LoadM7VRAM - Load Mode 7 GFX Data To VRAM
//===========================================
//    SRCMAP: 24-Bit Address Of Source Map Data
//  SRCTILES: 24-Bit Address Of Source Tile Data
//      DEST: 16-Bit VRAM Destination (WORD Address)
//   SIZEMAP: Size Of Map Data (BYTE Size)
// SIZETILES: Size Of Tile Data (BYTE Size)
//      CHAN: DMA Channel To Transfer Data (0..7)
macro LoadM7VRAM(SRCMAP, SRCTILES, DEST, SIZEMAP, SIZETILES, CHAN) {
  // Load Mode7 Map Data To VRAM
  stz.w REG_VMAIN    // Set Increment VRAM Address After Accessing Lo Byte ($2115: Video Port Control)
  ldx.w #{DEST} >> 1 // Set VRAM Destination
  stx.w REG_VMADDL   // $2116: VRAM

  stz.w REG_DMAP{CHAN}  // Set DMA Mode (Write Byte, Increment Source) ($43X0: DMA Control)
  lda.b #$18            // Set Destination Register ($2118: VRAM Write)
  sta.w REG_BBAD{CHAN}  // $43X1: DMA Destination
  ldx.w #{SRCMAP}       // Set Source Offset (Map)
  stx.w REG_A1T{CHAN}L  // $43X2: DMA Source
  lda.b #{SRCMAP} >> 16 // Set Source Bank
  sta.w REG_A1B{CHAN}   // $43X4: Source Bank
  ldx.w #{SIZEMAP}      // Set Size In Bytes To DMA Transfer
  stx.w REG_DAS{CHAN}L  // $43X5: DMA Transfer Size/HDMA

  lda.b #$01 << {CHAN} // Start DMA Transfer On Channel
  sta.w REG_MDMAEN     // $420B: DMA Enable

  // Load Mode7 Tile Data To VRAM (Needs To Be On Same Bank As Map)
  lda.b #$80         // Set Increment VRAM Address After Accessing Hi Byte
  sta.w REG_VMAIN    // $2115: Video Port Control
  ldx.w #{DEST} >> 1 // Set VRAM Destination
  stx.w REG_VMADDL   // $2116: VRAM

  lda.b #$19           // Set Destination Register ($2119: VRAM Write)
  sta.w REG_BBAD{CHAN} // $43X1: DMA Destination
  ldx.w #{SRCTILES}    // Set Source Offset (Tiles)
  stx.w REG_A1T{CHAN}L // $43X2: DMA Source
  ldx.w #{SIZETILES}   // Set Size In Bytes To DMA Transfer
  stx.w REG_DAS{CHAN}L // $43X5: DMA Transfer Size/HDMA

  lda.b #$01 << {CHAN} // Start DMA Transfer On Channel
  sta.w REG_MDMAEN     // $420B: DMA Enable
}

//=============================================
// LoadLOVRAM - Load GFX Data To VRAM Lo Bytes
//=============================================
//  SRCTILES: 24-Bit Address Of Source Tile Data
//      DEST: 16-Bit VRAM Destination (WORD Address)
// SIZETILES: Size Of Tile Data (BYTE Size)
//      CHAN: DMA Channel To Transfer Data (0..7)
macro LoadLOVRAM(SRCTILES, DEST, SIZETILES, CHAN) {
  stz.w REG_VMAIN    // Set Increment VRAM Address After Accessing Lo Byte ($2115: Video Port Control)
  ldx.w #{DEST} >> 1 // Set VRAM Destination
  stx.w REG_VMADDL   // $2116: VRAM

  stz.w REG_DMAP{CHAN}    // Set DMA Mode (Write Byte, Increment Source) ($43X0: DMA Control)
  lda.b #$18              // Set Destination Register ($2118: VRAM Write)
  sta.w REG_BBAD{CHAN}    // $43X1: DMA Destination
  ldx.w #{SRCTILES}       // Set Source Offset
  stx.w REG_A1T{CHAN}L    // $43X2: DMA Source
  lda.b #{SRCTILES} >> 16 // Set Source Bank
  sta.w REG_A1B{CHAN}     // $43X4: Source Bank
  ldx.w #{SIZETILES}      // Set Size In Bytes To DMA Transfer
  stx.w REG_DAS{CHAN}L    // $43X5: DMA Transfer Size/HDMA

  lda.b #$01 << {CHAN} // Start DMA Transfer On Channel
  sta.w REG_MDMAEN     // $420B: DMA Enable
}

//=============================================
// LoadHIVRAM - Load GFX Data To VRAM Hi Bytes
//=============================================
//  SRCTILES: 24-Bit Address Of Source Tile Data
//      DEST: 16-Bit VRAM Destination (WORD Address)
// SIZETILES: Size Of Tile Data (BYTE Size)
//      CHAN: DMA Channel To Transfer Data (0..7)
macro LoadHIVRAM(SRCTILES, DEST, SIZETILES, CHAN) {
  lda.b #$80         // Set Increment VRAM Address After Accessing Hi Byte
  sta.w REG_VMAIN    // $2115: Video Port Control
  ldx.w #{DEST} >> 1 // Set VRAM Destination
  stx.w REG_VMADDL   // $2116: VRAM

  stz.w REG_DMAP{CHAN}    // Set DMA Mode (Write Byte, Increment Source) ($43X0: DMA Control)
  lda.b #$19              // Set Destination Register ($2119: VRAM Write)
  sta.w REG_BBAD{CHAN}    // $43X1: DMA Destination
  ldx.w #{SRCTILES}       // Set Source Offset
  stx.w REG_A1T{CHAN}L    // $43X2: DMA Source
  lda.b #{SRCTILES} >> 16 // Set Source Bank
  sta.w REG_A1B{CHAN}     // $43X4: Source Bank
  ldx.w #{SIZETILES}      // Set Size In Bytes To DMA Transfer
  stx.w REG_DAS{CHAN}L    // $43X5: DMA Transfer Size/HDMA

  lda.b #$01 << {CHAN} // Start DMA Transfer On Channel
  sta.w REG_MDMAEN     // $420B: DMA Enable
}

//===============================================
// BGScroll8 - Scroll GFX BG From Memory (8-Bit)
//===============================================
// BGSCR: Source BG Scroll Position
// BGPOS: Destination BG Pos Register
//   DIR: Direction To Scroll (de = Decrement, in = Increment)
macro BGScroll8(BGSCR, BGPOS, DIR) {
  {DIR}c {BGSCR} // Decrement Or Increment BG Scroll Position Low Byte
  lda {BGSCR}    // Load BG Scroll Position Low Byte
  sta {BGPOS}    // Store BG Scroll Position Low Byte
  stz {BGPOS}    // Store Zero To BG Scroll Position High Byte
}

//=================================================
// BGScroll16 - Scroll GFX BG From Memory (16-Bit)
//=================================================
// BGSCR: Source BG Scroll Position
// BGPOS: Destination BG Pos Register
//   DIR: Direction To Scroll (de = Decrement, in = Increment)
macro BGScroll16(BGSCR, BGPOS, DIR) {
  rep #$38        // Set 16-Bit Accumulator & Index
  {DIR}c {BGSCR}  // Decrement Or Increment BG Scroll Position Word
  sep #$20        // Set 8-Bit Accumulator
  lda {BGSCR}     // Load BG Scroll Position Low Byte
  sta {BGPOS}     // Store BG Scroll Position Low Byte
  lda {BGSCR} + 1 // Load BG Scroll Position High Byte
  sta {BGPOS}     // Store BG Scroll Position High Byte
}

//===============================================
// BGScroll8I - Scroll GFX BG From Index (8-Bit)
//===============================================
//   REG: Source Index Register (x, y)
// BGPOS: Destination BG Position Register
//   DIR: Direction To Scroll (de = Decrement, in = Increment)
macro BGScroll8I(REG, BGPOS, DIR) {
  {DIR}{REG}  // Decrement Or Increment BG Scroll Position Low Byte
  t{REG}a     // Swaps 8-Bit Index To 8-Bit A
  sta {BGPOS} // Store A To BG Scroll Position Low Byte
  stz {BGPOS} // Store Zero To BG Scroll Position High Byte
}

//=================================================
// BGScroll16I - Scroll GFX BG From Index (16-Bit)
//=================================================
//   REG: Source Index Register (x, y)
// BGPOS: Destination BG Position Register
//   DIR: Direction To Scroll (de = Decrement, in = Increment)
macro BGScroll16I(REG, BGPOS, DIR) {
  {DIR}{REG}  // Decrement Or Increment BG Scroll Position Word
  rep #$38    // Set 16-Bit Accumulator & Index
  t{REG}a     // Swaps 16-Bit Index To 16-Bit A
  sep #$20    // Set 8-Bit Accumulator
  sta {BGPOS} // Store A To BG Scroll Position Low Byte
  xba         // Exchange B & A Accumulators
  sta {BGPOS} // Store A To BG Scroll Position High Byte
}

//======================================
// Mode7CALC - Mode7 Matrix Calculation
//======================================
//      A: Mode7 COS A Word
//      B: Mode7 SIN A Word
//      C: Mode7 SIN B Word
//      D: Mode7 COS B Word
//  ANGLE: Mode7 Angle Byte
//     SX: Mode7 Scale X Word
//     SY: Mode7 Scale Y Word
// SINCOS: Mode7 SINCOS Table
macro Mode7CALC(A, B, C, D, ANGLE, SX, SY, SINCOS) {
  lda.b {ANGLE} // Load Angle To A
  tax           // Transfer A To X

  // Calculate B & C (SIN)
  // B
  lda.b {SX}       // Scale X
  sta.w REG_M7A    // $211B: MODE7 COSINE A (Multiplicand A)
  lda.b {SX} + 1
  sta.w REG_M7A    // $211B: MODE7 COSINE A (Multiplicand A)
  lda.w {SINCOS},x // SIN(X)
  sta.w REG_M7B    // $211C: MODE7 SINE A   (Multiplicand B)
  ldy.w REG_MPYM   // $2135: Multiplication Result -> 8.8
  sty.w {B}
  // C
  lda.b {SY}       // Scale Y
  sta.w REG_M7A    // $211B: MODE7 COSINE A (Multiplicand A)
  lda.b {SY} + 1   // High Byte
  sta.w REG_M7A    // $211B: MODE7 COSINE A (Multiplicand A)
  lda.w {SINCOS},x // SIN(X)
  eor.b #$FF       // Make Negative
  inc
  sta.w REG_M7B  // $211C: MODE7 SINE A   (Multiplicand B)
  ldy.w REG_MPYM // $2135: Multiplication Result -> 8.8
  sty.w {C}

  // Change X Index To Point To COS Values (X + 64)
  txa       // Transfer X Index To A
  clc       // Clear Carry Flag
  adc.b #64 // Add 64 With Carry
  tax       // Transfer A To X Index

  // Calculate A & D (COS)
  // A
  lda.b {SX}       // Scale X
  sta.w REG_M7A    // $211B: MODE7 COSINE A (Multiplicand A)
  lda.b {SX} + 1
  sta.w REG_M7A    // $211B: MODE7 COSINE A (Multiplicand A)
  lda.w {SINCOS},x // COS(X)
  sta.w REG_M7B    // $211C: MODE7 SINE A   (Multiplicand B)
  ldy.w REG_MPYM   // $2135: Multiplication Result -> 8.8
  sty.w {A}
  // D
  lda.b {SY}       // Scale Y
  sta.w REG_M7A    // $211B: MODE7 COSINE A (Multiplicand A)
  lda.b {SY} + 1
  sta.w REG_M7A    // $211B: MODE7 COSINE A (Multiplicand A)
  lda.w {SINCOS},x // COS(X)
  sta.w REG_M7B    // $211C: MODE7 SINE A   (Multiplicand B)
  ldy.w REG_MPYM   // $2135: Multiplication Result -> 8.8
  sty.w {D}

  // Store Result To Matrix
  lda.b {A}
  sta.w REG_M7A // $211B: MODE7 COSINE A (Multiplicand A)
  lda.b {A} + 1
  sta.w REG_M7A // $211B: MODE7 COSINE A (Multiplicand A)

  lda.b {B}
  sta.w REG_M7B // $211C: MODE7 SINE A   (Multiplicand B)
  lda.b {B} + 1
  sta.w REG_M7B // $211C: MODE7 SINE A   (Multiplicand B)

  lda.b {C}
  sta.w REG_M7C // $211D: MODE7 SINE B
  lda.b {C} + 1
  sta.w REG_M7C // $211D: MODE7 SINE B

  lda.b {D}
  sta.w REG_M7D // $211E: MODE7 COSINE B
  lda.b {D} + 1
  sta.w REG_M7D // $211E: MODE7 COSINE B
}