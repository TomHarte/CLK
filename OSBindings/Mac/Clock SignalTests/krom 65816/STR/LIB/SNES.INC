//============== (Key: R=Read, W=Write, D=Double Read/Write) 
// SNES Include
//==============
// Memory Map
constant WRAM($0000)         // WRAM Mirror ($7E0000-$7E1FFF)                        8KB/RW

// PPU Picture Processing Unit Ports (Write-Only)
constant REG_INIDISP($2100)  // Display Control 1                                     1B/W
constant REG_OBSEL($2101)    // Object Size & Object Base                             1B/W
constant REG_OAMADDL($2102)  // OAM Address (Lower 8-Bit)                             2B/W
constant REG_OAMADDH($2103)  // OAM Address (Upper 1-Bit) & Priority Rotation         1B/W
constant REG_OAMDATA($2104)  // OAM Data Write                                        1B/W D
constant REG_BGMODE($2105)   // BG Mode & BG Character Size                           1B/W
constant REG_MOSAIC($2106)   // Mosaic Size & Mosaic Enable                           1B/W
constant REG_BG1SC($2107)    // BG1 Screen Base & Screen Size                         1B/W
constant REG_BG2SC($2108)    // BG2 Screen Base & Screen Size                         1B/W
constant REG_BG3SC($2109)    // BG3 Screen Base & Screen Size                         1B/W
constant REG_BG4SC($210A)    // BG4 Screen Base & Screen Size                         1B/W
constant REG_BG12NBA($210B)  // BG1/BG2 Character Data Area Designation               1B/W
constant REG_BG34NBA($210C)  // BG3/BG4 Character Data Area Designation               1B/W
constant REG_BG1HOFS($210D)  // BG1 Horizontal Scroll (X) / M7HOFS                    1B/W D
constant REG_BG1VOFS($210E)  // BG1 Vertical   Scroll (Y) / M7VOFS                    1B/W D
constant REG_BG2HOFS($210F)  // BG2 Horizontal Scroll (X)                             1B/W D
constant REG_BG2VOFS($2110)  // BG2 Vertical   Scroll (Y)                             1B/W D
constant REG_BG3HOFS($2111)  // BG3 Horizontal Scroll (X)                             1B/W D
constant REG_BG3VOFS($2112)  // BG3 Vertical   Scroll (Y)                             1B/W D
constant REG_BG4HOFS($2113)  // BG4 Horizontal Scroll (X)                             1B/W D
constant REG_BG4VOFS($2114)  // BG4 Vertical   Scroll (Y)                             1B/W D
constant REG_VMAIN($2115)    // VRAM Address Increment Mode                           1B/W
constant REG_VMADDL($2116)   // VRAM Address    (Lower 8-Bit)                         2B/W
constant REG_VMADDH($2117)   // VRAM Address    (Upper 8-Bit)                         1B/W
constant REG_VMDATAL($2118)  // VRAM Data Write (Lower 8-Bit)                         2B/W
constant REG_VMDATAH($2119)  // VRAM Data Write (Upper 8-Bit)                         1B/W
constant REG_M7SEL($211A)    // Mode7 Rot/Scale Mode Settings                         1B/W
constant REG_M7A($211B)      // Mode7 Rot/Scale A (COSINE A) & Maths 16-Bit Operand   1B/W D
constant REG_M7B($211C)      // Mode7 Rot/Scale B (SINE A)   & Maths  8-bit Operand   1B/W D
constant REG_M7C($211D)      // Mode7 Rot/Scale C (SINE B)                            1B/W D
constant REG_M7D($211E)      // Mode7 Rot/Scale D (COSINE B)                          1B/W D
constant REG_M7X($211F)      // Mode7 Rot/Scale Center Coordinate X                   1B/W D
constant REG_M7Y($2120)      // Mode7 Rot/Scale Center Coordinate Y                   1B/W D
constant REG_CGADD($2121)    // Palette CGRAM Address                                 1B/W
constant REG_CGDATA($2122)   // Palette CGRAM Data Write                              1B/W D
constant REG_W12SEL($2123)   // Window BG1/BG2  Mask Settings                         1B/W
constant REG_W34SEL($2124)   // Window BG3/BG4  Mask Settings                         1B/W
constant REG_WOBJSEL($2125)  // Window OBJ/MATH Mask Settings                         1B/W
constant REG_WH0($2126)      // Window 1 Left  Position (X1)                          1B/W
constant REG_WH1($2127)      // Window 1 Right Position (X2)                          1B/W
constant REG_WH2($2128)      // Window 2 Left  Position (X1)                          1B/W
constant REG_WH3($2129)      // Window 2 Right Position (X2)                          1B/W
constant REG_WBGLOG($212A)   // Window 1/2 Mask Logic (BG1..BG4)                      1B/W
constant REG_WOBJLOG($212B)  // Window 1/2 Mask Logic (OBJ/MATH)                      1B/W
constant REG_TM($212C)       // Main Screen Designation                               1B/W
constant REG_TS($212D)       // Sub  Screen Designation                               1B/W
constant REG_TMW($212E)      // Window Area Main Screen Disable                       1B/W
constant REG_TSW($212F)      // Window Area Sub  Screen Disable                       1B/W
constant REG_CGWSEL($2130)   // Color Math Control Register A                         1B/W
constant REG_CGADSUB($2131)  // Color Math Control Register B                         1B/W
constant REG_COLDATA($2132)  // Color Math Sub Screen Backdrop Color                  1B/W
constant REG_SETINI($2133)   // Display Control 2                                     1B/W

// PPU Picture Processing Unit Ports (Read-Only)
constant REG_MPYL($2134)     // PPU1 Signed Multiply Result (Lower  8-Bit)            1B/R
constant REG_MPYM($2135)     // PPU1 Signed Multiply Result (Middle 8-Bit)            1B/R
constant REG_MPYH($2136)     // PPU1 Signed Multiply Result (Upper  8-Bit)            1B/R
constant REG_SLHV($2137)     // PPU1 Latch H/V-Counter By Software (Read=Strobe)      1B/R
constant REG_RDOAM($2138)    // PPU1 OAM  Data Read                                   1B/R D
constant REG_RDVRAML($2139)  // PPU1 VRAM  Data Read (Lower 8-Bit)                    1B/R
constant REG_RDVRAMH($213A)  // PPU1 VRAM  Data Read (Upper 8-Bit)                    1B/R
constant REG_RDCGRAM($213B)  // PPU2 CGRAM Data Read (Palette)                        1B/R D
constant REG_OPHCT($213C)    // PPU2 Horizontal Counter Latch (Scanline X)            1B/R D
constant REG_OPVCT($213D)    // PPU2 Vertical   Counter Latch (Scanline Y)            1B/R D
constant REG_STAT77($213E)   // PPU1 Status & PPU1 Version Number                     1B/R
constant REG_STAT78($213F)   // PPU2 Status & PPU2 Version Number (Bit 7=0)           1B/R

// APU Audio Processing Unit Ports (Read/Write)
constant REG_APUIO0($2140)   // Main CPU To Sound CPU Communication Port 0            1B/RW
constant REG_APUIO1($2141)   // Main CPU To Sound CPU Communication Port 1            1B/RW
constant REG_APUIO2($2142)   // Main CPU To Sound CPU Communication Port 2            1B/RW
constant REG_APUIO3($2143)   // Main CPU To Sound CPU Communication Port 3            1B/RW
// $2140..$2143 - APU Ports Mirrored To $2144..$217F

// WRAM Access Ports
constant REG_WMDATA($2180)   // WRAM Data Read/Write                                  1B/RW
constant REG_WMADDL($2181)   // WRAM Address (Lower  8-Bit)                           1B/W
constant REG_WMADDM($2182)   // WRAM Address (Middle 8-Bit)                           1B/W
constant REG_WMADDH($2183)   // WRAM Address (Upper  1-Bit)                           1B/W
// $2184..$21FF - Unused Region (Open Bus)/Expansion (B-Bus)
// $2200..$3FFF - Unused Region (A-Bus)

// CPU On-Chip I/O Ports (These Have Long Waitstates: 1.78MHz Cycles Instead Of 3.5MHz)
// ($4000..$4015 - Unused Region (Open Bus)
constant REG_JOYWR($4016)    // Joypad Output                                         1B/W
constant REG_JOYA($4016)     // Joypad Input Register A (Joypad Auto Polling)         1B/R
constant REG_JOYB($4017)     // Joypad Input Register B (Joypad Auto Polling)         1B/R
// $4018..$41FF - Unused Region (Open Bus)

// CPU On-Chip I/O Ports (Write-only, Read=Open Bus)
constant REG_NMITIMEN($4200) // Interrupt Enable & Joypad Request                     1B/W
constant REG_WRIO($4201)     // Programmable I/O Port (Open-Collector Output)         1B/W
constant REG_WRMPYA($4202)   // Set Unsigned  8-Bit Multiplicand                      1B/W
constant REG_WRMPYB($4203)   // Set Unsigned  8-Bit Multiplier & Start Multiplication 1B/W
constant REG_WRDIVL($4204)   // Set Unsigned 16-Bit Dividend (Lower 8-Bit)            2B/W
constant REG_WRDIVH($4205)   // Set Unsigned 16-Bit Dividend (Upper 8-Bit)            1B/W
constant REG_WRDIVB($4206)   // Set Unsigned  8-Bit Divisor & Start Division          1B/W
constant REG_HTIMEL($4207)   // H-Count Timer Setting (Lower 8-Bit)                   2B/W
constant REG_HTIMEH($4208)   // H-Count Timer Setting (Upper 1bit)                    1B/W
constant REG_VTIMEL($4209)   // V-Count Timer Setting (Lower 8-Bit)                   2B/W
constant REG_VTIMEH($420A)   // V-Count Timer Setting (Upper 1-Bit)                   1B/W
constant REG_MDMAEN($420B)   // Select General Purpose DMA Channels & Start Transfer  1B/W
constant REG_HDMAEN($420C)   // Select H-Blank DMA (H-DMA) Channels                   1B/W
constant REG_MEMSEL($420D)   // Memory-2 Waitstate Control                            1B/W
// $420E..$420F - Unused Region (Open Bus)

// CPU On-Chip I/O Ports (Read-only)
constant REG_RDNMI($4210)    // V-Blank NMI Flag and CPU Version Number (Read/Ack)    1B/R
constant REG_TIMEUP($4211)   // H/V-Timer IRQ Flag (Read/Ack)                         1B/R
constant REG_HVBJOY($4212)   // H/V-Blank Flag & Joypad Busy Flag                     1B/R
constant REG_RDIO($4213)     // Joypad Programmable I/O Port (Input)                  1B/R
constant REG_RDDIVL($4214)   // Unsigned Div Result (Quotient) (Lower 8-Bit)          2B/R
constant REG_RDDIVH($4215)   // Unsigned Div Result (Quotient) (Upper 8-Bit)          1B/R
constant REG_RDMPYL($4216)   // Unsigned Div Remainder / Mul Product (Lower 8-Bit)    2B/R
constant REG_RDMPYH($4217)   // Unsigned Div Remainder / Mul Product (Upper 8-Bit)    1B/R
constant REG_JOY1L($4218)    // Joypad 1 (Gameport 1, Pin 4) (Lower 8-Bit)            2B/R
constant REG_JOY1H($4219)    // Joypad 1 (Gameport 1, Pin 4) (Upper 8-Bit)            1B/R
constant REG_JOY2L($421A)    // Joypad 2 (Gameport 2, Pin 4) (Lower 8-Bit)            2B/R
constant REG_JOY2H($421B)    // Joypad 2 (Gameport 2, Pin 4) (Upper 8-Bit)            1B/R
constant REG_JOY3L($421C)    // Joypad 3 (Gameport 1, Pin 5) (Lower 8-Bit)            2B/R
constant REG_JOY3H($421D)    // Joypad 3 (Gameport 1, Pin 5) (Upper 8-Bit)            1B/R
constant REG_JOY4L($421E)    // Joypad 4 (Gameport 2, Pin 5) (Lower 8-Bit)            2B/R
constant REG_JOY4H($421F)    // Joypad 4 (Gameport 2, Pin 5) (Upper 8-Bit)            1B/R
// $4220..$42FF - Unused Region (Open Bus)

// CPU DMA Ports (Read/Write) ($43XP X = Channel Number 0..7, P = Port)
constant REG_DMAP0($4300)    // DMA0 DMA/HDMA Parameters                              1B/RW
constant REG_BBAD0($4301)    // DMA0 DMA/HDMA I/O-Bus Address (PPU-Bus AKA B-Bus)     1B/RW
constant REG_A1T0L($4302)    // DMA0 DMA/HDMA Table Start Address (Lower 8-Bit)       2B/RW
constant REG_A1T0H($4303)    // DMA0 DMA/HDMA Table Start Address (Upper 8-Bit)       1B/RW
constant REG_A1B0($4304)     // DMA0 DMA/HDMA Table Start Address (Bank)              1B/RW
constant REG_DAS0L($4305)    // DMA0 DMA Count / Indirect HDMA Address (Lower 8-Bit)  2B/RW
constant REG_DAS0H($4306)    // DMA0 DMA Count / Indirect HDMA Address (Upper 8-Bit)  1B/RW
constant REG_DASB0($4307)    // DMA0 Indirect HDMA Address (Bank)                     1B/RW
constant REG_A2A0L($4308)    // DMA0 HDMA Table Current Address (Lower 8-Bit)         2B/RW
constant REG_A2A0H($4309)    // DMA0 HDMA Table Current Address (Upper 8-Bit)         1B/RW
constant REG_NTRL0($430A)    // DMA0 HDMA Line-Counter (From Current Table entry)     1B/RW
constant REG_UNUSED0($430B)  // DMA0 Unused Byte                                      1B/RW
// $430C..$430E - Unused Region (Open Bus)
constant REG_MIRR0($430F)    // DMA0 Mirror Of $430B                                  1B/RW

constant REG_DMAP1($4310)    // DMA1 DMA/HDMA Parameters                              1B/RW
constant REG_BBAD1($4311)    // DMA1 DMA/HDMA I/O-Bus Address (PPU-Bus AKA B-Bus)     1B/RW
constant REG_A1T1L($4312)    // DMA1 DMA/HDMA Table Start Address (Lower 8-Bit)       2B/RW
constant REG_A1T1H($4313)    // DMA1 DMA/HDMA Table Start Address (Upper 8-Bit)       1B/RW
constant REG_A1B1($4314)     // DMA1 DMA/HDMA Table Start Address (Bank)              1B/RW
constant REG_DAS1L($4315)    // DMA1 DMA Count / Indirect HDMA Address (Lower 8-Bit)  2B/RW
constant REG_DAS1H($4316)    // DMA1 DMA Count / Indirect HDMA Address (Upper 8-Bit)  1B/RW
constant REG_DASB1($4317)    // DMA1 Indirect HDMA Address (Bank)                     1B/RW
constant REG_A2A1L($4318)    // DMA1 HDMA Table Current Address (Lower 8-Bit)         2B/RW
constant REG_A2A1H($4319)    // DMA1 HDMA Table Current Address (Upper 8-Bit)         1B/RW
constant REG_NTRL1($431A)    // DMA1 HDMA Line-Counter (From Current Table entry)     1B/RW
constant REG_UNUSED1($431B)  // DMA1 Unused Byte                                      1B/RW
// $431C..$431E - Unused Region (Open Bus)
constant REG_MIRR1($431F)    // DMA1 Mirror Of $431B                                  1B/RW

constant REG_DMAP2($4320)    // DMA2 DMA/HDMA Parameters                              1B/RW
constant REG_BBAD2($4321)    // DMA2 DMA/HDMA I/O-Bus Address (PPU-Bus AKA B-Bus)     1B/RW
constant REG_A1T2L($4322)    // DMA2 DMA/HDMA Table Start Address (Lower 8-Bit)       2B/RW
constant REG_A1T2H($4323)    // DMA2 DMA/HDMA Table Start Address (Upper 8-Bit)       1B/RW
constant REG_A1B2($4324)     // DMA2 DMA/HDMA Table Start Address (Bank)              1B/RW
constant REG_DAS2L($4325)    // DMA2 DMA Count / Indirect HDMA Address (Lower 8-Bit)  2B/RW
constant REG_DAS2H($4326)    // DMA2 DMA Count / Indirect HDMA Address (Upper 8-Bit)  1B/RW
constant REG_DASB2($4327)    // DMA2 Indirect HDMA Address (Bank)                     1B/RW
constant REG_A2A2L($4328)    // DMA2 HDMA Table Current Address (Lower 8-Bit)         2B/RW
constant REG_A2A2H($4329)    // DMA2 HDMA Table Current Address (Upper 8-Bit)         1B/RW
constant REG_NTRL2($432A)    // DMA2 HDMA Line-Counter (From Current Table entry)     1B/RW
constant REG_UNUSED2($432B)  // DMA2 Unused Byte                                      1B/RW
// $432C..$432E - Unused Region (Open Bus)
constant REG_MIRR2($432F)    // DMA2 Mirror Of $432B                                  1B/RW

constant REG_DMAP3($4330)    // DMA3 DMA/HDMA Parameters                              1B/RW
constant REG_BBAD3($4331)    // DMA3 DMA/HDMA I/O-Bus Address (PPU-Bus AKA B-Bus)     1B/RW
constant REG_A1T3L($4332)    // DMA3 DMA/HDMA Table Start Address (Lower 8-Bit)       2B/RW
constant REG_A1T3H($4333)    // DMA3 DMA/HDMA Table Start Address (Upper 8-Bit)       1B/RW
constant REG_A1B3($4334)     // DMA3 DMA/HDMA Table Start Address (Bank)              1B/RW
constant REG_DAS3L($4335)    // DMA3 DMA Count / Indirect HDMA Address (Lower 8-Bit)  2B/RW
constant REG_DAS3H($4336)    // DMA3 DMA Count / Indirect HDMA Address (Upper 8-Bit)  1B/RW
constant REG_DASB3($4337)    // DMA3 Indirect HDMA Address (Bank)                     1B/RW
constant REG_A2A3L($4338)    // DMA3 HDMA Table Current Address (Lower 8-Bit)         2B/RW
constant REG_A2A3H($4339)    // DMA3 HDMA Table Current Address (Upper 8-Bit)         1B/RW
constant REG_NTRL3($433A)    // DMA3 HDMA Line-Counter (From Current Table entry)     1B/RW
constant REG_UNUSED3($433B)  // DMA3 Unused Byte                                      1B/RW
// $433C..$433E - Unused Region (Open Bus)
constant REG_MIRR3($433F)    // DMA3 Mirror Of $433B                                  1B/RW

constant REG_DMAP4($4340)    // DMA4 DMA/HDMA Parameters                              1B/RW
constant REG_BBAD4($4341)    // DMA4 DMA/HDMA I/O-Bus Address (PPU-Bus AKA B-Bus)     1B/RW
constant REG_A1T4L($4342)    // DMA4 DMA/HDMA Table Start Address (Lower 8-Bit)       2B/RW
constant REG_A1T4H($4343)    // DMA4 DMA/HDMA Table Start Address (Upper 8-Bit)       1B/RW
constant REG_A1B4($4344)     // DMA4 DMA/HDMA Table Start Address (Bank)              1B/RW
constant REG_DAS4L($4345)    // DMA4 DMA Count / Indirect HDMA Address (Lower 8-Bit)  2B/RW
constant REG_DAS4H($4346)    // DMA4 DMA Count / Indirect HDMA Address (Upper 8-Bit)  1B/RW
constant REG_DASB4($4347)    // DMA4 Indirect HDMA Address (Bank)                     1B/RW
constant REG_A2A4L($4348)    // DMA4 HDMA Table Current Address (Lower 8-Bit)         2B/RW
constant REG_A2A4H($4349)    // DMA4 HDMA Table Current Address (Upper 8-Bit)         1B/RW
constant REG_NTRL4($434A)    // DMA4 HDMA Line-Counter (From Current Table entry)     1B/RW
constant REG_UNUSED4($434B)  // DMA4 Unused Byte                                      1B/RW
// $434C..$434E - Unused Region (Open Bus)
constant REG_MIRR4($434F)    // DMA4 Mirror Of $434B                                  1B/RW

constant REG_DMAP5($4350)    // DMA5 DMA/HDMA Parameters                              1B/RW
constant REG_BBAD5($4351)    // DMA5 DMA/HDMA I/O-Bus Address (PPU-Bus AKA B-Bus)     1B/RW
constant REG_A1T5L($4352)    // DMA5 DMA/HDMA Table Start Address (Lower 8-Bit)       2B/RW
constant REG_A1T5H($4353)    // DMA5 DMA/HDMA Table Start Address (Upper 8-Bit)       1B/RW
constant REG_A1B5($4354)     // DMA5 DMA/HDMA Table Start Address (Bank)              1B/RW
constant REG_DAS5L($4355)    // DMA5 DMA Count / Indirect HDMA Address (Lower 8-Bit)  2B/RW
constant REG_DAS5H($4356)    // DMA5 DMA Count / Indirect HDMA Address (Upper 8-Bit)  1B/RW
constant REG_DASB5($4357)    // DMA5 Indirect HDMA Address (Bank)                     1B/RW
constant REG_A2A5L($4358)    // DMA5 HDMA Table Current Address (Lower 8-Bit)         2B/RW
constant REG_A2A5H($4359)    // DMA5 HDMA Table Current Address (Upper 8-Bit)         1B/RW
constant REG_NTRL5($435A)    // DMA5 HDMA Line-Counter (From Current Table entry)     1B/RW
constant REG_UNUSED5($435B)  // DMA5 Unused Byte                                      1B/RW
// $435C..$435E - Unused Region (Open Bus)
constant REG_MIRR5($435F)    // DMA5 Mirror Of $435B                                  1B/RW

constant REG_DMAP6($4360)    // DMA6 DMA/HDMA Parameters                              1B/RW
constant REG_BBAD6($4361)    // DMA6 DMA/HDMA I/O-Bus Address (PPU-Bus AKA B-Bus)     1B/RW
constant REG_A1T6L($4362)    // DMA6 DMA/HDMA Table Start Address (Lower 8-Bit)       2B/RW
constant REG_A1T6H($4363)    // DMA6 DMA/HDMA Table Start Address (Upper 8-Bit)       1B/RW
constant REG_A1B6($4364)     // DMA6 DMA/HDMA Table Start Address (Bank)              1B/RW
constant REG_DAS6L($4365)    // DMA6 DMA Count / Indirect HDMA Address (Lower 8-Bit)  2B/RW
constant REG_DAS6H($4366)    // DMA6 DMA Count / Indirect HDMA Address (Upper 8-Bit)  1B/RW
constant REG_DASB6($4367)    // DMA6 Indirect HDMA Address (Bank)                     1B/RW
constant REG_A2A6L($4368)    // DMA6 HDMA Table Current Address (Lower 8-Bit)         2B/RW
constant REG_A2A6H($4369)    // DMA6 HDMA Table Current Address (Upper 8-Bit)         1B/RW
constant REG_NTRL6($436A)    // DMA6 HDMA Line-Counter (From Current Table entry)     1B/RW
constant REG_UNUSED6($436B)  // DMA6 Unused Byte                                      1B/RW
// $436C..$436E - Unused Region (Open Bus)
constant REG_MIRR6($436F)    // DMA6 Mirror Of $436B                                  1B/RW

constant REG_DMAP7($4370)    // DMA7 DMA/HDMA Parameters                              1B/RW
constant REG_BBAD7($4371)    // DMA7 DMA/HDMA I/O-Bus Address (PPU-Bus AKA B-Bus)     1B/RW
constant REG_A1T7L($4372)    // DMA7 DMA/HDMA Table Start Address (Lower 8-Bit)       2B/RW
constant REG_A1T7H($4373)    // DMA7 DMA/HDMA Table Start Address (Upper 8-Bit)       1B/RW
constant REG_A1B7($4374)     // DMA7 DMA/HDMA Table Start Address (Bank)              1B/RW
constant REG_DAS7L($4375)    // DMA7 DMA Count / Indirect HDMA Address (Lower 8-Bit)  2B/RW
constant REG_DAS7H($4376)    // DMA7 DMA Count / Indirect HDMA Address (Upper 8-Bit)  1B/RW
constant REG_DASB7($4377)    // DMA7 Indirect HDMA Address (Bank)                     1B/RW
constant REG_A2A7L($4378)    // DMA7 HDMA Table Current Address (Lower 8-Bit)         2B/RW
constant REG_A2A7H($4379)    // DMA7 HDMA Table Current Address (Upper 8-Bit)         1B/RW
constant REG_NTRL7($437A)    // DMA7 HDMA Line-Counter (From Current Table entry)     1B/RW
constant REG_UNUSED7($437B)  // DMA7 Unused Byte                                      1B/RW
// $437C..$437E - Unused Region (Open Bus)
constant REG_MIRR7($437F)    // DMA7 Mirror Of $437B                                  1B/RW
// $4380..$5FFF - Unused Region (Open Bus)

// Further Memory
// $6000..$7FFF - Expansion (Battery Backed RAM, In HiROM Cartridges)
// $8000..$FFFF - Cartridge ROM

//================================================
// ReadD16 - Read Double 8-bit To Memory (16-Bit)
//================================================
//  SRC: Source Address
// DEST: Destination Address
macro ReadD16(SRC, DEST) {
  lda {SRC}      // Load Source Low Byte
  sta {DEST}     // Store Destination Low Byte
  lda {SRC}      // Load Source High Byte
  sta {DEST} + 1 // Store Destination High Byte
}

//====================================================
// ReadD16Index - Read Double 8-bit To Index (16-Bit)
//====================================================
// SRC: Source Address
// REG: Destination Index Register (x, y)
macro ReadD16Index(SRC, REG) {
  lda {SRC} // Load Source Low Byte
  xba       // Exchange B & A Accumulators
  lda {SRC} // Load Source High Byte
  xba       // Exchange B & A Accumulators
  ta{REG}   // Transfer 16-Bit A To 16-Bit REG
}

//================================================
// WriteD8 - Write Memory To Double 8-bit (8-Bit)
//================================================
//  SRC: Source Address
// DEST: Destination Address
macro WriteD8(SRC, DEST) {
  lda {SRC}  // Load Source Low Byte
  sta {DEST} // Store Destination Low Byte
  stz {DEST} // Store Zero To Destination High Byte
}

//==================================================
// WriteD16 - Write Memory To Double 8-bit (16-Bit)
//==================================================
//  SRC: Source Address
// DEST: Destination Address
macro WriteD16(SRC, DEST) {
  lda {SRC}     // Load Source Low Byte
  sta {DEST}    // Store Destination Low Byte
  lda {SRC} + 1 // Load Source High Byte
  sta {DEST}    // Store Destination High Byte
}

//====================================================
// WriteD8Index - Write Index To Double 8-bit (8-Bit)
//====================================================
//  REG: Source Index Register (x, y)
// DEST: Destination Address
macro WriteD8Index(REG, DEST) {
  t{REG}a    // Transfer 8-Bit REG To 8-Bit A
  sta {DEST} // Store Destination Low Byte
  stz {DEST} // Store Zero To Destination High Byte
}

//======================================================
// WriteD16Index - Write Index To Double 8-bit (16-Bit)
//======================================================
//  REG: Source Index Register (x, y)
// DEST: Destination Address
macro WriteD16Index(REG, DEST) {
  rep #$38   // Set 16-Bit Accumulator & Index
  t{REG}a    // Transfer 16-Bit REG To 16-Bit A
  sep #$20   // Set 8-Bit Accumulator
  sta {DEST} // Store Destination Low Byte
  xba        // Exchange B & A Accumulators
  sta {DEST} // Store Destination High Byte
}

//=====================
// SNES Initialisation
//=====================
// ROMSPEED: ROM Speed (SLOWROM, FASTROM)
constant SLOWROM(0) // Access Cycle Designation (Slow ROM)
constant FASTROM(1) // Access Cycle Designation (Fast ROM)
macro SNES_INIT(ROMSPEED) {
  sei // Disable Interrupts
  clc // Clear Carry To Switch To Native Mode
  xce // Xchange Carry & Emulation Bit (Native Mode)

  phk
  plb
  rep #$38

  ldx.w #$1FFF // Set Stack To $1FFF
  txs // Transfer Index Register X To Stack Pointer

  lda.w #$0000
  tcd

  sep #$20 // Set 8-Bit Accumulator

  lda.b #{ROMSPEED} // Romspeed: Slow ROM = 0, Fast ROM = 1
  sta.w REG_MEMSEL  // Access Cycle Designation (Slow ROM / Fast ROM)

  lda.b #$8F // Force VBlank (Screen Off, Maximum Brightness)
  sta.w REG_INIDISP // Display Control 1: Brightness & Screen Enable Register ($2100)

  stz.w REG_OBSEL   // Object Size & Object Base ($2101)
  stz.w REG_OAMADDL // OAM Address (Lower 8-Bit) ($2102)
  stz.w REG_OAMADDH // OAM Address (Upper 1-Bit) & Priority Rotation ($2103)
  stz.w REG_BGMODE  // BG Mode & BG Character Size: Set Graphics Mode 0 ($2105)
  stz.w REG_MOSAIC  // Mosaic Size & Mosaic Enable: No Planes, No Mosiac ($2106)
  stz.w REG_BG1SC   // BG1 Screen Base & Screen Size: BG1 Map VRAM Location = $0000 ($2107)
  stz.w REG_BG2SC   // BG2 Screen Base & Screen Size: BG2 Map VRAM Location = $0000 ($2108)
  stz.w REG_BG3SC   // BG3 Screen Base & Screen Size: BG3 Map VRAM Location = $0000 ($2109)
  stz.w REG_BG4SC   // BG4 Screen Base & Screen Size: BG4 Map VRAM Location = $0000 ($210A)
  stz.w REG_BG12NBA // BG1/BG2 Character Data Area Designation: BG1/BG2 Tile Data Location = $0000 ($210B)
  stz.w REG_BG34NBA // BG3/BG4 Character Data Area Designation: BG3/BG4 Tile Data Location = $0000 ($210C)
  stz.w REG_BG1HOFS // BG1 Horizontal Scroll (X) / M7HOFS: BG1 Horizontal Scroll 1st Write = 0 (Lower 8-Bit) ($210D)
  stz.w REG_BG1HOFS // BG1 Horizontal Scroll (X) / M7HOFS: BG1 Horizontal Scroll 2nd Write = 0 (Upper 3-Bit) ($210D)
  stz.w REG_BG1VOFS // BG1 Vertical   Scroll (Y) / M7VOFS: BG1 Vertical   Scroll 1st Write = 0 (Lower 8-Bit) ($210E)
  stz.w REG_BG1VOFS // BG1 Vertical   Scroll (Y) / M7VOFS: BG1 Vertical   Scroll 2nd Write = 0 (Upper 3-Bit) ($210E)
  stz.w REG_BG2HOFS // BG2 Horizontal Scroll (X): BG2 Horizontal Scroll 1st Write = 0 (Lower 8-Bit) ($210F)
  stz.w REG_BG2HOFS // BG2 Horizontal Scroll (X): BG2 Horizontal Scroll 2nd Write = 0 (Upper 3-Bit) ($210F)
  stz.w REG_BG2VOFS // BG2 Vertical   Scroll (Y): BG2 Vertical   Scroll 1st Write = 0 (Lower 8-Bit) ($2110)
  stz.w REG_BG2VOFS // BG2 Vertical   Scroll (Y): BG2 Vertical   Scroll 2nd Write = 0 (Upper 3-Bit) ($2110)
  stz.w REG_BG3HOFS // BG3 Horizontal Scroll (X): BG3 Horizontal Scroll 1st Write = 0 (Lower 8-Bit) ($2111)
  stz.w REG_BG3HOFS // BG3 Horizontal Scroll (X): BG3 Horizontal Scroll 2nd Write = 0 (Upper 3-Bit) ($2111)
  stz.w REG_BG3VOFS // BG3 Vertical   Scroll (Y): BG3 Vertical   Scroll 1st Write = 0 (Lower 8-Bit) ($2112)
  stz.w REG_BG3VOFS // BG3 Vertical   Scroll (Y): BG3 Vertical   Scroll 2nd Write = 0 (Upper 3-Bit) ($2112)
  stz.w REG_BG4HOFS // BG4 Horizontal Scroll (X): BG4 Horizontal Scroll 1st Write = 0 (Lower 8-Bit) ($2113)
  stz.w REG_BG4HOFS // BG4 Horizontal Scroll (X): BG4 Horizontal Scroll 2nd Write = 0 (Upper 3-Bit) ($2113)
  stz.w REG_BG4VOFS // BG4 Vertical   Scroll (Y): BG4 Vertical   Scroll 1st Write = 0 (Lower 8-Bit) ($2114)
  stz.w REG_BG4VOFS // BG4 Vertical   Scroll (Y): BG4 Vertical   Scroll 2nd Write = 0 (Upper 3-Bit) ($2114)

  lda.b #$01
  stz.w REG_M7A // Mode7 Rot/Scale A (COSINE A) & Maths 16-Bit Operand: 1st Write = 0 (Lower 8-Bit) ($211B)
  sta.w REG_M7A // Mode7 Rot/Scale A (COSINE A) & Maths 16-Bit Operand: 2nd Write = 1 (Upper 8-Bit) ($211B)
  stz.w REG_M7B // Mode7 Rot/Scale B (SINE A)   & Maths  8-bit Operand: 1st Write = 0 (Lower 8-Bit) ($211C)
  stz.w REG_M7B // Mode7 Rot/Scale B (SINE A)   & Maths  8-bit Operand: 2nd Write = 0 (Upper 8-Bit) ($211C)
  stz.w REG_M7C // Mode7 Rot/Scale C (SINE B): 1st Write = 0 (Lower 8-Bit) ($211D)
  stz.w REG_M7C // Mode7 Rot/Scale C (SINE B): 2nd Write = 0 (Upper 8-Bit) ($211D)
  stz.w REG_M7D // Mode7 Rot/Scale D (COSINE B): 1st Write = 0 (Lower 8-Bit) ($211E)
  sta.w REG_M7D // Mode7 Rot/Scale D (COSINE B): 2nd Write = 1 (Upper 8-Bit) ($211E)
  stz.w REG_M7X // Mode7 Rot/Scale Center Coordinate X: 1st Write = 0 (Lower 8-Bit) ($211F)
  stz.w REG_M7X // Mode7 Rot/Scale Center Coordinate X: 2nd Write = 0 (Upper 8-Bit) ($211F)
  stz.w REG_M7Y // Mode7 Rot/Scale Center Coordinate Y: 1st Write = 0 (Lower 8-Bit) ($2120)
  stz.w REG_M7Y // Mode7 Rot/Scale Center Coordinate Y: 2nd Write = 0 (Upper 8-Bit) ($2120)

  stz.w REG_W12SEL  // Window BG1/BG2  Mask Settings = 0 ($2123)
  stz.w REG_W34SEL  // Window BG3/BG4  Mask Settings = 0 ($2124)
  stz.w REG_WOBJSEL // Window OBJ/MATH Mask Settings = 0 ($2125)
  stz.w REG_WH0     // Window 1 Left  Position (X1) = 0 ($2126)
  stz.w REG_WH1     // Window 1 Right Position (X2) = 0 ($2127)
  stz.w REG_WH2     // Window 2 Left  Position (X1) = 0 ($2128)
  stz.w REG_WH3     // Window 2 Right Position (X2) = 0 ($2129)
  stz.w REG_WBGLOG  // Window 1/2 Mask Logic (BG1..BG4) = 0 ($212A)
  stz.w REG_WOBJLOG // Window 1/2 Mask Logic (OBJ/MATH) = 0 ($212B)
  stz.w REG_TM      // Main Screen Designation = 0 ($212C)
  stz.w REG_TS      // Sub  Screen Designation = 0 ($212D)
  stz.w REG_TMW     // Window Area Main Screen Disable = 0 ($212E)
  stz.w REG_TSW     // Window Area Sub  Screen Disable = 0 ($212F)

  lda.b #$30
  sta.w REG_CGWSEL  // Color Math Control Register A = $30 ($2130)
  stz.w REG_CGADSUB // Color Math Control Register B = 0 ($2131)

  lda.b #$E0
  sta.w REG_COLDATA // Color Math Sub Screen Backdrop Color = $E0 ($2132)
  stz.w REG_SETINI  // Display Control 2 = 0 ($2133)

  stz.w REG_JOYWR // Joypad Output = 0 ($4016)

  stz.w REG_NMITIMEN // Interrupt Enable & Joypad Request: Reset VBlank, Interrupt, Joypad ($4200)

  lda.b #$FF
  sta.w REG_WRIO // Programmable I/O Port (Open-Collector Output) = $FF ($4201)

  stz.w REG_WRMPYA // Set Unsigned  8-Bit Multiplicand = 0 ($4202)
  stz.w REG_WRMPYB // Set Unsigned  8-Bit Multiplier & Start Multiplication = 0 ($4203)
  stz.w REG_WRDIVL // Set Unsigned 16-Bit Dividend (Lower 8-Bit) = 0 ($4204)
  stz.w REG_WRDIVH // Set Unsigned 16-Bit Dividend (Upper 8-Bit) = 0 ($4205)
  stz.w REG_WRDIVB // Set Unsigned  8-Bit Divisor & Start Division = 0 ($4206)
  stz.w REG_HTIMEL // H-Count Timer Setting (Lower 8-Bit) = 0 ($4207)
  stz.w REG_HTIMEH // H-Count Timer Setting (Upper 1-Bit) = 0 ($4208)
  stz.w REG_VTIMEL // V-Count Timer Setting (Lower 8-Bit) = 0 ($4209)
  stz.w REG_VTIMEH // V-Count Timer Setting (Upper 1-Bit) = 0 ($420A)
  stz.w REG_MDMAEN // Select General Purpose DMA Channels & Start Transfer = 0 ($420B)
  stz.w REG_HDMAEN // Select H-Blank DMA (H-DMA) Channels = 0 ($420C)

  // Clear OAM
  ldx.w #$0080
  lda.b #$E0
  -
    sta.w REG_OAMDATA // OAM Data Write 1st Write = $E0 (Lower 8-Bit) ($2104)
    sta.w REG_OAMDATA // OAM Data Write 2nd Write = $E0 (Upper 8-Bit) ($2104)
    stz.w REG_OAMDATA // OAM Data Write 1st Write = 0 (Lower 8-Bit) ($2104)
    stz.w REG_OAMDATA // OAM Data Write 2nd Write = 0 (Upper 8-Bit) ($2104)
    dex
    bne -

  ldx.w #$0020
  -
    stz.w REG_OAMDATA // OAM Data Write 1st/2nd Write = 0 (Lower/Upper 8-Bit) ($2104)
    dex
    bne -

  // Clear WRAM
  ldy.w #$0000
  sty.w REG_WMADDL // WRAM Address (Lower  8-Bit): Transfer To $7E:0000 ($2181)
  stz.w REG_WMADDH // WRAM Address (Upper  1-Bit): Select 1st WRAM Bank = $7E ($2183)

  ldx.w #$8008    // Fixed Source Byte Write To REG_WMDATA: WRAM Data Read/Write ($2180)
  stx.w REG_DMAP0 // DMA0 DMA/HDMA Parameters ($4300)

  ldx.w #CONST_ZERO     // Load Lower 16-Bit Address Of Zero Data In Rom
  lda.b #CONST_ZERO>>16 // Load Upper  8-Bit Address Of Zero Data In ROM (Bank)
  stx.w REG_A1T0L // DMA0 DMA/HDMA Table Start Address (Lower 16-Bit) ($4302)
  sta.w REG_A1B0  // DMA0 DMA/HDMA Table Start Address (Upper  8-Bit) (Bank) ($4304)
  sty.w REG_DAS0L // DMA0 DMA Count / Indirect HDMA Address: Transfer 64KB ($4305)

  lda.b #$01
  sta.w REG_MDMAEN // Select General Purpose DMA Channels & Start Transfer ($420B)
  nop // Delay
  sta.w REG_MDMAEN // Select General Purpose DMA Channels & Start Transfer: $2181..$2183 & $4305 Wrap Appropriately ($420B)

  // VRAM
  lda.b #$80      // Increment VRAM Address On Writes To REG_VMDATAH: VRAM Data Write (Upper 8-Bit) ($2119)
  sta.w REG_VMAIN // VRAM Address Increment Mode ($2115)
  ldy.w #$0000
  sty.w REG_VMADDL // VRAM Address: Begin At VRAM Address $0000 ($2116)
  sty.w REG_DAS0L  // DMA0 DMA Count / Indirect HDMA Address: Transfer 64KB ($4305)

  ldx.w #$1809    // Fixed Source Alternate Byte Write To REG_VMDATAL/REG_VMDATAH: VRAM Data Write (Lower/Upper 8-Bit) ($2118/$2119)
  stx.w REG_DMAP0 // DMA0 DMA/HDMA Parameters ($4300)

  lda.b #$01
  sta.w REG_MDMAEN // Select General Purpose DMA Channels & Start Transfer ($420B)

  // CGRAM
  stz.w REG_CGADD // Palette CGRAM Address = 0 ($2121)
  ldx.w #$200 // 512 Byte
  stx.w REG_DAS0L // DMA0 DMA Count / Indirect HDMA Address ($4305)
  ldx.w #$2208 // Fixed Source Byte Write To REG_CGDATA: Palette CGRAM Data Write ($2122)
  stx.w REG_DMAP0 // DMA0 DMA/HDMA Parameters ($4300)
  sta.w REG_MDMAEN // Select General Purpose DMA Channels & Start Transfer ($420B)

  jml +

CONST_ZERO:
    dw $0000

  +
}