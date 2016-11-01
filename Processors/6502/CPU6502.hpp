//
//  CPU6502.hpp
//  CLK
//
//  Created by Thomas Harte on 09/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#ifndef CPU6502_cpp
#define CPU6502_cpp

#include <stdio.h>
#include <stdint.h>

namespace CPU6502 {

/*
	The list of registers that can be accessed via @c set_value_of_register and @c set_value_of_register.
*/
enum Register {
	LastOperationAddress,
	ProgramCounter,
	StackPointer,
	Flags,
	A,
	X,
	Y,
	S
};

/*
	Flags as defined on the 6502; can be used to decode the result of @c get_flags or to form a value for @c set_flags.
*/
enum Flag: uint8_t {
	Sign		= 0x80,
	Overflow	= 0x40,
	Always		= 0x20,
	Break		= 0x10,
	Decimal		= 0x08,
	Interrupt	= 0x04,
	Zero		= 0x02,
	Carry		= 0x01
};

/*!
	Subclasses will be given the task of performing bus operations, allowing them to provide whatever interface they like
	between a 6502 and the rest of the system. @c BusOperation lists the types of bus operation that may be requested.

	@c None is reserved for internal use. It will never be requested from a subclass. It is safe always to use the
	isReadOperation macro to make a binary choice between reading and writing.
*/
enum BusOperation {
	Read, ReadOpcode, Write, Ready, None
};

/*!
	Evaluates to `true` if the operation is a read; `false` if it is a write.
*/
#define isReadOperation(v)	(v == CPU6502::BusOperation::Read || v == CPU6502::BusOperation::ReadOpcode)

/*!
	An opcode that is guaranteed to cause the CPU to jam.
*/
extern const uint8_t JamOpcode;

/*!
	@abstact An abstract base class for emulation of a 6502 processor via the curiously recurring template pattern/f-bounded polymorphism.

	@discussion Subclasses should implement @c perform_bus_operation(BusOperation operation, uint16_t address, uint8_t *value) in
	order to provide the bus on which the 6502 operates and @c synchronise(), which is called upon completion of a continuous run
	of cycles to allow a subclass to bring any on-demand activities up to date.

	Additional functionality can be provided by the host machine by providing a jam handler and inserting jam opcodes where appropriate;
	that will cause call outs when the program counter reaches those addresses. @c return_from_subroutine can be used to exit from a
	jammed state.
*/
template <class T> class Processor {
	public:

		class JamHandler {
			public:
				virtual void processor_did_jam(Processor *processor, uint16_t address) = 0;
		};

	private:

		/*
			This emulation funcitons by decomposing instructions into micro programs, consisting of the micro operations
			as per the enum below. Each micro op takes at most one cycle. By convention, those called CycleX take a cycle
			to perform whereas those called OperationX occur for free (so, in effect, their cost is loaded onto the next cycle).
		*/
		enum MicroOp {
			CycleFetchOperation,						CycleFetchOperand,					OperationDecodeOperation,				CycleIncPCPushPCH,
			CyclePushPCH,								CyclePushPCL,						CyclePushA,								CyclePushOperand,
			OperationSetI,

			OperationBRKPickVector,						OperationNMIPickVector,				OperationRSTPickVector,
			CycleReadVectorLow,							CycleReadVectorHigh,

			CycleReadFromS,								CycleReadFromPC,
			CyclePullOperand,							CyclePullPCL,						CyclePullPCH,							CyclePullA,
			CycleNoWritePush,
			CycleReadAndIncrementPC,					CycleIncrementPCAndReadStack,		CycleIncrementPCReadPCHLoadPCL,			CycleReadPCHLoadPCL,
			CycleReadAddressHLoadAddressL,				CycleReadPCLFromAddress,			CycleReadPCHFromAddress,				CycleLoadAddressAbsolute,
			OperationLoadAddressZeroPage,				CycleLoadAddessZeroX,				CycleLoadAddessZeroY,					CycleAddXToAddressLow,
			CycleAddYToAddressLow,						CycleAddXToAddressLowRead,			OperationCorrectAddressHigh,			CycleAddYToAddressLowRead,
			OperationMoveToNextProgram,					OperationIncrementPC,
			CycleFetchOperandFromAddress,				CycleWriteOperandToAddress,			OperationCopyOperandFromA,				OperationCopyOperandToA,
			CycleIncrementPCFetchAddressLowFromOperand,	CycleAddXToOperandFetchAddressLow,	CycleIncrementOperandFetchAddressHigh,	OperationDecrementOperand,
			OperationIncrementOperand,					OperationORA,						OperationAND,							OperationEOR,
			OperationINS,								OperationADC,						OperationSBC,							OperationLDA,
			OperationLDX,								OperationLDY,						OperationLAX,							OperationSTA,
			OperationSTX,								OperationSTY,						OperationSAX,							OperationSHA,
			OperationSHX,								OperationSHY,						OperationSHS,							OperationCMP,
			OperationCPX,								OperationCPY,						OperationBIT,							OperationASL,
			OperationASO,								OperationROL,						OperationRLA,							OperationLSR,
			OperationLSE,								OperationASR,						OperationROR,							OperationRRA,
			OperationCLC,								OperationCLI,						OperationCLV,							OperationCLD,
			OperationSEC,								OperationSEI,						OperationSED,							OperationINC,
			OperationDEC,								OperationINX,						OperationDEX,							OperationINY,
			OperationDEY,								OperationBPL,						OperationBMI,							OperationBVC,
			OperationBVS,								OperationBCC,						OperationBCS,							OperationBNE,
			OperationBEQ,								OperationTXA,						OperationTYA,							OperationTXS,
			OperationTAY,								OperationTAX,						OperationTSX,							OperationARR,
			OperationSBX,								OperationLXA,						OperationANE,							OperationANC,
			OperationLAS,								CycleAddSignedOperandToPC,			OperationSetFlagsFromOperand,			OperationSetOperandFromFlagsWithBRKSet,
			OperationSetOperandFromFlags,
			OperationSetFlagsFromA,
			CycleScheduleJam
		};

#define JAM {CycleFetchOperand, CycleScheduleJam, OperationMoveToNextProgram}

		union RegisterPair {
			uint16_t full;
			struct {
				uint8_t low, high;
			} bytes;
		};

		/*
			Storage for the 6502 registers; F is stored as individual flags.
		*/
		RegisterPair _pc, _lastOperationPC;
		uint8_t _a, _x, _y, _s;
		uint8_t _carryFlag, _negativeResult, _zeroResult, _decimalFlag, _overflowFlag, _inverseInterruptFlag;

		/*
			Temporary state for the micro programs.
		*/
		uint8_t _operation, _operand;
		RegisterPair _address, _nextAddress;

		/*
			Up to four programs can be scheduled; each will be carried out in turn. This
			storage maintains pointers to the scheduled list of programs.

			Programs should be terminated by an OperationMoveToNextProgram, causing this
			queue to take that step.
		*/
		const MicroOp *_scheduledPrograms[4];
		unsigned int _scheduleProgramsWritePointer, _scheduleProgramsReadPointer, _scheduleProgramProgramCounter;

		/*
			Temporary storage allowing a common dispatch point for calling perform_bus_operation;
			possibly deferring is no longer of value.
		*/
		BusOperation _nextBusOperation;
		uint16_t _busAddress;
		uint8_t *_busValue;

		/*!
			Schedules a new program, adding it to the end of the queue. Programs should be
			terminated with a OperationMoveToNextProgram. No attempt to copy the program
			is made; a non-owning reference is kept.

			@param program The program to schedule.
		*/
		inline void schedule_program(const MicroOp *program)
		{
			_scheduledPrograms[_scheduleProgramsWritePointer] = program;
			_scheduleProgramsWritePointer = (_scheduleProgramsWritePointer+1)&3;
		}

		/*!
			Gets the flags register.

			@see set_flags

			@returns The current value of the flags register.
		*/
		uint8_t get_flags()
		{
			return _carryFlag | _overflowFlag | (_inverseInterruptFlag ^ Flag::Interrupt) | (_negativeResult & 0x80) | (_zeroResult ? 0 : Flag::Zero) | Flag::Always | _decimalFlag;
		}

		/*!
			Sets the flags register.

			@see set_flags

			@param flags The new value of the flags register.
		*/
		void set_flags(uint8_t flags)
		{
			_carryFlag				= flags		& Flag::Carry;
			_negativeResult			= flags		& Flag::Sign;
			_zeroResult				= (~flags)	& Flag::Zero;
			_overflowFlag			= flags		& Flag::Overflow;
			_inverseInterruptFlag	= (~flags)	& Flag::Interrupt;
			_decimalFlag			= flags		& Flag::Decimal;
		}

		/*!
			Schedules the program corresponding to the specified operation.

			@param operation The operation code for which to schedule a program.
		*/
		inline void decode_operation(uint8_t operation)
		{
#define Program(...)						{__VA_ARGS__, OperationMoveToNextProgram}

#define Absolute							CycleLoadAddressAbsolute
#define AbsoluteXr							CycleLoadAddressAbsolute,					CycleAddXToAddressLow,					OperationCorrectAddressHigh
#define AbsoluteYr							CycleLoadAddressAbsolute,					CycleAddYToAddressLow,					OperationCorrectAddressHigh
#define AbsoluteX							CycleLoadAddressAbsolute,					CycleAddXToAddressLowRead,				OperationCorrectAddressHigh
#define AbsoluteY							CycleLoadAddressAbsolute,					CycleAddYToAddressLowRead,				OperationCorrectAddressHigh
#define Zero								OperationLoadAddressZeroPage
#define ZeroX								CycleLoadAddessZeroX
#define ZeroY								CycleLoadAddessZeroY
#define IndexedIndirect						CycleIncrementPCFetchAddressLowFromOperand, CycleAddXToOperandFetchAddressLow,		CycleIncrementOperandFetchAddressHigh
#define IndirectIndexedr					CycleIncrementPCFetchAddressLowFromOperand, CycleIncrementOperandFetchAddressHigh,	CycleAddYToAddressLow,					OperationCorrectAddressHigh
#define IndirectIndexed						CycleIncrementPCFetchAddressLowFromOperand, CycleIncrementOperandFetchAddressHigh,	CycleAddYToAddressLowRead,				OperationCorrectAddressHigh

#define Read(...)							CycleFetchOperandFromAddress,	__VA_ARGS__
#define Write(...)							__VA_ARGS__,					CycleWriteOperandToAddress
#define ReadModifyWrite(...)				CycleFetchOperandFromAddress,	CycleWriteOperandToAddress,			__VA_ARGS__,							CycleWriteOperandToAddress

#define AbsoluteRead(op)					Program(Absolute,			Read(op))
#define AbsoluteXRead(op)					Program(AbsoluteXr,			Read(op))
#define AbsoluteYRead(op)					Program(AbsoluteYr,			Read(op))
#define ZeroRead(...)						Program(Zero,				Read(__VA_ARGS__))
#define ZeroXRead(op)						Program(ZeroX,				Read(op))
#define ZeroYRead(op)						Program(ZeroY,				Read(op))
#define IndexedIndirectRead(op)				Program(IndexedIndirect,	Read(op))
#define IndirectIndexedRead(op)				Program(IndirectIndexedr,	Read(op))

#define AbsoluteWrite(op)					Program(Absolute,			Write(op))
#define AbsoluteXWrite(op)					Program(AbsoluteX,			Write(op))
#define AbsoluteYWrite(op)					Program(AbsoluteY,			Write(op))
#define ZeroWrite(op)						Program(Zero,				Write(op))
#define ZeroXWrite(op)						Program(ZeroX,				Write(op))
#define ZeroYWrite(op)						Program(ZeroY,				Write(op))
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

#define Immediate(op)						Program(OperationIncrementPC,		op)
#define Implied(op)							Program(OperationCopyOperandFromA,	op,	OperationCopyOperandToA)

#define ZeroNop()							Program(Zero, CycleFetchOperandFromAddress)
#define ZeroXNop()							Program(ZeroX, CycleFetchOperandFromAddress)
#define AbsoluteNop()						Program(Absolute)
#define AbsoluteXNop()						Program(AbsoluteX)
#define ImpliedNop()						{OperationMoveToNextProgram}
#define ImmediateNop()						Program(OperationIncrementPC)

			static const MicroOp operations[256][10] = {

			/* 0x00 BRK */			Program(CycleIncPCPushPCH, CyclePushPCL, OperationBRKPickVector, OperationSetOperandFromFlagsWithBRKSet, CyclePushOperand, OperationSetI, CycleReadVectorLow, CycleReadVectorHigh),
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
			/* 0x6c JMP (abs) */	Program(CycleReadAddressHLoadAddressL, CycleReadPCLFromAddress, CycleReadPCHFromAddress),
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
			};

#undef Program
#undef Absolute
#undef AbsoluteX
#undef AbsoluteY
#undef Zero
#undef ZeroX
#undef ZeroY
#undef IndexedIndirect
#undef IndirectIndexed
#undef Read
#undef Write
#undef ReadModifyWrite
#undef AbsoluteRead
#undef AbsoluteXRead
#undef AbsoluteYRead
#undef ZeroRead
#undef ZeroXRead
#undef ZeroYRead
#undef IndexedIndirectRead
#undef IndirectIndexedRead
#undef AbsoluteWrite
#undef AbsoluteXWrite
#undef AbsoluteYWrite
#undef ZeroWrite
#undef ZeroXWrite
#undef ZeroYWrite
#undef IndexedIndirectWrite
#undef IndirectIndexedWrite
#undef AbsoluteReadModifyWrite
#undef AbsoluteXReadModifyWrite
#undef AbsoluteYReadModifyWrite
#undef ZeroReadModifyWrite
#undef ZeroXReadModifyWrite
#undef ZeroYReadModify
#undef IndexedIndirectReadModify
#undef IndirectIndexedReadModify
#undef Immediate
#undef Implied

			schedule_program(operations[operation]);
		}

		bool _is_jammed;
		JamHandler *_jam_handler;

		int _cycles_left_to_run;

		enum InterruptRequestFlags: uint8_t {
			Reset		= 0x80,
			IRQ			= Flag::Interrupt,
			NMI			= 0x20,

			PowerOn		= 0x10,
		};
		uint8_t _interrupt_requests;

		bool _ready_is_active;
		bool _ready_line_is_enabled;

		uint8_t _irq_line, _irq_request_history;
		bool _nmi_line_is_enabled, _set_overflow_line_is_enabled;

		/*!
			Gets the program representing an RST response.

			@returns The program representing an RST response.
		*/
		inline const MicroOp *get_reset_program() {
			static const MicroOp reset[] = {
				CycleFetchOperand,
				CycleFetchOperand,
				CycleNoWritePush,
				CycleNoWritePush,
				OperationRSTPickVector,
				CycleNoWritePush,
				CycleReadVectorLow,
				CycleReadVectorHigh,
				OperationMoveToNextProgram
			};
			return reset;
		}

		/*!
			Gets the program representing an IRQ response.

			@returns The program representing an IRQ response.
		*/
		inline const MicroOp *get_irq_program() {
			static const MicroOp reset[] = {
				CycleFetchOperand,
				CycleFetchOperand,
				CyclePushPCH,
				CyclePushPCL,
				OperationBRKPickVector,
				OperationSetOperandFromFlags,
				CyclePushOperand,
				OperationSetI,
				CycleReadVectorLow,
				CycleReadVectorHigh,
				OperationMoveToNextProgram
			};
			return reset;
		}

		/*!
			Gets the program representing an NMI response.

			@returns The program representing an NMI response.
		*/
		inline const MicroOp *get_nmi_program() {
			static const MicroOp reset[] = {
				CycleFetchOperand,
				CycleFetchOperand,
				CyclePushPCH,
				CyclePushPCL,
				OperationNMIPickVector,
				OperationSetOperandFromFlags,
				CyclePushOperand,
				CycleReadVectorLow,
				CycleReadVectorHigh,
				OperationMoveToNextProgram
			};
			return reset;
		}

	protected:
		Processor() :
			_scheduleProgramsReadPointer(0),
			_scheduleProgramsWritePointer(0),
			_is_jammed(false),
			_jam_handler(nullptr),
			_cycles_left_to_run(0),
			_ready_line_is_enabled(false),
			_ready_is_active(false),
			_scheduledPrograms{nullptr, nullptr, nullptr, nullptr},
			_inverseInterruptFlag(0),
			_s(0),
			_nextBusOperation(BusOperation::None),
			_interrupt_requests(InterruptRequestFlags::PowerOn),
			_irq_line(0),
			_nmi_line_is_enabled(false),
			_set_overflow_line_is_enabled(false)
		{
			// only the interrupt flag is defined upon reset but get_flags isn't going to
			// mask the other flags so we need to do that, at least
			_carryFlag &= Flag::Carry;
			_decimalFlag &= Flag::Decimal;
			_overflowFlag &= Flag::Overflow;
		}

	public:
		/*!
			Runs the 6502 for a supplied number of cycles.

			@discussion Subclasses must implement @c perform_bus_operation(BusOperation operation, uint16_t address, uint8_t *value) .
			The 6502 will call that method for all bus accesses. The 6502 is guaranteed to perform one bus operation call per cycle.

			@param number_of_cycles The number of cycles to run the 6502 for.
		*/
		void run_for_cycles(int number_of_cycles)
		{
			static const MicroOp doBranch[] = {
				CycleReadFromPC,
				CycleAddSignedOperandToPC,
				OperationMoveToNextProgram
			};
			static uint8_t throwaway_target;
			static const MicroOp fetch_decode_execute[] =
			{
				CycleFetchOperation,
				CycleFetchOperand,
				OperationDecodeOperation,
				OperationMoveToNextProgram
			};

			// These plus program below act to give the compiler permission to update these values
			// without touching the class storage (i.e. it explicitly says they need be completely up
			// to date in this stack frame only); which saves some complicated addressing
			unsigned int scheduleProgramsReadPointer = _scheduleProgramsReadPointer;
			unsigned int scheduleProgramProgramCounter = _scheduleProgramProgramCounter;
			RegisterPair nextAddress = _nextAddress;
			BusOperation nextBusOperation = _nextBusOperation;
			uint16_t busAddress = _busAddress;
			uint8_t *busValue = _busValue;

#define checkSchedule(op) \
	if(!_scheduledPrograms[scheduleProgramsReadPointer]) {\
		scheduleProgramsReadPointer = _scheduleProgramsWritePointer = scheduleProgramProgramCounter = 0;\
		if(_interrupt_requests) {\
			if(_interrupt_requests & (InterruptRequestFlags::Reset | InterruptRequestFlags::PowerOn)) {\
				_interrupt_requests &= ~InterruptRequestFlags::PowerOn;\
				schedule_program(get_reset_program());\
			} else if(_interrupt_requests & InterruptRequestFlags::NMI) {\
				_interrupt_requests &= ~InterruptRequestFlags::NMI;\
				schedule_program(get_nmi_program());\
			} else if(_interrupt_requests & InterruptRequestFlags::IRQ) {\
				schedule_program(get_irq_program());\
			} \
		} else {\
			schedule_program(fetch_decode_execute);\
		}\
		op;\
	}

#define bus_access() \
	_interrupt_requests = (_interrupt_requests & ~InterruptRequestFlags::IRQ) | _irq_request_history;	\
	_irq_request_history = _irq_line & _inverseInterruptFlag;	\
	number_of_cycles -= static_cast<T *>(this)->perform_bus_operation(nextBusOperation, busAddress, busValue);	\
	nextBusOperation = BusOperation::None;	\
	if(number_of_cycles <= 0) break;

			checkSchedule();
			number_of_cycles += _cycles_left_to_run;
			const MicroOp *program = _scheduledPrograms[scheduleProgramsReadPointer];

			while(number_of_cycles > 0) {

				while (_ready_is_active && number_of_cycles > 0) {
					number_of_cycles -= static_cast<T *>(this)->perform_bus_operation(BusOperation::Ready, busAddress, busValue);
				}

				if(!_ready_is_active)
				{
					if(nextBusOperation != BusOperation::None)
					{
						bus_access();
					}

					while(1) {

						const MicroOp cycle = program[scheduleProgramProgramCounter];
						scheduleProgramProgramCounter++;

#define read_op(val, addr)		nextBusOperation = BusOperation::ReadOpcode;	busAddress = addr;		busValue = &val
#define read_mem(val, addr)		nextBusOperation = BusOperation::Read;			busAddress = addr;		busValue = &val
#define throwaway_read(addr)	nextBusOperation = BusOperation::Read;			busAddress = addr;		busValue = &throwaway_target
#define write_mem(val, addr)	nextBusOperation = BusOperation::Write;			busAddress = addr;		busValue = &val

						switch(cycle) {

#pragma mark - Fetch/Decode

							case CycleFetchOperation: {
								_lastOperationPC = _pc;
//								printf("%04x	x:%02x\n", _pc.full, _x);
								_pc.full++;
								read_op(_operation, _lastOperationPC.full);

//								static int last_cycles_left_to_run = 0;
//								static bool printed_map[256] = {false};
//								if(!printed_map[_operation]) {
//									printed_map[_operation] = true;
//									if(last_cycles_left_to_run > _cycles_left_to_run)
//										printf("%02x %d\n", _operation, last_cycles_left_to_run - _cycles_left_to_run);
//									else
//										printf("%02x\n", _operation);
//								}
//								last_cycles_left_to_run = _cycles_left_to_run;
							} break;

							case CycleFetchOperand:
								read_mem(_operand, _pc.full);
							break;

							case OperationDecodeOperation:
//								printf("d %02x\n", _operation);
								decode_operation(_operation);
							continue;

							case OperationMoveToNextProgram:
								_scheduledPrograms[scheduleProgramsReadPointer] = NULL;
								scheduleProgramsReadPointer = (scheduleProgramsReadPointer+1)&3;
								scheduleProgramProgramCounter = 0;
								checkSchedule();
								program = _scheduledPrograms[scheduleProgramsReadPointer];
							continue;

#define push(v) \
		{\
			uint16_t targetAddress = _s | 0x100; _s--;\
			write_mem(v, targetAddress);\
		}

							case CycleIncPCPushPCH:				_pc.full++;														// deliberate fallthrough
							case CyclePushPCH:					push(_pc.bytes.high);											break;
							case CyclePushPCL:					push(_pc.bytes.low);											break;
							case CyclePushOperand:				push(_operand);													break;
							case CyclePushA:					push(_a);														break;
							case CycleNoWritePush:
							{
								uint16_t targetAddress = _s | 0x100; _s--;
								read_mem(_operand, targetAddress);
							}
							break;

#undef push

							case CycleReadFromS:				throwaway_read(_s | 0x100);										break;
							case CycleReadFromPC:				throwaway_read(_pc.full);										break;

							case OperationBRKPickVector:
								// NMI can usurp BRK-vector operations
								nextAddress.full = (_interrupt_requests & InterruptRequestFlags::NMI) ? 0xfffa : 0xfffe;
								_interrupt_requests &= ~InterruptRequestFlags::NMI;	// TODO: this probably doesn't happen now?
							continue;
							case OperationNMIPickVector:		nextAddress.full = 0xfffa;											continue;
							case OperationRSTPickVector:		nextAddress.full = 0xfffc;											continue;
							case CycleReadVectorLow:			read_mem(_pc.bytes.low, nextAddress.full);							break;
							case CycleReadVectorHigh:			read_mem(_pc.bytes.high, nextAddress.full+1);						break;
							case OperationSetI:					_inverseInterruptFlag = 0;											continue;

							case CyclePullPCL:					_s++; read_mem(_pc.bytes.low, _s | 0x100);							break;
							case CyclePullPCH:					_s++; read_mem(_pc.bytes.high, _s | 0x100);							break;
							case CyclePullA:					_s++; read_mem(_a, _s | 0x100);										break;
							case CyclePullOperand:				_s++; read_mem(_operand, _s | 0x100);								break;
							case OperationSetFlagsFromOperand:	set_flags(_operand);												continue;
							case OperationSetOperandFromFlagsWithBRKSet: _operand = get_flags() | Flag::Break;						continue;
							case OperationSetOperandFromFlags:  _operand = get_flags();												continue;
							case OperationSetFlagsFromA:		_zeroResult = _negativeResult = _a;									continue;

							case CycleIncrementPCAndReadStack:	_pc.full++; throwaway_read(_s | 0x100);								break;
							case CycleReadPCLFromAddress:		read_mem(_pc.bytes.low, _address.full);								break;
							case CycleReadPCHFromAddress:		_address.bytes.low++; read_mem(_pc.bytes.high, _address.full);		break;

							case CycleReadAndIncrementPC: {
								uint16_t oldPC = _pc.full;
								_pc.full++;
								throwaway_read(oldPC);
							} break;

#pragma mark - JAM

							case CycleScheduleJam: {
								_is_jammed = true;
								static const MicroOp jam[] = JAM;
								schedule_program(jam);

								if(_jam_handler) {
									_jam_handler->processor_did_jam(this, _pc.full - 1);
									checkSchedule(_is_jammed = false; program = _scheduledPrograms[scheduleProgramsReadPointer]);
								}
							} continue;

#pragma mark - Bitwise

							case OperationORA:	_a |= _operand;	_negativeResult = _zeroResult = _a;		continue;
							case OperationAND:	_a &= _operand;	_negativeResult = _zeroResult = _a;		continue;
							case OperationEOR:	_a ^= _operand;	_negativeResult = _zeroResult = _a;		continue;

#pragma mark - Load and Store

							case OperationLDA:	_a = _negativeResult = _zeroResult = _operand;			continue;
							case OperationLDX:	_x = _negativeResult = _zeroResult = _operand;			continue;
							case OperationLDY:	_y = _negativeResult = _zeroResult = _operand;			continue;
							case OperationLAX:	_a = _x = _negativeResult = _zeroResult = _operand;		continue;

							case OperationSTA:	_operand = _a;											continue;
							case OperationSTX:	_operand = _x;											continue;
							case OperationSTY:	_operand = _y;											continue;
							case OperationSAX:	_operand = _a & _x;										continue;
							case OperationSHA:	_operand = _a & _x & (_address.bytes.high+1);			continue;
							case OperationSHX:	_operand = _x & (_address.bytes.high+1);				continue;
							case OperationSHY:	_operand = _y & (_address.bytes.high+1);				continue;
							case OperationSHS:	_s = _a & _x; _operand = _s & (_address.bytes.high+1);	continue;

							case OperationLXA:
								_a = _x = (_a | 0xee) & _operand;
								_negativeResult = _zeroResult = _a;
							continue;

#pragma mark - Compare

							case OperationCMP: {
								const uint16_t temp16 = _a - _operand;
								_negativeResult = _zeroResult = (uint8_t)temp16;
								_carryFlag = ((~temp16) >> 8)&1;
							} continue;
							case OperationCPX: {
								const uint16_t temp16 = _x - _operand;
								_negativeResult = _zeroResult = (uint8_t)temp16;
								_carryFlag = ((~temp16) >> 8)&1;
							} continue;
							case OperationCPY: {
								const uint16_t temp16 = _y - _operand;
								_negativeResult = _zeroResult = (uint8_t)temp16;
								_carryFlag = ((~temp16) >> 8)&1;
							} continue;

#pragma mark - BIT

							case OperationBIT:
								_zeroResult = _operand & _a;
								_negativeResult = _operand;
								_overflowFlag = _operand&Flag::Overflow;
							continue;

#pragma mark ADC/SBC (and INS)

							case OperationINS:
								_operand++;			// deliberate fallthrough
							case OperationSBC:
								if(_decimalFlag) {
									const uint16_t notCarry = _carryFlag ^ 0x1;
									const uint16_t decimalResult = (uint16_t)_a - (uint16_t)_operand - notCarry;
									uint16_t temp16;

									temp16 = (_a&0xf) - (_operand&0xf) - notCarry;
									if(temp16 > 0xf) temp16 -= 0x6;
									temp16 = (temp16&0x0f) | ((temp16 > 0x0f) ? 0xfff0 : 0x00);
									temp16 += (_a&0xf0) - (_operand&0xf0);

									_overflowFlag = ( ( (decimalResult^_a)&(~decimalResult^_operand) )&0x80) >> 1;
									_negativeResult = (uint8_t)temp16;
									_zeroResult = (uint8_t)decimalResult;

									if(temp16 > 0xff) temp16 -= 0x60;

									_carryFlag = (temp16 > 0xff) ? 0 : Flag::Carry;
									_a = (uint8_t)temp16;
									continue;
								} else {
									_operand = ~_operand;
								}

							// deliberate fallthrough
							case OperationADC:
								if(_decimalFlag) {
									const uint16_t decimalResult = (uint16_t)_a + (uint16_t)_operand + (uint16_t)_carryFlag;

									uint8_t low_nibble = (_a & 0xf) + (_operand & 0xf) + _carryFlag;
									if(low_nibble >= 0xa) low_nibble = ((low_nibble + 0x6) & 0xf) + 0x10;
									uint16_t result = (uint16_t)(_a & 0xf0) + (uint16_t)(_operand & 0xf0) + (uint16_t)low_nibble;
									_negativeResult = (uint8_t)result;
									_overflowFlag =  (( (result^_a)&(result^_operand) )&0x80) >> 1;
									if(result >= 0xa0) result += 0x60;

									_carryFlag = (result >> 8) ? 1 : 0;
									_a = (uint8_t)result;
									_zeroResult = (uint8_t)decimalResult;
								} else {
									const uint16_t result = (uint16_t)_a + (uint16_t)_operand + (uint16_t)_carryFlag;
									_overflowFlag =  (( (result^_a)&(result^_operand) )&0x80) >> 1;
									_negativeResult = _zeroResult = _a = (uint8_t)result;
									_carryFlag = (result >> 8)&1;
								}

								// fix up in case this was INS
								if(cycle == OperationINS) _operand = ~_operand;
							continue;

#pragma mark - Shifts and Rolls

							case OperationASL:
								_carryFlag = _operand >> 7;
								_operand <<= 1;
								_negativeResult = _zeroResult = _operand;
							continue;

							case OperationASO:
								_carryFlag = _operand >> 7;
								_operand <<= 1;
								_a |= _operand;
								_negativeResult = _zeroResult = _a;
							continue;

							case OperationROL: {
								const uint8_t temp8 = (uint8_t)((_operand << 1) | _carryFlag);
								_carryFlag = _operand >> 7;
								_operand = _negativeResult = _zeroResult = temp8;
							} continue;

							case OperationRLA: {
								const uint8_t temp8 = (uint8_t)((_operand << 1) | _carryFlag);
								_carryFlag = _operand >> 7;
								_operand = temp8;
								_a &= _operand;
								_negativeResult = _zeroResult = _a;
							} continue;

							case OperationLSR:
								_carryFlag = _operand & 1;
								_operand >>= 1;
								_negativeResult = _zeroResult = _operand;
							continue;

							case OperationLSE:
								_carryFlag = _operand & 1;
								_operand >>= 1;
								_a ^= _operand;
								_negativeResult = _zeroResult = _a;
							continue;

							case OperationASR:
								_a &= _operand;
								_carryFlag = _a & 1;
								_a >>= 1;
								_negativeResult = _zeroResult = _a;
							continue;

							case OperationROR: {
								const uint8_t temp8 = (uint8_t)((_operand >> 1) | (_carryFlag << 7));
								_carryFlag = _operand & 1;
								_operand = _negativeResult = _zeroResult = temp8;
							} continue;

							case OperationRRA: {
								const uint8_t temp8 = (uint8_t)((_operand >> 1) | (_carryFlag << 7));
								_carryFlag = _operand & 1;
								_operand = temp8;
							} continue;

							case OperationDecrementOperand: _operand--; continue;
							case OperationIncrementOperand: _operand++; continue;

							case OperationCLC: _carryFlag = 0;								continue;
							case OperationCLI: _inverseInterruptFlag = Flag::Interrupt;		continue;
							case OperationCLV: _overflowFlag = 0;							continue;
							case OperationCLD: _decimalFlag = 0;							continue;

							case OperationSEC: _carryFlag = Flag::Carry;		continue;
							case OperationSEI: _inverseInterruptFlag = 0;		continue;
							case OperationSED: _decimalFlag = Flag::Decimal;	continue;

							case OperationINC: _operand++; _negativeResult = _zeroResult = _operand; continue;
							case OperationDEC: _operand--; _negativeResult = _zeroResult = _operand; continue;
							case OperationINX: _x++; _negativeResult = _zeroResult = _x; continue;
							case OperationDEX: _x--; _negativeResult = _zeroResult = _x; continue;
							case OperationINY: _y++; _negativeResult = _zeroResult = _y; continue;
							case OperationDEY: _y--; _negativeResult = _zeroResult = _y; continue;

							case OperationANE:
								_a = (_a | 0xee) & _operand & _x;
								_negativeResult = _zeroResult = _a;
							continue;

							case OperationANC:
								_a &= _operand;
								_negativeResult = _zeroResult = _a;
								_carryFlag = _a >> 7;
							continue;

							case OperationLAS:
								_a = _x = _s = _s & _operand;
								_negativeResult = _zeroResult = _a;
							continue;

#pragma mark - Addressing Mode Work

							case CycleAddXToAddressLow:
								nextAddress.full = _address.full + _x;
								_address.bytes.low = nextAddress.bytes.low;
								if(_address.bytes.high != nextAddress.bytes.high) {
									throwaway_read(_address.full);
									break;
								}
							continue;
							case CycleAddXToAddressLowRead:
								nextAddress.full = _address.full + _x;
								_address.bytes.low = nextAddress.bytes.low;
								throwaway_read(_address.full);
							break;
							case CycleAddYToAddressLow:
								nextAddress.full = _address.full + _y;
								_address.bytes.low = nextAddress.bytes.low;
								if(_address.bytes.high != nextAddress.bytes.high) {
									throwaway_read(_address.full);
									break;
								}
							continue;
							case CycleAddYToAddressLowRead:
								nextAddress.full = _address.full + _y;
								_address.bytes.low = nextAddress.bytes.low;
								throwaway_read(_address.full);
							break;
							case OperationCorrectAddressHigh:
								_address.full = nextAddress.full;
							continue;
							case CycleIncrementPCFetchAddressLowFromOperand:
								_pc.full++;
								read_mem(_address.bytes.low, _operand);
							break;
							case CycleAddXToOperandFetchAddressLow:
								_operand += _x;
								read_mem(_address.bytes.low, _operand);
							break;
							case CycleIncrementOperandFetchAddressHigh:
								_operand++;
								read_mem(_address.bytes.high, _operand);
							break;
							case CycleIncrementPCReadPCHLoadPCL:	// deliberate fallthrough
								_pc.full++;
							case CycleReadPCHLoadPCL: {
								uint16_t oldPC = _pc.full;
								_pc.bytes.low = _operand;
								read_mem(_pc.bytes.high, oldPC);
							} break;

							case CycleReadAddressHLoadAddressL:
								_address.bytes.low = _operand; _pc.full++;
								read_mem(_address.bytes.high, _pc.full);
							break;

							case CycleLoadAddressAbsolute: {
								uint16_t nextPC = _pc.full+1;
								_pc.full += 2;
								_address.bytes.low = _operand;
								read_mem(_address.bytes.high, nextPC);
							} break;

							case OperationLoadAddressZeroPage:
								_pc.full++;
								_address.full = _operand;
							continue;

							case CycleLoadAddessZeroX:
								_pc.full++;
								_address.full = (_operand + _x)&0xff;
								throwaway_read(_operand);
							break;

							case CycleLoadAddessZeroY:
								_pc.full++;
								_address.full = (_operand + _y)&0xff;
								throwaway_read(_operand);
							break;

							case OperationIncrementPC:			_pc.full++;						continue;
							case CycleFetchOperandFromAddress:	read_mem(_operand, _address.full);	break;
							case CycleWriteOperandToAddress:	write_mem(_operand, _address.full);	break;
							case OperationCopyOperandFromA:		_operand = _a;					continue;
							case OperationCopyOperandToA:		_a = _operand;					continue;

#pragma mark - Branching

#define BRA(condition)	_pc.full++; if(condition) schedule_program(doBranch)

							case OperationBPL: BRA(!(_negativeResult&0x80));				continue;
							case OperationBMI: BRA(_negativeResult&0x80);					continue;
							case OperationBVC: BRA(!_overflowFlag);							continue;
							case OperationBVS: BRA(_overflowFlag);							continue;
							case OperationBCC: BRA(!_carryFlag);							continue;
							case OperationBCS: BRA(_carryFlag);								continue;
							case OperationBNE: BRA(_zeroResult);							continue;
							case OperationBEQ: BRA(!_zeroResult);							continue;

							case CycleAddSignedOperandToPC:
								nextAddress.full = (uint16_t)(_pc.full + (int8_t)_operand);
								_pc.bytes.low = nextAddress.bytes.low;
								if(nextAddress.bytes.high != _pc.bytes.high) {
									uint16_t halfUpdatedPc = _pc.full;
									_pc.full = nextAddress.full;
									throwaway_read(halfUpdatedPc);
									break;
								}
							continue;

#undef BRA

#pragma mark - Transfers

							case OperationTXA: _zeroResult = _negativeResult = _a = _x; continue;
							case OperationTYA: _zeroResult = _negativeResult = _a = _y; continue;
							case OperationTXS: _s = _x;									continue;
							case OperationTAY: _zeroResult = _negativeResult = _y = _a; continue;
							case OperationTAX: _zeroResult = _negativeResult = _x = _a; continue;
							case OperationTSX: _zeroResult = _negativeResult = _x = _s; continue;

							case OperationARR:
								if(_decimalFlag) {
									_a &= _operand;
									uint8_t unshiftedA = _a;
									_a = (uint8_t)((_a >> 1) | (_carryFlag << 7));
									_zeroResult = _negativeResult = _a;
									_overflowFlag = (_a^(_a << 1))&Flag::Overflow;

									if((unshiftedA&0xf) + (unshiftedA&0x1) > 5) _a = ((_a + 6)&0xf) | (_a & 0xf0);

									_carryFlag = ((unshiftedA&0xf0) + (unshiftedA&0x10) > 0x50) ? 1 : 0;
									if(_carryFlag) _a += 0x60;
								} else {
									_a &= _operand;
									_a = (uint8_t)((_a >> 1) | (_carryFlag << 7));
									_negativeResult = _zeroResult = _a;
									_carryFlag = (_a >> 6)&1;
									_overflowFlag = (_a^(_a << 1))&Flag::Overflow;
								}
							continue;

							case OperationSBX:
								_x &= _a;
								uint16_t difference = _x - _operand;
								_x = (uint8_t)difference;
								_negativeResult = _zeroResult = _x;
								_carryFlag = ((difference >> 8)&1)^1;
							continue;
						}

						if(_ready_line_is_enabled && isReadOperation(nextBusOperation)) {
							_ready_is_active = true;
							break;
						}
						bus_access();
					}
				}
			}

			_cycles_left_to_run = number_of_cycles;
			_scheduleProgramsReadPointer = scheduleProgramsReadPointer;
			_scheduleProgramProgramCounter = scheduleProgramProgramCounter;
			_nextAddress = nextAddress;
			_nextBusOperation = nextBusOperation;
			_busAddress = busAddress;
			_busValue = busValue;

			static_cast<T *>(this)->synchronise();
		}

		/*!
			Called to announce the end of a run_for_cycles period, allowing deferred work to take place.

			Users of the 6502 template may override this.
		*/
		void synchronise() {}

		/*!
			Gets the value of a register.

			@see set_value_of_register

			@param r The register to set.
			@returns The value of the register. 8-bit registers will be returned as unsigned.
		*/
		uint16_t get_value_of_register(Register r)
		{
			switch (r) {
				case Register::ProgramCounter:			return _pc.full;
				case Register::LastOperationAddress:	return _lastOperationPC.full;
				case Register::StackPointer:			return _s;
				case Register::Flags:					return get_flags();
				case Register::A:						return _a;
				case Register::X:						return _x;
				case Register::Y:						return _y;
				case Register::S:						return _s;
				default: return 0;
			}
		}

		/*!
			Sets the value of a register.

			@see get_value_of_register

			@param r The register to set.
			@param value The value to set. If the register is only 8 bit, the value will be truncated.
		*/
		void set_value_of_register(Register r, uint16_t value)
		{
			switch (r) {
				case Register::ProgramCounter:	_pc.full = value;			break;
				case Register::StackPointer:	_s = (uint8_t)value;		break;
				case Register::Flags:			set_flags((uint8_t)value);	break;
				case Register::A:				_a = (uint8_t)value;		break;
				case Register::X:				_x = (uint8_t)value;		break;
				case Register::Y:				_y = (uint8_t)value;		break;
				case Register::S:				_s = (uint8_t)value;		break;
				default: break;
			}
		}

		/*!
			Interrupts current execution flow to perform an RTS and, if the 6502 is currently jammed,
			to unjam it.
		*/
		void return_from_subroutine()
		{
			_s++;
			static_cast<T *>(this)->perform_bus_operation(CPU6502::BusOperation::Read, 0x100 | _s, &_pc.bytes.low); _s++;
			static_cast<T *>(this)->perform_bus_operation(CPU6502::BusOperation::Read, 0x100 | _s, &_pc.bytes.high);
			_pc.full++;

			if(_is_jammed) {
				_scheduledPrograms[0] = _scheduledPrograms[1] = _scheduledPrograms[2] = _scheduledPrograms[3] = nullptr;
			}
		}

		/*!
			Sets the current level of the RDY line.

			@param active @c true if the line is logically active; @c false otherwise.
		*/
		inline void set_ready_line(bool active)
		{
			if(active) {
				_ready_line_is_enabled = true;
			} else {
				_ready_line_is_enabled = false;
				_ready_is_active = false;
			}
		}

		/*!
			Sets the current level of the RST line.

			@param active @c true if the line is logically active; @c false otherwise.
		*/
		inline void set_reset_line(bool active)
		{
			_interrupt_requests = (_interrupt_requests & ~InterruptRequestFlags::Reset) | (active ? InterruptRequestFlags::Reset : 0);
		}

		/*!
			Gets whether the 6502 would reset at the next opportunity.

			@returns @c true if the line is logically active; @c false otherwise.
		*/
		inline bool get_is_resetting()
		{
			return !!(_interrupt_requests & (InterruptRequestFlags::Reset | InterruptRequestFlags::PowerOn));
		}

		/*!
			This emulation automatically sets itself up in power-on state at creation, which has the effect of triggering a
			reset at the first opportunity. Use @c set_power_on to disable that behaviour.
		*/
		inline void set_power_on(bool active)
		{
			_interrupt_requests = (_interrupt_requests & ~InterruptRequestFlags::PowerOn) | (active ? InterruptRequestFlags::PowerOn : 0);
		}

		/*!
			Sets the current level of the IRQ line.

			@param active @c true if the line is logically active; @c false otherwise.
		*/
		inline void set_irq_line(bool active)
		{
			_irq_line = active ? Flag::Interrupt : 0;
		}

		/*!
			Sets the current level of the set overflow line.

			@param active @c true if the line is logically active; @c false otherwise.
		*/
		inline void set_overflow_line(bool active)
		{
			// a leading edge will set the overflow flag
			if(active && !_set_overflow_line_is_enabled)
				_overflowFlag = Flag::Overflow;
			_set_overflow_line_is_enabled = active;
		}

		/*!
			Sets the current level of the NMI line.

			@param active `true` if the line is logically active; `false` otherwise.
		*/
		inline void set_nmi_line(bool active)
		{
			// NMI is edge triggered, not level
			if(active && !_nmi_line_is_enabled)
				_interrupt_requests |= InterruptRequestFlags::NMI;
			_nmi_line_is_enabled = active;
		}

		/*!
			Queries whether the 6502 is now 'jammed'; i.e. has entered an invalid state
			such that it will not of itself perform any more meaningful processing.

			@returns @c true if the 6502 is jammed; @c false otherwise.
		*/
		inline bool is_jammed()
		{
			return _is_jammed;
		}

		/*!
			Installs a jam handler. Jam handlers are notified if a running 6502 jams.

			@param handler The class instance that will be this 6502's jam handler from now on.
		*/
		inline void set_jam_handler(JamHandler *handler)
		{
			_jam_handler = handler;
		}
};

}

#endif /* CPU6502_cpp */
