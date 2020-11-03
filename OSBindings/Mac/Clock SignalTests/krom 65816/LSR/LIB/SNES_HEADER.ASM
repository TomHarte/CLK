//=============
// SNES Header
//=============
seek($FFC0)
// PROGRAM TITLE (21 Byte ASCII String, Use Spaces For Unused Bytes)
db "65816 CPU TEST LSR   "
// "123456789012345678901"

// ROM MODE/SPEED (Bits 7-4 = Speed, Bits 3-0 = Map Mode)
db $20
//  ||___________________Map Mode:
//  |                    $X0 = LoROM/32K Banks           (Mode 20)
//  |                    $X1 = HiROM/64K Banks           (Mode 21)
//  |                    $X2 = LoROM/32K Banks + S-DD1   (Mode 22 Mappable)
// Speed:                $X3 = LoROM/32K Banks + SA-1    (Mode 23 Mappable)
// $2X = SlowROM (200ns) $X5 = HiROM/64K Banks           (Mode 25 ExHiROM)
// $3X = FastROM (120ns) $XA = HiROM/64K Banks + SPC7110 (Mode 2A Mappable)

// ROM TYPE (Bits 7-4 = Co-processor, Bits 3-0 = Type)
db $00
//  ||___________________Type:
//  |                    $00 = ROM
//  |                    $01 = ROM+RAM
// Co-processor:         $02 = ROM+RAM+Battery
// $0X = DSP             $X3 = ROM+Co-processor
// $1X = GSU             $X4 = ROM+Co-processor+RAM
// $2X = OBC1            $X5 = ROM+Co-processor+RAM+Battery
// $3X = SA-1            $X6 = ROM+Co-processor+Battery
// $4X = S-DD1           $X9 = ROM+Co-processor+RAM+Battery+RTC-4513
// $5X = S-RTC           $XA = ROM+Co-processor+RAM+Battery+Overclocked

// ROM SIZE (Values are rounded-up for carts with 10,12,20,24 Mbits)
db $01
// $01 = 1 Bank  =  32Kb (0.25 Mbit), $07 =   7 Banks =  224Kb (1.75 Mbit)
// $02 = 2 Banks =  64Kb (0.50 Mbit), $08 =   8 Banks =  256Kb (2.00 Mbit)
// $03 = 3 Banks =  96Kb (0.75 Mbit), $09 =  16 Banks =  512Kb (4.00 Mbit)
// $04 = 4 Banks = 128Kb (1.00 Mbit), $0A =  32 Banks = 1024Kb (8.00 Mbit)
// $05 = 5 Banks = 160Kb (1.25 Mbit), $0B =  64 Banks = 2048Kb (16.00 Mbit)
// $06 = 6 Banks = 192Kb (1.50 Mbit), $0C = 128 Banks = 4096Kb (32.00 Mbit)

// RAM SIZE
db $00
// $00 = None, $04 =  16Kb
// $01 =  2Kb, $05 =  32Kb
// $02 =  4Kb, $06 =  64Kb
// $03 =  8Kb, $07 = 128Kb

// COUNTRY/VIDEO REFRESH (NTSC/PAL-M = 60 Hz, PAL/SECAM = 50 Hz)
db $00
// $00 = (J)apan              (NTSC), $09 = (D)Germany, Austria, Switz  (PAL)
// $01 = (E)USA, Canada       (NTSC), $0A = (I)taly                     (PAL)
// $02 = Euro(P)e              (PAL), $0B = (C)hina, Hong Kong          (PAL)
// $03 = S(W)eden, Scandinavia (PAL), $0C = Indonesia                   (PAL)
// $04 = Finland               (PAL), $0D = South (K)orea              (NTSC)
// $05 = Denmark               (PAL), $0E = (A)Common                   (ANY)
// $06 = (F)rance            (SECAM), $0F = Ca(N)ada                   (NTSC)
// $07 = (H)olland             (PAL), $10 = (B)razil                  (PAL-M)
// $08 = (S)pain               (PAL), $11 = (U)Australia                (PAL)

// DEVELOPER ID CODE
db $00
// $00 = None
// $01 = Nintendo
// $33 = New (Uses Extended Header)

// ROM VERSION NUMBER
db $00
// $00 = 1.00, $01 = 1.01

// COMPLEMENT CHECK
db "CC"
// CHECKSUM
db "CS"

// NATIVE VECTOR (65C816 Mode)
dw $0000 // RESERVED
dw $0000 // RESERVED
dw $0000 // COP VECTOR   (COP Opcode)
dw $0000 // BRK VECTOR   (BRK Opcode)
dw $0000 // ABORT VECTOR (Unused)
dw $0000 // NMI VECTOR   (V-Blank Interrupt)
dw $0000 // RESET VECTOR (Unused)
dw $0000 // IRQ VECTOR   (H/V-Timer/External Interrupt)

// EMU VECTOR (6502 Mode)
dw $0000 // RESERVED
dw $0000 // RESERVED
dw $0000 // COP VECTOR   (COP Opcode)
dw $0000 // BRK VECTOR   (Unused)
dw $0000 // ABORT VECTOR (Unused)
dw $0000 // NMI VECTOR   (V-Blank Interrupt)
dw $8000 // RESET VECTOR (CPU is always in 6502 mode on RESET)
dw $0000 // IRQ/BRK VECTOR