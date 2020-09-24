//
//  65816Storage.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/09/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#include <cstdint>
#include <vector>

#include "65816Storage.hpp"

ProcessorStorage::ProcessorStorage() {
	// 1a. Absolute a [read and write].
	const auto absolute_read =
		install_ops({
			CycleFetchIncrementPC,			// OpCode.
			CycleFetchIncrementPC,			// AAL.
			CycleFetchIncrementPC,			// AAH.
			OperationConstructAbsolute,		// (copy AAL and AAH fetch to address)
			CycleFetchIncrementData,		// Data low.
			OperationSkipIf8,				// (don't do the next fetch if in emulation mode)
			CycleFetchIncrementData,		// Data high.
			OperationPerform,				// (whatever the operation is)
			OperationMoveToNextProgram
		});

	const auto absolute_write =
		install_ops({
			CycleFetchIncrementPC,			// OpCode.
			CycleFetchIncrementPC,			// AAL.
			CycleFetchIncrementPC,			// AAH.
			OperationConstructAbsolute,		// (copy AAL and AAH fetch to address)
			OperationPerform,				// (whatever the operation is)
			CycleStoreIncrementData,		// Data low.
			OperationSkipIf8,				// (don't do the next fetch if in emulation mode)
			CycleStoreIncrementData,		// Data high.
			OperationMoveToNextProgram
		});

	// Install the instructions.
#define op set_instruction
	/* 0x00 BRK s */
	/* 0x01 ORA (d, x) */
	/* 0x02 COP s */
	/* 0x03 ORA d, s */
	/* 0x04 TSB d */
	/* 0x05 ORA d */
	/* 0x06 ASL d */
	/* 0x07 ORA [d] */
	/* 0x08 PHP s */
	/* 0x09 ORA # */
	/* 0x0a ASL a */
	/* 0x0b PHD s */
	/* 0x0c TSB a */
	/* 0x0d ORA a */			op(0x0d, absolute_read, ORA);
	/* 0x0e ASL a */
	/* 0x0f ORA al */

	/* 0x10 BPL r */
	/* 0x11 ORA (d), y */
	/* 0x12 ORA (d) */
	/* 0x13 ORA (d, s), y */
	/* 0x14 TRB d */
	/* 0x15 ORA d,x */
	/* 0x16 ASL d, x */
	/* 0x17 ORA [d], y */
	/* 0x18 CLC i */
	/* 0x19 ORA a, y */
	/* 0x1a INC A */
	/* 0x1b TCS i */
	/* 0x1c TRB a */
	/* 0x1d ORA a, x */
	/* 0x1e ASL a, x */
	/* 0x1f ORA al, x */

	/* 0x20 JSR a */
	/* 0x21 ORA (d), y */
	/* 0x22 AND (d, x) */
	/* 0x23 JSL al */
	/* 0x24 BIT d */
	/* 0x25 AND d */
	/* 0x26 ROL d */
	/* 0x27 AND [d] */
	/* 0x28 PLP s */
	/* 0x29 AND # */
	/* 0x2a ROL A */
	/* 0x2b PLD s */
	/* 0x2c BIT a */			op(0x2c, absolute_read, BIT);
	/* 0x2d AND a */			op(0x2d, absolute_read, AND);
	/* 0x2e ROL a */
	/* 0x2f AND al */

	/* 0x30 BMI R */
	/* 0x31 AND (d), y */
	/* 0x32 AND (d) */
	/* 0x33 AND (d, s), y */
	/* 0x34 BIT d, x */
	/* 0x35 AND d, x */
	/* 0x36 TOL d, x */
	/* 0x37 AND [d], y */
	/* 0x38 SEC i */
	/* 0x39 AND a, y */
	/* 0x3a DEC A */
	/* 0x3b TSC i */
	/* 0x3c BIT a, x */
	/* 0x3d AND a, x */
	/* 0x3e TLD a, x */
	/* 0x3f AND al, x */

	/* 0x40 RTI s */
	/* 0x41	EOR (d, x) */
	/* 0x42	WDM i */
	/* 0x43	EOR d, s */
	/* 0x44	MVP xyc */
	/* 0x45	EOR d */
	/* 0x46	LSR d */
	/* 0x47	EOR [d] */
	/* 0x48	PHA s */
	/* 0x49	EOR # */
	/* 0x4a	LSR A */
	/* 0x4b	PHK s */
	/* 0x4c	JMP a */
	/* 0x4d	EOR a */			op(0x4d, absolute_read, EOR);
	/* 0x4e	LSR a */
	/* 0x4f	EOR Al */

	/* 0x50 BVC r */
	/* 0x51 EOR (d), y */
	/* 0x52 EOR (d) */
	/* 0x53 EOR (d, s), y */
	/* 0x54 MVN xyc */
	/* 0x55 EOR d, x */
	/* 0x56 LSR d, x */
	/* 0x57 EOR [d],y */
	/* 0x58 CLI i */
	/* 0x59 EOR a, y */
	/* 0x5a PHY s */
	/* 0x5b TCD i */
	/* 0x5c JMP al */
	/* 0x5d EOR a, x */
	/* 0x5e LSR a, x */
	/* 0x5f EOR al, x */

	/* 0x60 RTS s */
	/* 0x61 ADC (d, x) */
	/* 0x62 PER s */
	/* 0x63 ADC d, s */
	/* 0x64 STZ d */
	/* 0x65 ADC d */
	/* 0x66 ROR d */
	/* 0x67 ADC [d] */
	/* 0x68 PLA s */
	/* 0x69 ADC # */
	/* 0x6a ROR A */
	/* 0x6b RTL s */
	/* 0x6c JMP (a) */
	/* 0x6d ADC a */			op(0x6d, absolute_read, ADC);
	/* 0x6e ROR a */
	/* 0x6f ADC al */

	/* 0x70 BVS r */
	/* 0x71 ADC (d), y */
	/* 0x72 ADC (d) */
	/* 0x73 ADC (d, s), y */
	/* 0x74 STZ d, x */
	/* 0x75 ADC d, x */
	/* 0x76 ROR d, x */
	/* 0x77 ADC [d], y */
	/* 0x78 SEI i */
	/* 0x79 ADC a, y */
	/* 0x7a PLY s */
	/* 0x7b TDC i */
	/* 0x7c JMP (a, x) */
	/* 0x7d ADC a, x */
	/* 0x7e ROR a, x */
	/* 0x7f ADC al, x */

	/* 0x80 BRA r */
	/* 0x81 STA (d, x) */
	/* 0x82 BRL rl */
	/* 0x83 STA d, s */
	/* 0x84 STY d */
	/* 0x85 STA d */
	/* 0x86 STX d */
	/* 0x87 STA [d] */
	/* 0x88 DEY i */
	/* 0x89 BIT # */
	/* 0x8a TXA i */
	/* 0x8b PHB s */
	/* 0x8c STY a */			op(0x8c, absolute_write, STY);
	/* 0x8d STA a */			op(0x8d, absolute_write, STA);
	/* 0x8e STX a */			op(0x8e, absolute_write, STX);
	/* 0x8f STA al */

	/* 0x90 BCC r */
	/* 0x91 STA (d), y */
	/* 0x92 STA (d) */
	/* 0x93 STA (d, x), y */
	/* 0x94 STY d, x */
	/* 0x95 STA d, x */
	/* 0x96 STX d, y */
	/* 0x97 STA [d], y */
	/* 0x98 TYA i */
	/* 0x99 STA a, y */
	/* 0x9a TXS i */
	/* 0x9b TXY i */
	/* 0x9c STZ a */			op(0x9c, absolute_write, STZ);
	/* 0x9d STA a, x */
	/* 0x9e STZ a, x */
	/* 0x9f STA al, x */

	/* 0xa0 LDY # */
	/* 0xa1 LDA (d, x) */
	/* 0xa2 LDX # */
	/* 0xa3 LDA d, s */
	/* 0xa4 LDY d */
	/* 0xa5 LDA d */
	/* 0xa6 LDX d */
	/* 0xa7 LDA [d] */
	/* 0xa8 TAY i */
	/* 0xa9 LDA # */
	/* 0xaa TAX i */
	/* 0xab PLB s */
	/* 0xac LDY a */			op(0xac, absolute_read, LDY);
	/* 0xad LDA a */			op(0xad, absolute_read, LDA);
	/* 0xae LDX a */			op(0xae, absolute_read, LDX);
	/* 0xaf LDA al */

	/* 0xb0 BCS r */
	/* 0xb1 LDA (d), y */
	/* 0xb2 LDA (d) */
	/* 0xb3 LDA (d, s), y */
	/* 0xb4 LDY d, x */
	/* 0xb5 LDA d, x */
	/* 0xb6 LDX d, y */
	/* 0xb7 LDA [d], y */
	/* 0xb8 CLV i */
	/* 0xb9 LDA a, y */
	/* 0xba TSX i */
	/* 0xbb TYX i */
	/* 0xbc LDY a, x */
	/* 0xbd LDA a, x */
	/* 0xbe LDX a, y */
	/* 0xbf LDA al, x */

	/* 0xc0 CPY # */
	/* 0xc1 CMP (d, x) */
	/* 0xc2 REP # */
	/* 0xc3 CMP d, s */
	/* 0xc4 CPY d */
	/* 0xc5 CMP d */
	/* 0xc6 DEC d */
	/* 0xc7 CMP [d] */
	/* 0xc8 INY i */
	/* 0xc9 CMP # */
	/* 0xca DEX i */
	/* 0xcb WAI i */
	/* 0xcc CPY a */			op(0xcd, absolute_read, CPY);
	/* 0xcd CMP a */			op(0xcd, absolute_read, CMP);
	/* 0xce DEC a */
	/* 0xcf CMP al */

	/* 0xd0 BNE r */
	/* 0xd1 CMP (d), y */
	/* 0xd2 CMP (d) */
	/* 0xd3 CMP (d, s), y */
	/* 0xd4 PEI s */
	/* 0xd5 CMP d, x */
	/* 0xd6 DEC d, x */
	/* 0xd7 CMP [d], y */
	/* 0xd8 CLD i */
	/* 0xd9 CMP a, y */
	/* 0xda PHX s */
	/* 0xdb STP i */
	/* 0xdc JMP (a) */
	/* 0xdd CMP a, x */
	/* 0xde DEC a, x */
	/* 0xdf CMP al, x */

	/* 0xe0 CPX # */
	/* 0xe1 SBC (d, x) */
	/* 0xe2 SEP # */
	/* 0xe3 SBC d, s */
	/* 0xe4 CPX d */
	/* 0xe5 SBC d */
	/* 0xe6 INC d */
	/* 0xe7 SBC [d] */
	/* 0xe8 INX i */
	/* 0xe9 SBC # */
	/* 0xea NOP i */
	/* 0xeb XBA i */
	/* 0xec CPX a */			op(0xec, absolute_read, CPX);
	/* 0xed SBC a */			op(0xed, absolute_read, SBC);
	/* 0xee INC a */
	/* 0xef SBC al */

	/* 0xf0 BEQ r */
	/* 0xf1 SBC (d), y */
	/* 0xf2 SBC (d) */
	/* 0xf3 SBC (d, s), y */
	/* 0xf4 PEA s */
	/* 0xf5 SBC d, x */
	/* 0xf6 INC d, x */
	/* 0xf7 SBC [d], y */
	/* 0xf8 SED i */
	/* 0xf9 SBC a, y */
	/* 0xfa PLX s */
	/* 0xfb XCE i */
	/* 0xfc JSR (a, x) */
	/* 0xfd SBC a, x */
	/* 0xfe INC a, x */
	/* 0xff SBC al, x */

#undef op
}
