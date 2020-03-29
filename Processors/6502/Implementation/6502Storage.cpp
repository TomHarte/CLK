//
//  6502Storage.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/09/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "../6502.hpp"

#include <cstring>

using namespace CPU::MOS6502;

#define Program(...)						{__VA_ARGS__, OperationMoveToNextProgram}

#define Absolute							CycleLoadAddressAbsolute
#define AbsoluteXr							CycleLoadAddressAbsolute,					CycleAddXToAddressLow,					OperationCorrectAddressHigh
#define AbsoluteYr							CycleLoadAddressAbsolute,					CycleAddYToAddressLow,					OperationCorrectAddressHigh
#define AbsoluteX							CycleLoadAddressAbsolute,					CycleAddXToAddressLowRead,				OperationCorrectAddressHigh
#define AbsoluteY							CycleLoadAddressAbsolute,					CycleAddYToAddressLowRead,				OperationCorrectAddressHigh
#define Zero								OperationLoadAddressZeroPage
#define ZeroX								CycleLoadAddessZeroX
#define ZeroY								CycleLoadAddessZeroY
#define ZeroIndirect						OperationLoadAddressZeroPage,				CycleFetchAddressLowFromOperand,		CycleIncrementOperandFetchAddressHigh
#define IndexedIndirect						CycleIncrementPCFetchAddressLowFromOperand, CycleAddXToOperandFetchAddressLow,		CycleIncrementOperandFetchAddressHigh
#define IndirectIndexedr					CycleIncrementPCFetchAddressLowFromOperand, CycleIncrementOperandFetchAddressHigh,	CycleAddYToAddressLow,					OperationCorrectAddressHigh
#define IndirectIndexed						CycleIncrementPCFetchAddressLowFromOperand, CycleIncrementOperandFetchAddressHigh,	CycleAddYToAddressLowRead,				OperationCorrectAddressHigh

#define Read(...)							CycleFetchOperandFromAddress,	__VA_ARGS__
#define Write(...)							__VA_ARGS__,					CycleWriteOperandToAddress
#define ReadModifyWrite(...)				CycleFetchOperandFromAddress,	is_65c02(personality) ? CycleFetchOperandFromAddress : CycleWriteOperandToAddress,			__VA_ARGS__,							CycleWriteOperandToAddress

#define AbsoluteRead(op)					Program(Absolute,			Read(op))
#define AbsoluteXRead(op)					Program(AbsoluteXr,			Read(op))
#define AbsoluteYRead(op)					Program(AbsoluteYr,			Read(op))
#define ZeroRead(...)						Program(Zero,				Read(__VA_ARGS__))
#define ZeroXRead(op)						Program(ZeroX,				Read(op))
#define ZeroYRead(op)						Program(ZeroY,				Read(op))
#define ZeroIndirectRead(op)				Program(ZeroIndirect,		Read(op))
#define IndexedIndirectRead(op)				Program(IndexedIndirect,	Read(op))
#define IndirectIndexedRead(op)				Program(IndirectIndexedr,	Read(op))

#define AbsoluteWrite(op)					Program(Absolute,			Write(op))
#define AbsoluteXWrite(op)					Program(AbsoluteX,			Write(op))
#define AbsoluteYWrite(op)					Program(AbsoluteY,			Write(op))
#define ZeroWrite(op)						Program(Zero,				Write(op))
#define ZeroXWrite(op)						Program(ZeroX,				Write(op))
#define ZeroYWrite(op)						Program(ZeroY,				Write(op))
#define ZeroIndirectWrite(op)				Program(ZeroIndirect,		Write(op))
#define IndexedIndirectWrite(op)			Program(IndexedIndirect,	Write(op))
#define IndirectIndexedWrite(op)			Program(IndirectIndexed,	Write(op))

#define AbsoluteReadModifyWrite(...)		Program(Absolute,			ReadModifyWrite(__VA_ARGS__))
#define AbsoluteXReadModifyWrite(...)		Program(AbsoluteX,			ReadModifyWrite(__VA_ARGS__))
#define AbsoluteYReadModifyWrite(...)		Program(AbsoluteY,			ReadModifyWrite(__VA_ARGS__))
#define ZeroReadModifyWrite(...)			Program(Zero,				ReadModifyWrite(__VA_ARGS__))
#define ZeroXReadModifyWrite(...)			Program(ZeroX,				ReadModifyWrite(__VA_ARGS__))
#define ZeroYReadModifyWrite(...)			Program(ZeroY,				ReadModifyWrite(__VA_ARGS__))
#define IndexedIndirectReadModifyWrite(...)	Program(IndexedIndirect,	ReadModifyWrite(__VA_ARGS__))
#define IndirectIndexedReadModifyWrite(...)	Program(IndirectIndexed,	ReadModifyWrite(__VA_ARGS__))

#define FastAbsoluteXReadModifyWrite(...)		Program(AbsoluteXr,			ReadModifyWrite(__VA_ARGS__))
#define FastAbsoluteYReadModifyWrite(...)		Program(AbsoluteYr,			ReadModifyWrite(__VA_ARGS__))

#define Immediate(op)						Program(OperationIncrementPC,		op)
#define Implied(op)							Program(OperationSTA,				op,	OperationCopyOperandToA)

#define ZeroNop()							Program(Zero, CycleFetchOperandFromAddress)
#define ZeroXNop()							Program(ZeroX, CycleFetchOperandFromAddress)
#define AbsoluteNop()						Program(Absolute)
#define AbsoluteXNop()						Program(AbsoluteX)
#define ImpliedNop()						{OperationMoveToNextProgram}
#define ImmediateNop()						Program(OperationIncrementPC)

#define JAM									{CycleFetchOperand, OperationScheduleJam}

ProcessorStorage::ProcessorStorage(Personality personality) {
	// only the interrupt flag is defined upon reset but get_flags isn't going to
	// mask the other flags so we need to do that, at least
	carry_flag_ &= Flag::Carry;
	decimal_flag_ &= Flag::Decimal;
	overflow_flag_ &= Flag::Overflow;

	const InstructionList operations_6502[] = {
		/* 0x00 BRK */			Program(CycleIncPCPushPCH, CyclePushPCL, OperationBRKPickVector, OperationSetOperandFromFlagsWithBRKSet, CyclePushOperand, OperationSetIRQFlags, CycleReadVectorLow, CycleReadVectorHigh),
		/* 0x01 ORA x, ind */	IndexedIndirectRead(OperationORA),
		/* 0x02 JAM */			JAM,																	/* 0x03 ASO x, ind */	IndexedIndirectReadModifyWrite(OperationASO),
		/* 0x04 NOP zpg */		ZeroNop(),																/* 0x05 ORA zpg */		ZeroRead(OperationORA),
		/* 0x06 ASL zpg */		ZeroReadModifyWrite(OperationASL),										/* 0x07 ASO zpg */		ZeroReadModifyWrite(OperationASO),
		/* 0x08 PHP */			Program(OperationSetOperandFromFlagsWithBRKSet, CyclePushOperand),
		/* 0x09 ORA # */		Immediate(OperationORA),
		/* 0x0a ASL A */		Implied(OperationASL),													/* 0x0b ANC # */		Immediate(OperationANC),
		/* 0x0c NOP abs */		AbsoluteNop(),															/* 0x0d ORA abs */		AbsoluteRead(OperationORA),
		/* 0x0e ASL abs */		AbsoluteReadModifyWrite(OperationASL),									/* 0x0f ASO abs */		AbsoluteReadModifyWrite(OperationASO),
		/* 0x10 BPL */			Program(OperationBPL),													/* 0x11 ORA ind, y */	IndirectIndexedRead(OperationORA),
		/* 0x12 JAM */			JAM,																	/* 0x13 ASO ind, y */	IndirectIndexedReadModifyWrite(OperationASO),
		/* 0x14 NOP zpg, x */	ZeroXNop(),																/* 0x15 ORA zpg, x */	ZeroXRead(OperationORA),
		/* 0x16 ASL zpg, x */	ZeroXReadModifyWrite(OperationASL),										/* 0x17 ASO zpg, x */	ZeroXReadModifyWrite(OperationASO),
		/* 0x18 CLC */			Program(OperationCLC),													/* 0x19 ORA abs, y */	AbsoluteYRead(OperationORA),
		/* 0x1a NOP # */		ImpliedNop(),															/* 0x1b ASO abs, y */	AbsoluteYReadModifyWrite(OperationASO),
		/* 0x1c NOP abs, x */	AbsoluteXNop(),															/* 0x1d ORA abs, x */	AbsoluteXRead(OperationORA),
		/* 0x1e ASL abs, x */	AbsoluteXReadModifyWrite(OperationASL),									/* 0x1f ASO abs, x */	AbsoluteXReadModifyWrite(OperationASO),
		/* 0x20 JSR abs */		Program(CycleIncrementPCAndReadStack, CyclePushPCH, CyclePushPCL, CycleReadPCHLoadPCL),
		/* 0x21 AND x, ind */	IndexedIndirectRead(OperationAND),
		/* 0x22 JAM */			JAM,																	/* 0x23 RLA x, ind */	IndexedIndirectReadModifyWrite(OperationRLA),
		/* 0x24 BIT zpg */		ZeroRead(OperationBIT),													/* 0x25 AND zpg */		ZeroRead(OperationAND),
		/* 0x26 ROL zpg */		ZeroReadModifyWrite(OperationROL),										/* 0x27 RLA zpg */		ZeroReadModifyWrite(OperationRLA),
		/* 0x28 PLP */			Program(CycleReadFromS, CyclePullOperand, OperationSetFlagsFromOperand),
		/* 0x29 AND A # */		Immediate(OperationAND),
		/* 0x2a ROL A */		Implied(OperationROL),													/* 0x2b ANC # */		Immediate(OperationANC),
		/* 0x2c BIT abs */		AbsoluteRead(OperationBIT),												/* 0x2d AND abs */		AbsoluteRead(OperationAND),
		/* 0x2e ROL abs */		AbsoluteReadModifyWrite(OperationROL),									/* 0x2f RLA abs */		AbsoluteReadModifyWrite(OperationRLA),
		/* 0x30 BMI */			Program(OperationBMI),													/* 0x31 AND ind, y */	IndirectIndexedRead(OperationAND),
		/* 0x32 JAM */			JAM,																	/* 0x33 RLA ind, y */	IndirectIndexedReadModifyWrite(OperationRLA),
		/* 0x34 NOP zpg, x */	ZeroXNop(),																/* 0x35 AND zpg, x */	ZeroXRead(OperationAND),
		/* 0x36 ROL zpg, x */	ZeroXReadModifyWrite(OperationROL),										/* 0x37 RLA zpg, x */	ZeroXReadModifyWrite(OperationRLA),
		/* 0x38 SEC */			Program(OperationSEC),													/* 0x39 AND abs, y */	AbsoluteYRead(OperationAND),
		/* 0x3a NOP # */		ImpliedNop(),															/* 0x3b RLA abs, y */	AbsoluteYReadModifyWrite(OperationRLA),
		/* 0x3c NOP abs, x */	AbsoluteXNop(),															/* 0x3d AND abs, x */	AbsoluteXRead(OperationAND),
		/* 0x3e ROL abs, x */	AbsoluteXReadModifyWrite(OperationROL),									/* 0x3f RLA abs, x */	AbsoluteXReadModifyWrite(OperationRLA),
		/* 0x40 RTI */			Program(CycleReadFromS, CyclePullOperand, OperationSetFlagsFromOperand, CyclePullPCL, CyclePullPCH),
		/* 0x41 EOR x, ind */	IndexedIndirectRead(OperationEOR),
		/* 0x42 JAM */			JAM,																	/* 0x43 LSE x, ind */	IndexedIndirectReadModifyWrite(OperationLSE),
		/* 0x44 NOP zpg */		ZeroNop(),																/* 0x45 EOR zpg */		ZeroRead(OperationEOR),
		/* 0x46 LSR zpg */		ZeroReadModifyWrite(OperationLSR),										/* 0x47 LSE zpg */		ZeroReadModifyWrite(OperationLSE),
		/* 0x48 PHA */			Program(CyclePushA),													/* 0x49 EOR # */		Immediate(OperationEOR),
		/* 0x4a LSR A */		Implied(OperationLSR),													/* 0x4b ASR A */		Immediate(OperationASR),
		/* 0x4c JMP abs */		Program(CycleIncrementPCReadPCHLoadPCL),								/* 0x4d EOR abs */		AbsoluteRead(OperationEOR),
		/* 0x4e LSR abs */		AbsoluteReadModifyWrite(OperationLSR),									/* 0x4f LSE abs */		AbsoluteReadModifyWrite(OperationLSE),
		/* 0x50 BVC */			Program(OperationBVC),													/* 0x51 EOR ind, y */	IndirectIndexedRead(OperationEOR),
		/* 0x52 JAM */			JAM,																	/* 0x53 LSE ind, y */	IndirectIndexedReadModifyWrite(OperationLSE),
		/* 0x54 NOP zpg, x */	ZeroXNop(),																/* 0x55 EOR zpg, x */	ZeroXRead(OperationEOR),
		/* 0x56 LSR zpg, x */	ZeroXReadModifyWrite(OperationLSR),										/* 0x57 LSE zpg, x */	ZeroXReadModifyWrite(OperationLSE),
		/* 0x58 CLI */			Program(OperationCLI),													/* 0x59 EOR abs, y */	AbsoluteYRead(OperationEOR),
		/* 0x5a NOP # */		ImpliedNop(),															/* 0x5b LSE abs, y */	AbsoluteYReadModifyWrite(OperationLSE),
		/* 0x5c NOP abs, x */	AbsoluteXNop(),															/* 0x5d EOR abs, x */	AbsoluteXRead(OperationEOR),
		/* 0x5e LSR abs, x */	AbsoluteXReadModifyWrite(OperationLSR),									/* 0x5f LSE abs, x */	AbsoluteXReadModifyWrite(OperationLSE),
		/* 0x60 RTS */			Program(CycleReadFromS, CyclePullPCL, CyclePullPCH, CycleReadAndIncrementPC),
		/* 0x61 ADC x, ind */	IndexedIndirectRead(OperationADC),
		/* 0x62 JAM */			JAM,																	/* 0x63 RRA x, ind */	IndexedIndirectReadModifyWrite(OperationRRA, OperationADC),
		/* 0x64 NOP zpg */		ZeroNop(),																/* 0x65 ADC zpg */		ZeroRead(OperationADC),
		/* 0x66 ROR zpg */		ZeroReadModifyWrite(OperationROR),										/* 0x67 RRA zpg */		ZeroReadModifyWrite(OperationRRA, OperationADC),
		/* 0x68 PLA */			Program(CycleReadFromS, CyclePullA, OperationSetFlagsFromA),			/* 0x69 ADC # */		Immediate(OperationADC),
		/* 0x6a ROR A */		Implied(OperationROR),													/* 0x6b ARR # */		Immediate(OperationARR),
		/* 0x6c JMP (abs) */	Program(CycleReadAddressHLoadAddressL, CycleReadPCLFromAddress, CycleReadPCHFromAddressLowInc),
		/* 0x6d ADC abs */		AbsoluteRead(OperationADC),
		/* 0x6e ROR abs */		AbsoluteReadModifyWrite(OperationROR),									/* 0x6f RRA abs */		AbsoluteReadModifyWrite(OperationRRA, OperationADC),
		/* 0x70 BVS */			Program(OperationBVS),													/* 0x71 ADC ind, y */	IndirectIndexedRead(OperationADC),
		/* 0x72 JAM */			JAM,																	/* 0x73 RRA ind, y */	IndirectIndexedReadModifyWrite(OperationRRA, OperationADC),
		/* 0x74 NOP zpg, x */	ZeroXNop(),																/* 0x75 ADC zpg, x */	ZeroXRead(OperationADC),
		/* 0x76 ROR zpg, x */	ZeroXReadModifyWrite(OperationROR),										/* 0x77 RRA zpg, x */	ZeroXReadModifyWrite(OperationRRA, OperationADC),
		/* 0x78 SEI */			Program(OperationSEI),													/* 0x79 ADC abs, y */	AbsoluteYRead(OperationADC),
		/* 0x7a NOP # */		ImpliedNop(),															/* 0x7b RRA abs, y */	AbsoluteYReadModifyWrite(OperationRRA, OperationADC),
		/* 0x7c NOP abs, x */	AbsoluteXNop(),															/* 0x7d ADC abs, x */	AbsoluteXRead(OperationADC),
		/* 0x7e ROR abs, x */	AbsoluteXReadModifyWrite(OperationROR),									/* 0x7f RRA abs, x */	AbsoluteXReadModifyWrite(OperationRRA, OperationADC),
		/* 0x80 NOP # */		ImmediateNop(),															/* 0x81 STA x, ind */	IndexedIndirectWrite(OperationSTA),
		/* 0x82 NOP # */		ImmediateNop(),															/* 0x83 SAX x, ind */	IndexedIndirectWrite(OperationSAX),
		/* 0x84 STY zpg */		ZeroWrite(OperationSTY),												/* 0x85 STA zpg */		ZeroWrite(OperationSTA),
		/* 0x86 STX zpg */		ZeroWrite(OperationSTX),												/* 0x87 SAX zpg */		ZeroWrite(OperationSAX),
		/* 0x88 DEY */			Program(OperationDEY),													/* 0x89 NOP # */		ImmediateNop(),
		/* 0x8a TXA */			Program(OperationTXA),													/* 0x8b ANE # */		Immediate(OperationANE),
		/* 0x8c STY abs */		AbsoluteWrite(OperationSTY),											/* 0x8d STA abs */		AbsoluteWrite(OperationSTA),
		/* 0x8e STX abs */		AbsoluteWrite(OperationSTX),											/* 0x8f SAX abs */		AbsoluteWrite(OperationSAX),
		/* 0x90 BCC */			Program(OperationBCC),													/* 0x91 STA ind, y */	IndirectIndexedWrite(OperationSTA),
		/* 0x92 JAM */			JAM,																	/* 0x93 SHA ind, y */	IndirectIndexedWrite(OperationSHA),
		/* 0x94 STY zpg, x */	ZeroXWrite(OperationSTY),												/* 0x95 STA zpg, x */	ZeroXWrite(OperationSTA),
		/* 0x96 STX zpg, y */	ZeroYWrite(OperationSTX),												/* 0x97 SAX zpg, y */	ZeroYWrite(OperationSAX),
		/* 0x98 TYA */			Program(OperationTYA),													/* 0x99 STA abs, y */	AbsoluteYWrite(OperationSTA),
		/* 0x9a TXS */			Program(OperationTXS),													/* 0x9b SHS abs, y */	AbsoluteYWrite(OperationSHS),
		/* 0x9c SHY abs, x */	AbsoluteXWrite(OperationSHY),											/* 0x9d STA abs, x */	AbsoluteXWrite(OperationSTA),
		/* 0x9e SHX abs, y */	AbsoluteYWrite(OperationSHX),											/* 0x9f SHA abs, y */	AbsoluteYWrite(OperationSHA),
		/* 0xa0 LDY # */		Immediate(OperationLDY),												/* 0xa1 LDA x, ind */	IndexedIndirectRead(OperationLDA),
		/* 0xa2 LDX # */		Immediate(OperationLDX),												/* 0xa3 LAX x, ind */	IndexedIndirectRead(OperationLAX),
		/* 0xa4 LDY zpg */		ZeroRead(OperationLDY),													/* 0xa5 LDA zpg */		ZeroRead(OperationLDA),
		/* 0xa6 LDX zpg */		ZeroRead(OperationLDX),													/* 0xa7 LAX zpg */		ZeroRead(OperationLAX),
		/* 0xa8 TAY */			Program(OperationTAY),													/* 0xa9 LDA # */		Immediate(OperationLDA),
		/* 0xaa TAX */			Program(OperationTAX),													/* 0xab LXA # */		Immediate(OperationLXA),
		/* 0xac LDY abs */		AbsoluteRead(OperationLDY),												/* 0xad LDA abs */		AbsoluteRead(OperationLDA),
		/* 0xae LDX abs */		AbsoluteRead(OperationLDX),												/* 0xaf LAX abs */		AbsoluteRead(OperationLAX),
		/* 0xb0 BCS */			Program(OperationBCS),													/* 0xb1 LDA ind, y */	IndirectIndexedRead(OperationLDA),
		/* 0xb2 JAM */			JAM,																	/* 0xb3 LAX ind, y */	IndirectIndexedRead(OperationLAX),
		/* 0xb4 LDY zpg, x */	ZeroXRead(OperationLDY),												/* 0xb5 LDA zpg, x */	ZeroXRead(OperationLDA),
		/* 0xb6 LDX zpg, y */	ZeroYRead(OperationLDX),												/* 0xb7 LAX zpg, x */	ZeroYRead(OperationLAX),
		/* 0xb8 CLV */			Program(OperationCLV),													/* 0xb9 LDA abs, y */	AbsoluteYRead(OperationLDA),
		/* 0xba TSX */			Program(OperationTSX),													/* 0xbb LAS abs, y */	AbsoluteYRead(OperationLAS),
		/* 0xbc LDY abs, x */	AbsoluteXRead(OperationLDY),											/* 0xbd LDA abs, x */	AbsoluteXRead(OperationLDA),
		/* 0xbe LDX abs, y */	AbsoluteYRead(OperationLDX),											/* 0xbf LAX abs, y */	AbsoluteYRead(OperationLAX),
		/* 0xc0 CPY # */		Immediate(OperationCPY),												/* 0xc1 CMP x, ind */	IndexedIndirectRead(OperationCMP),
		/* 0xc2 NOP # */		ImmediateNop(),															/* 0xc3 DCP x, ind */	IndexedIndirectReadModifyWrite(OperationDecrementOperand, OperationCMP),
		/* 0xc4 CPY zpg */		ZeroRead(OperationCPY),													/* 0xc5 CMP zpg */		ZeroRead(OperationCMP),
		/* 0xc6 DEC zpg */		ZeroReadModifyWrite(OperationDEC),										/* 0xc7 DCP zpg */		ZeroReadModifyWrite(OperationDecrementOperand, OperationCMP),
		/* 0xc8 INY */			Program(OperationINY),													/* 0xc9 CMP # */		Immediate(OperationCMP),
		/* 0xca DEX */			Program(OperationDEX),													/* 0xcb ARR # */		Immediate(OperationSBX),
		/* 0xcc CPY abs */		AbsoluteRead(OperationCPY),												/* 0xcd CMP abs */		AbsoluteRead(OperationCMP),
		/* 0xce DEC abs */		AbsoluteReadModifyWrite(OperationDEC),									/* 0xcf DCP abs */		AbsoluteReadModifyWrite(OperationDecrementOperand, OperationCMP),
		/* 0xd0 BNE */			Program(OperationBNE),													/* 0xd1 CMP ind, y */	IndirectIndexedRead(OperationCMP),
		/* 0xd2 JAM */			JAM,																	/* 0xd3 DCP ind, y */	IndirectIndexedReadModifyWrite(OperationDecrementOperand, OperationCMP),
		/* 0xd4 NOP zpg, x */	ZeroXNop(),																/* 0xd5 CMP zpg, x */	ZeroXRead(OperationCMP),
		/* 0xd6 DEC zpg, x */	ZeroXReadModifyWrite(OperationDEC),										/* 0xd7 DCP zpg, x */	ZeroXReadModifyWrite(OperationDecrementOperand, OperationCMP),
		/* 0xd8 CLD */			Program(OperationCLD),													/* 0xd9 CMP abs, y */	AbsoluteYRead(OperationCMP),
		/* 0xda NOP # */		ImpliedNop(),															/* 0xdb DCP abs, y */	AbsoluteYReadModifyWrite(OperationDecrementOperand, OperationCMP),
		/* 0xdc NOP abs, x */	AbsoluteXNop(),															/* 0xdd CMP abs, x */	AbsoluteXRead(OperationCMP),
		/* 0xde DEC abs, x */	AbsoluteXReadModifyWrite(OperationDEC),									/* 0xdf DCP abs, x */	AbsoluteXReadModifyWrite(OperationDecrementOperand, OperationCMP),
		/* 0xe0 CPX # */		Immediate(OperationCPX),												/* 0xe1 SBC x, ind */	IndexedIndirectRead(OperationSBC),
		/* 0xe2 NOP # */		ImmediateNop(),															/* 0xe3 INS x, ind */	IndexedIndirectReadModifyWrite(OperationINS),
		/* 0xe4 CPX zpg */		ZeroRead(OperationCPX),													/* 0xe5 SBC zpg */		ZeroRead(OperationSBC),
		/* 0xe6 INC zpg */		ZeroReadModifyWrite(OperationINC),										/* 0xe7 INS zpg */		ZeroReadModifyWrite(OperationINS),
		/* 0xe8 INX */			Program(OperationINX),													/* 0xe9 SBC # */		Immediate(OperationSBC),
		/* 0xea NOP # */		ImpliedNop(),															/* 0xeb SBC # */		Immediate(OperationSBC),
		/* 0xec CPX abs */		AbsoluteRead(OperationCPX),												/* 0xed SBC abs */		AbsoluteRead(OperationSBC),
		/* 0xee INC abs */		AbsoluteReadModifyWrite(OperationINC),									/* 0xef INS abs */		AbsoluteReadModifyWrite(OperationINS),
		/* 0xf0 BEQ */			Program(OperationBEQ),													/* 0xf1 SBC ind, y */	IndirectIndexedRead(OperationSBC),
		/* 0xf2 JAM */			JAM,																	/* 0xf3 INS ind, y */	IndirectIndexedReadModifyWrite(OperationINS),
		/* 0xf4 NOP zpg, x */	ZeroXNop(),																/* 0xf5 SBC zpg, x */	ZeroXRead(OperationSBC),
		/* 0xf6 INC zpg, x */	ZeroXReadModifyWrite(OperationINC),										/* 0xf7 INS zpg, x */	ZeroXReadModifyWrite(OperationINS),
		/* 0xf8 SED */			Program(OperationSED),													/* 0xf9 SBC abs, y */	AbsoluteYRead(OperationSBC),
		/* 0xfa NOP # */		ImpliedNop(),															/* 0xfb INS abs, y */	AbsoluteYReadModifyWrite(OperationINS),
		/* 0xfc NOP abs, x */	AbsoluteXNop(),															/* 0xfd SBC abs, x */	AbsoluteXRead(OperationSBC),
		/* 0xfe INC abs, x */	AbsoluteXReadModifyWrite(OperationINC),									/* 0xff INS abs, x */	AbsoluteXReadModifyWrite(OperationINS),

		/* 0x100: Fetch, decode, execute. */
		{
			CycleFetchOperation,
			CycleFetchOperand,
			OperationDecodeOperation
		},

		/* 0x101: Reset. */
		Program(
			CycleFetchOperand,
			CycleFetchOperand,
			CycleNoWritePush,
			CycleNoWritePush,
			OperationRSTPickVector,
			CycleNoWritePush,
			OperationSetNMIRSTFlags,
			CycleReadVectorLow,
			CycleReadVectorHigh
		),

		/* 0x102: IRQ. */
		Program(
			CycleFetchOperand,
			CycleFetchOperand,
			CyclePushPCH,
			CyclePushPCL,
			OperationBRKPickVector,
			OperationSetOperandFromFlags,
			CyclePushOperand,
			OperationSetIRQFlags,
			CycleReadVectorLow,
			CycleReadVectorHigh
		),

		/* 0x103: NMI. */
		Program(
			CycleFetchOperand,
			CycleFetchOperand,
			CyclePushPCH,
			CyclePushPCL,
			OperationNMIPickVector,
			OperationSetOperandFromFlags,
			CyclePushOperand,
			OperationSetNMIRSTFlags,
			CycleReadVectorLow,
			CycleReadVectorHigh
		),

		/* 0x104: Do BRA. */
		Program(
			CycleReadFromPC,
			CycleAddSignedOperandToPC
		),

		/* 0x105: Do BBR or BBS. */
		Program(
			CycleFetchOperand,				// Fetch offset.
			OperationIncrementPC,
			CycleFetchFromHalfUpdatedPC,
			OperationAddSignedOperandToPC16
		),

		/* 0x106: Complete BBR or BBS without branching. */
		Program(
			CycleFetchOperand,
			OperationIncrementPC,
			CycleFetchFromHalfUpdatedPC
		)
	};

	static_assert(sizeof(operations_6502) == sizeof(operations_));

	// Install the basic 6502 table.
	memcpy(operations_, operations_6502, sizeof(operations_));

	// Patch the table according to the chip's personality.
	//
	// The 6502 and NES 6502 both have the same mapping of operation codes to actions
	// (respect for the decimal mode flag aside); included in that are 'unofficial'
	// operations — spots that are not formally defined to do anything but which the
	// processor makes no particular effort to react to in a well-defined way.
	//
	// The 65C02s add some official instructions but also ensure that all of the
	// undefined ones act as no-ops of various addressing modes.
	//
	// So the branch below has to add a bunch of new actions but also removes various
	// others by dint of replacing them with NOPs.
	//
	// Those 6502 opcodes that need redefining, one way or the other, are:
	//
	// 0x02, 0x03, 0x04, 0x07, 0x0b, 0x0c, 0x0f, 0x12, 0x13, 0x14, 0x17, 0x1a, 0x1b, 0x1c, 0x1f,
	// 0x22, 0x23, 0x27, 0x2b, 0x2f, 0x32, 0x33, 0x34, 0x37, 0x3a, 0x3b, 0x3c, 0x3f,
	// 0x42, 0x43, 0x47, 0x4b, 0x4f, 0x52, 0x53, 0x57, 0x5a, 0x5b, 0x5f,
	// 0x62, 0x63, 0x64, 0x67, 0x6b, 0x6f, 0x72, 0x73, 0x74, 0x77, 0x7b, 0x7a, 0x7c, 0x7f,
	// 0x80, 0x82, 0x83, 0x87, 0x89, 0x8b, 0x8f, 0x92, 0x93, 0x97, 0x9b, 0x9e, 0x9c, 0x9f,
	// 0xa3, 0xa7, 0xab, 0xaf, 0xb2, 0xb3, 0xb7, 0xbb, 0xbf,
	// 0xc3, 0xc7, 0xcb, 0xcf, 0xd2, 0xd3, 0xd7, 0xda, 0xdb, 0xdf,
	// 0xe3, 0xe7, 0xeb, 0xef, 0xf2, 0xf3, 0xf7, 0xfa, 0xfb, 0xff
	//
	// ... not including those that aren't defined on the 6502 but perform NOPs exactly like they
	// would on a 65C02.

#define Install(location, instructions) {\
		const InstructionList code = instructions;	\
		memcpy(&operations_[location], code, sizeof(InstructionList));	\
	}
	if(is_65c02(personality)) {
		// Add P[L/H][X/Y].
		Install(0x5a, Program(CyclePushY));
		Install(0xda, Program(CyclePushX));
		Install(0x7a, Program(CycleReadFromS, CyclePullY, OperationSetFlagsFromY));
		Install(0xfa, Program(CycleReadFromS, CyclePullX, OperationSetFlagsFromX));

		// Add BRA.
		Install(0x80, Program(OperationBRA));

		// The 1-byte, 1-cycle (!) NOPs.
		for(int c = 0x03; c <= 0xf3; c += 0x10) {
			Install(c, ImpliedNop());
		}
		for(int c = 0x0b; c <= 0xbb; c += 0x10) {
			Install(c, ImpliedNop());
		}
		for(int c = 0xeb; c <= 0xfb; c += 0x10) {
			Install(c, ImpliedNop());
		}

		// The 2-byte, 2-cycle NOPs that the 6502 doesn't have.
		for(int c = 0x02; c <= 0x62; c += 0x10) {
			Install(c, ImmediateNop());
		}

		// Correct JMP (abs) and install JMP (abs, x).
		Install(0x6c, Program(CycleReadAddressHLoadAddressL, CycleReadPCLFromAddress, CycleReadPCHFromAddressLowInc, CycleReadPCHFromAddressFixed));
		Install(0x7c, Program(
			CycleReadAddressHLoadAddressL,	// (3) read second byte of (addr)
			CycleAddXToAddressLowRead,		// (4) calculate addr+x, read from (addr+x) with high byte not yet calculated
			OperationCorrectAddressHigh, CycleReadPCLFromAddress,	// (5) read from real (addr+x)
			CycleReadPCHFromAddressInc		// (6) read from addr+x+1
		));

		// Add INA and DEA.
		Install(0x1a, Program(OperationINA));
		Install(0x3a, Program(OperationDEA));

		// Add (zp) operations.
		Install(0x12, ZeroIndirectRead(OperationORA));
		Install(0x32, ZeroIndirectRead(OperationAND));
		Install(0x52, ZeroIndirectRead(OperationEOR));
		Install(0x72, ZeroIndirectRead(OperationADC));
		Install(0x92, ZeroIndirectWrite(OperationSTA));
		Install(0xb2, ZeroIndirectRead(OperationLDA));
		Install(0xd2, ZeroIndirectRead(OperationCMP));
		Install(0xf2, ZeroIndirectRead(OperationSBC));

		// Add STZ.
		Install(0x9c, AbsoluteWrite(OperationSTZ));
		Install(0x9e, AbsoluteXWrite(OperationSTZ));
		Install(0x64, ZeroWrite(OperationSTZ));
		Install(0x74, ZeroXWrite(OperationSTZ));

		// Add the extra BITs.
		Install(0x34, ZeroXRead(OperationBIT));
		Install(0x3c, AbsoluteXRead(OperationBIT));
		Install(0x89, Immediate(OperationBITNoNV));

		// Add TRB and TSB.
		Install(0x04, ZeroReadModifyWrite(OperationTSB));
		Install(0x0c, AbsoluteReadModifyWrite(OperationTSB));
		Install(0x14, ZeroReadModifyWrite(OperationTRB));
		Install(0x1c, AbsoluteReadModifyWrite(OperationTRB));

		// Install faster ASL, LSR, ROL, ROR abs,[x/y]. Note: INC, DEC deliberately not improved.
		Install(0x1e, FastAbsoluteXReadModifyWrite(OperationASL));
		Install(0x1f, FastAbsoluteXReadModifyWrite(OperationASO));
		Install(0x3e, FastAbsoluteXReadModifyWrite(OperationROL));
		Install(0x3f, FastAbsoluteXReadModifyWrite(OperationRLA));
		Install(0x5e, FastAbsoluteXReadModifyWrite(OperationLSR));
		Install(0x5f, FastAbsoluteXReadModifyWrite(OperationLSE));
		Install(0x7e, FastAbsoluteXReadModifyWrite(OperationROR));
		Install(0x7f, FastAbsoluteXReadModifyWrite(OperationRRA, OperationADC));

		// Outstanding:
		// 0x07, 0x0f, 0x17, 0x1f,
		// 0x27, 0x2f, 0x37, 0x3f,
		// 0x47, 0x4f, 0x57, 0x5f,
		// 0x67, 0x6f, 0x77, 0x7f,
		// 0x87, 0x8f, 0x97, 0x9f,
		// 0xa7, 0xaf, 0xb7, 0xbf,
		// 0xc7, 0xcb, 0xcf, 0xd7, 0xdb, 0xdf,
		// 0xe7, 0xef, 0xf7, 0xff
		if(has_bbrbbsrmbsmb(personality)) {
			// Add BBS and BBR. These take five cycles. My guessed breakdown is:
			// 1. read opcode
			// 2. read operand
			// 3. read zero page
			// 4. read second operand
			// 5. read from PC without top byte fixed yet
			// ... with the caveat that (3) and (4) could be the other way around.
			for(int location = 0x0f; location <= 0xff; location += 0x10) {
				Install(location, Program(OperationLoadAddressZeroPage, CycleFetchOperandFromAddress, OperationBBRBBS));
			}

			// Add RMB and SMB.
			for(int c = 0x07; c <= 0x77; c += 0x10) {
				Install(c, ZeroReadModifyWrite(OperationRMB));
			}
			for(int c = 0x87; c <= 0xf7; c += 0x10) {
				Install(c, ZeroReadModifyWrite(OperationSMB));
			}
		} else {
			for(int location = 0x0f; location <= 0xef; location += 0x20) {
				Install(location, AbsoluteNop());
			}
			for(int location = 0x1f; location <= 0xff; location += 0x20) {
				Install(location, AbsoluteXNop());
			}
			for(int c = 0x07; c <= 0xe7; c += 0x20) {
				Install(c, ZeroNop());
			}
			for(int c = 0x17; c <= 0xf7; c += 0x20) {
				Install(c, ZeroXNop());
			}
		}

		// Outstanding:
		// 0xcb, 0xdb,
		if(has_stpwai(personality)) {
			Install(0xcb, Program(OperationScheduleWait));
			Install(0xdb, Program(OperationScheduleStop));
		} else {
			Install(0xcb, ImpliedNop());
			Install(0xdb, ZeroXNop());
		}
	}
#undef Install
}
