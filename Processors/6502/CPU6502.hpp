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
		RegisterPair pc_, last_operation_pc_;
		uint8_t a_, x_, y_, s_;
		uint8_t carry_flag_, negative_result_, zero_result_, decimal_flag_, overflow_flag_, inverse_interrupt_flag_;

		/*
			Temporary state for the micro programs.
		*/
		uint8_t operation_, operand_;
		RegisterPair address_, next_address_;

		/*
			Up to four programs can be scheduled; each will be carried out in turn. This
			storage maintains pointers to the scheduled list of programs.

			Programs should be terminated by an OperationMoveToNextProgram, causing this
			queue to take that step.
		*/
		const MicroOp *scheduled_programs_[4];
		unsigned int schedule_programs_write_pointer_, schedule_programs_read_pointer_, schedule_program_program_counter_;

		/*
			Temporary storage allowing a common dispatch point for calling perform_bus_operation;
			possibly deferring is no longer of value.
		*/
		BusOperation next_bus_operation_;
		uint16_t bus_address_;
		uint8_t *bus_value_;

		/*!
			Schedules a new program, adding it to the end of the queue. Programs should be
			terminated with a OperationMoveToNextProgram. No attempt to copy the program
			is made; a non-owning reference is kept.

			@param program The program to schedule.
		*/
		inline void schedule_program(const MicroOp *program)
		{
			scheduled_programs_[schedule_programs_write_pointer_] = program;
			schedule_programs_write_pointer_ = (schedule_programs_write_pointer_+1)&3;
		}

		/*!
			Gets the flags register.

			@see set_flags

			@returns The current value of the flags register.
		*/
		uint8_t get_flags()
		{
			return carry_flag_ | overflow_flag_ | (inverse_interrupt_flag_ ^ Flag::Interrupt) | (negative_result_ & 0x80) | (zero_result_ ? 0 : Flag::Zero) | Flag::Always | decimal_flag_;
		}

		/*!
			Sets the flags register.

			@see set_flags

			@param flags The new value of the flags register.
		*/
		void set_flags(uint8_t flags)
		{
			carry_flag_				= flags		& Flag::Carry;
			negative_result_		= flags		& Flag::Sign;
			zero_result_			= (~flags)	& Flag::Zero;
			overflow_flag_			= flags		& Flag::Overflow;
			inverse_interrupt_flag_	= (~flags)	& Flag::Interrupt;
			decimal_flag_			= flags		& Flag::Decimal;
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

		bool is_jammed_;
		JamHandler *jam_handler_;

		int cycles_left_to_run_;

		enum InterruptRequestFlags: uint8_t {
			Reset		= 0x80,
			IRQ			= Flag::Interrupt,
			NMI			= 0x20,

			PowerOn		= 0x10,
		};
		uint8_t interrupt_requests_;

		bool ready_is_active_;
		bool ready_line_is_enabled_;

		uint8_t irq_line_, irq_request_history_;
		bool nmi_line_is_enabled_, set_overflow_line_is_enabled_;

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
			schedule_programs_read_pointer_(0),
			schedule_programs_write_pointer_(0),
			is_jammed_(false),
			jam_handler_(nullptr),
			cycles_left_to_run_(0),
			ready_line_is_enabled_(false),
			ready_is_active_(false),
			scheduled_programs_{nullptr, nullptr, nullptr, nullptr},
			inverse_interrupt_flag_(0),
			s_(0),
			next_bus_operation_(BusOperation::None),
			interrupt_requests_(InterruptRequestFlags::PowerOn),
			irq_line_(0),
			nmi_line_is_enabled_(false),
			set_overflow_line_is_enabled_(false)
		{
			// only the interrupt flag is defined upon reset but get_flags isn't going to
			// mask the other flags so we need to do that, at least
			carry_flag_ &= Flag::Carry;
			decimal_flag_ &= Flag::Decimal;
			overflow_flag_ &= Flag::Overflow;
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
			unsigned int scheduleProgramsReadPointer = schedule_programs_read_pointer_;
			unsigned int scheduleProgramProgramCounter = schedule_program_program_counter_;
			RegisterPair nextAddress = next_address_;
			BusOperation nextBusOperation = next_bus_operation_;
			uint16_t busAddress = bus_address_;
			uint8_t *busValue = bus_value_;

#define checkSchedule(op) \
	if(!scheduled_programs_[scheduleProgramsReadPointer]) {\
		scheduleProgramsReadPointer = schedule_programs_write_pointer_ = scheduleProgramProgramCounter = 0;\
		if(interrupt_requests_) {\
			if(interrupt_requests_ & (InterruptRequestFlags::Reset | InterruptRequestFlags::PowerOn)) {\
				interrupt_requests_ &= ~InterruptRequestFlags::PowerOn;\
				schedule_program(get_reset_program());\
			} else if(interrupt_requests_ & InterruptRequestFlags::NMI) {\
				interrupt_requests_ &= ~InterruptRequestFlags::NMI;\
				schedule_program(get_nmi_program());\
			} else if(interrupt_requests_ & InterruptRequestFlags::IRQ) {\
				schedule_program(get_irq_program());\
			} \
		} else {\
			schedule_program(fetch_decode_execute);\
		}\
		op;\
	}

#define bus_access() \
	interrupt_requests_ = (interrupt_requests_ & ~InterruptRequestFlags::IRQ) | irq_request_history_;	\
	irq_request_history_ = irq_line_ & inverse_interrupt_flag_;	\
	number_of_cycles -= static_cast<T *>(this)->perform_bus_operation(nextBusOperation, busAddress, busValue);	\
	nextBusOperation = BusOperation::None;	\
	if(number_of_cycles <= 0) break;

			checkSchedule();
			number_of_cycles += cycles_left_to_run_;
			const MicroOp *program = scheduled_programs_[scheduleProgramsReadPointer];

			while(number_of_cycles > 0) {

				while (ready_is_active_ && number_of_cycles > 0) {
					number_of_cycles -= static_cast<T *>(this)->perform_bus_operation(BusOperation::Ready, busAddress, busValue);
				}

				if(!ready_is_active_)
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
								last_operation_pc_ = pc_;
//								printf("%04x	x:%02x\n", pc_.full, x_);
								pc_.full++;
								read_op(operation_, last_operation_pc_.full);

//								static int last_cycles_left_to_run = 0;
//								static bool printed_map[256] = {false};
//								if(!printed_map[operation_]) {
//									printed_map[operation_] = true;
//									if(last_cycles_left_to_run > cycles_left_to_run_)
//										printf("%02x %d\n", operation_, last_cycles_left_to_run - cycles_left_to_run_);
//									else
//										printf("%02x\n", operation_);
//								}
//								last_cycles_left_to_run = cycles_left_to_run_;
							} break;

							case CycleFetchOperand:
								read_mem(operand_, pc_.full);
							break;

							case OperationDecodeOperation:
//								printf("d %02x\n", operation_);
								decode_operation(operation_);
							continue;

							case OperationMoveToNextProgram:
								scheduled_programs_[scheduleProgramsReadPointer] = NULL;
								scheduleProgramsReadPointer = (scheduleProgramsReadPointer+1)&3;
								scheduleProgramProgramCounter = 0;
								checkSchedule();
								program = scheduled_programs_[scheduleProgramsReadPointer];
							continue;

#define push(v) \
		{\
			uint16_t targetAddress = s_ | 0x100; s_--;\
			write_mem(v, targetAddress);\
		}

							case CycleIncPCPushPCH:				pc_.full++;														// deliberate fallthrough
							case CyclePushPCH:					push(pc_.bytes.high);											break;
							case CyclePushPCL:					push(pc_.bytes.low);											break;
							case CyclePushOperand:				push(operand_);													break;
							case CyclePushA:					push(a_);														break;
							case CycleNoWritePush:
							{
								uint16_t targetAddress = s_ | 0x100; s_--;
								read_mem(operand_, targetAddress);
							}
							break;

#undef push

							case CycleReadFromS:				throwaway_read(s_ | 0x100);										break;
							case CycleReadFromPC:				throwaway_read(pc_.full);										break;

							case OperationBRKPickVector:
								// NMI can usurp BRK-vector operations
								nextAddress.full = (interrupt_requests_ & InterruptRequestFlags::NMI) ? 0xfffa : 0xfffe;
								interrupt_requests_ &= ~InterruptRequestFlags::NMI;	// TODO: this probably doesn't happen now?
							continue;
							case OperationNMIPickVector:		nextAddress.full = 0xfffa;											continue;
							case OperationRSTPickVector:		nextAddress.full = 0xfffc;											continue;
							case CycleReadVectorLow:			read_mem(pc_.bytes.low, nextAddress.full);							break;
							case CycleReadVectorHigh:			read_mem(pc_.bytes.high, nextAddress.full+1);						break;
							case OperationSetI:					inverse_interrupt_flag_ = 0;										continue;

							case CyclePullPCL:					s_++; read_mem(pc_.bytes.low, s_ | 0x100);							break;
							case CyclePullPCH:					s_++; read_mem(pc_.bytes.high, s_ | 0x100);							break;
							case CyclePullA:					s_++; read_mem(a_, s_ | 0x100);										break;
							case CyclePullOperand:				s_++; read_mem(operand_, s_ | 0x100);								break;
							case OperationSetFlagsFromOperand:	set_flags(operand_);												continue;
							case OperationSetOperandFromFlagsWithBRKSet: operand_ = get_flags() | Flag::Break;						continue;
							case OperationSetOperandFromFlags:  operand_ = get_flags();												continue;
							case OperationSetFlagsFromA:		zero_result_ = negative_result_ = a_;								continue;

							case CycleIncrementPCAndReadStack:	pc_.full++; throwaway_read(s_ | 0x100);								break;
							case CycleReadPCLFromAddress:		read_mem(pc_.bytes.low, address_.full);								break;
							case CycleReadPCHFromAddress:		address_.bytes.low++; read_mem(pc_.bytes.high, address_.full);		break;

							case CycleReadAndIncrementPC: {
								uint16_t oldPC = pc_.full;
								pc_.full++;
								throwaway_read(oldPC);
							} break;

#pragma mark - JAM

							case CycleScheduleJam: {
								is_jammed_ = true;
								static const MicroOp jam[] = JAM;
								schedule_program(jam);

								if(jam_handler_) {
									jam_handler_->processor_did_jam(this, pc_.full - 1);
									checkSchedule(is_jammed_ = false; program = scheduled_programs_[scheduleProgramsReadPointer]);
								}
							} continue;

#pragma mark - Bitwise

							case OperationORA:	a_ |= operand_;	negative_result_ = zero_result_ = a_;		continue;
							case OperationAND:	a_ &= operand_;	negative_result_ = zero_result_ = a_;		continue;
							case OperationEOR:	a_ ^= operand_;	negative_result_ = zero_result_ = a_;		continue;

#pragma mark - Load and Store

							case OperationLDA:	a_ = negative_result_ = zero_result_ = operand_;			continue;
							case OperationLDX:	x_ = negative_result_ = zero_result_ = operand_;			continue;
							case OperationLDY:	y_ = negative_result_ = zero_result_ = operand_;			continue;
							case OperationLAX:	a_ = x_ = negative_result_ = zero_result_ = operand_;		continue;

							case OperationSTA:	operand_ = a_;											continue;
							case OperationSTX:	operand_ = x_;											continue;
							case OperationSTY:	operand_ = y_;											continue;
							case OperationSAX:	operand_ = a_ & x_;										continue;
							case OperationSHA:	operand_ = a_ & x_ & (address_.bytes.high+1);			continue;
							case OperationSHX:	operand_ = x_ & (address_.bytes.high+1);				continue;
							case OperationSHY:	operand_ = y_ & (address_.bytes.high+1);				continue;
							case OperationSHS:	s_ = a_ & x_; operand_ = s_ & (address_.bytes.high+1);	continue;

							case OperationLXA:
								a_ = x_ = (a_ | 0xee) & operand_;
								negative_result_ = zero_result_ = a_;
							continue;

#pragma mark - Compare

							case OperationCMP: {
								const uint16_t temp16 = a_ - operand_;
								negative_result_ = zero_result_ = (uint8_t)temp16;
								carry_flag_ = ((~temp16) >> 8)&1;
							} continue;
							case OperationCPX: {
								const uint16_t temp16 = x_ - operand_;
								negative_result_ = zero_result_ = (uint8_t)temp16;
								carry_flag_ = ((~temp16) >> 8)&1;
							} continue;
							case OperationCPY: {
								const uint16_t temp16 = y_ - operand_;
								negative_result_ = zero_result_ = (uint8_t)temp16;
								carry_flag_ = ((~temp16) >> 8)&1;
							} continue;

#pragma mark - BIT

							case OperationBIT:
								zero_result_ = operand_ & a_;
								negative_result_ = operand_;
								overflow_flag_ = operand_&Flag::Overflow;
							continue;

#pragma mark ADC/SBC (and INS)

							case OperationINS:
								operand_++;			// deliberate fallthrough
							case OperationSBC:
								if(decimal_flag_) {
									const uint16_t notCarry = carry_flag_ ^ 0x1;
									const uint16_t decimalResult = (uint16_t)a_ - (uint16_t)operand_ - notCarry;
									uint16_t temp16;

									temp16 = (a_&0xf) - (operand_&0xf) - notCarry;
									if(temp16 > 0xf) temp16 -= 0x6;
									temp16 = (temp16&0x0f) | ((temp16 > 0x0f) ? 0xfff0 : 0x00);
									temp16 += (a_&0xf0) - (operand_&0xf0);

									overflow_flag_ = ( ( (decimalResult^a_)&(~decimalResult^operand_) )&0x80) >> 1;
									negative_result_ = (uint8_t)temp16;
									zero_result_ = (uint8_t)decimalResult;

									if(temp16 > 0xff) temp16 -= 0x60;

									carry_flag_ = (temp16 > 0xff) ? 0 : Flag::Carry;
									a_ = (uint8_t)temp16;
									continue;
								} else {
									operand_ = ~operand_;
								}

							// deliberate fallthrough
							case OperationADC:
								if(decimal_flag_) {
									const uint16_t decimalResult = (uint16_t)a_ + (uint16_t)operand_ + (uint16_t)carry_flag_;

									uint8_t low_nibble = (a_ & 0xf) + (operand_ & 0xf) + carry_flag_;
									if(low_nibble >= 0xa) low_nibble = ((low_nibble + 0x6) & 0xf) + 0x10;
									uint16_t result = (uint16_t)(a_ & 0xf0) + (uint16_t)(operand_ & 0xf0) + (uint16_t)low_nibble;
									negative_result_ = (uint8_t)result;
									overflow_flag_ = (( (result^a_)&(result^operand_) )&0x80) >> 1;
									if(result >= 0xa0) result += 0x60;

									carry_flag_ = (result >> 8) ? 1 : 0;
									a_ = (uint8_t)result;
									zero_result_ = (uint8_t)decimalResult;
								} else {
									const uint16_t result = (uint16_t)a_ + (uint16_t)operand_ + (uint16_t)carry_flag_;
									overflow_flag_ = (( (result^a_)&(result^operand_) )&0x80) >> 1;
									negative_result_ = zero_result_ = a_ = (uint8_t)result;
									carry_flag_ = (result >> 8)&1;
								}

								// fix up in case this was INS
								if(cycle == OperationINS) operand_ = ~operand_;
							continue;

#pragma mark - Shifts and Rolls

							case OperationASL:
								carry_flag_ = operand_ >> 7;
								operand_ <<= 1;
								negative_result_ = zero_result_ = operand_;
							continue;

							case OperationASO:
								carry_flag_ = operand_ >> 7;
								operand_ <<= 1;
								a_ |= operand_;
								negative_result_ = zero_result_ = a_;
							continue;

							case OperationROL: {
								const uint8_t temp8 = (uint8_t)((operand_ << 1) | carry_flag_);
								carry_flag_ = operand_ >> 7;
								operand_ = negative_result_ = zero_result_ = temp8;
							} continue;

							case OperationRLA: {
								const uint8_t temp8 = (uint8_t)((operand_ << 1) | carry_flag_);
								carry_flag_ = operand_ >> 7;
								operand_ = temp8;
								a_ &= operand_;
								negative_result_ = zero_result_ = a_;
							} continue;

							case OperationLSR:
								carry_flag_ = operand_ & 1;
								operand_ >>= 1;
								negative_result_ = zero_result_ = operand_;
							continue;

							case OperationLSE:
								carry_flag_ = operand_ & 1;
								operand_ >>= 1;
								a_ ^= operand_;
								negative_result_ = zero_result_ = a_;
							continue;

							case OperationASR:
								a_ &= operand_;
								carry_flag_ = a_ & 1;
								a_ >>= 1;
								negative_result_ = zero_result_ = a_;
							continue;

							case OperationROR: {
								const uint8_t temp8 = (uint8_t)((operand_ >> 1) | (carry_flag_ << 7));
								carry_flag_ = operand_ & 1;
								operand_ = negative_result_ = zero_result_ = temp8;
							} continue;

							case OperationRRA: {
								const uint8_t temp8 = (uint8_t)((operand_ >> 1) | (carry_flag_ << 7));
								carry_flag_ = operand_ & 1;
								operand_ = temp8;
							} continue;

							case OperationDecrementOperand: operand_--; continue;
							case OperationIncrementOperand: operand_++; continue;

							case OperationCLC: carry_flag_ = 0;								continue;
							case OperationCLI: inverse_interrupt_flag_ = Flag::Interrupt;	continue;
							case OperationCLV: overflow_flag_ = 0;							continue;
							case OperationCLD: decimal_flag_ = 0;							continue;

							case OperationSEC: carry_flag_ = Flag::Carry;		continue;
							case OperationSEI: inverse_interrupt_flag_ = 0;		continue;
							case OperationSED: decimal_flag_ = Flag::Decimal;	continue;

							case OperationINC: operand_++; negative_result_ = zero_result_ = operand_; continue;
							case OperationDEC: operand_--; negative_result_ = zero_result_ = operand_; continue;
							case OperationINX: x_++; negative_result_ = zero_result_ = x_; continue;
							case OperationDEX: x_--; negative_result_ = zero_result_ = x_; continue;
							case OperationINY: y_++; negative_result_ = zero_result_ = y_; continue;
							case OperationDEY: y_--; negative_result_ = zero_result_ = y_; continue;

							case OperationANE:
								a_ = (a_ | 0xee) & operand_ & x_;
								negative_result_ = zero_result_ = a_;
							continue;

							case OperationANC:
								a_ &= operand_;
								negative_result_ = zero_result_ = a_;
								carry_flag_ = a_ >> 7;
							continue;

							case OperationLAS:
								a_ = x_ = s_ = s_ & operand_;
								negative_result_ = zero_result_ = a_;
							continue;

#pragma mark - Addressing Mode Work

							case CycleAddXToAddressLow:
								nextAddress.full = address_.full + x_;
								address_.bytes.low = nextAddress.bytes.low;
								if(address_.bytes.high != nextAddress.bytes.high) {
									throwaway_read(address_.full);
									break;
								}
							continue;
							case CycleAddXToAddressLowRead:
								nextAddress.full = address_.full + x_;
								address_.bytes.low = nextAddress.bytes.low;
								throwaway_read(address_.full);
							break;
							case CycleAddYToAddressLow:
								nextAddress.full = address_.full + y_;
								address_.bytes.low = nextAddress.bytes.low;
								if(address_.bytes.high != nextAddress.bytes.high) {
									throwaway_read(address_.full);
									break;
								}
							continue;
							case CycleAddYToAddressLowRead:
								nextAddress.full = address_.full + y_;
								address_.bytes.low = nextAddress.bytes.low;
								throwaway_read(address_.full);
							break;
							case OperationCorrectAddressHigh:
								address_.full = nextAddress.full;
							continue;
							case CycleIncrementPCFetchAddressLowFromOperand:
								pc_.full++;
								read_mem(address_.bytes.low, operand_);
							break;
							case CycleAddXToOperandFetchAddressLow:
								operand_ += x_;
								read_mem(address_.bytes.low, operand_);
							break;
							case CycleIncrementOperandFetchAddressHigh:
								operand_++;
								read_mem(address_.bytes.high, operand_);
							break;
							case CycleIncrementPCReadPCHLoadPCL:	// deliberate fallthrough
								pc_.full++;
							case CycleReadPCHLoadPCL: {
								uint16_t oldPC = pc_.full;
								pc_.bytes.low = operand_;
								read_mem(pc_.bytes.high, oldPC);
							} break;

							case CycleReadAddressHLoadAddressL:
								address_.bytes.low = operand_; pc_.full++;
								read_mem(address_.bytes.high, pc_.full);
							break;

							case CycleLoadAddressAbsolute: {
								uint16_t nextPC = pc_.full+1;
								pc_.full += 2;
								address_.bytes.low = operand_;
								read_mem(address_.bytes.high, nextPC);
							} break;

							case OperationLoadAddressZeroPage:
								pc_.full++;
								address_.full = operand_;
							continue;

							case CycleLoadAddessZeroX:
								pc_.full++;
								address_.full = (operand_ + x_)&0xff;
								throwaway_read(operand_);
							break;

							case CycleLoadAddessZeroY:
								pc_.full++;
								address_.full = (operand_ + y_)&0xff;
								throwaway_read(operand_);
							break;

							case OperationIncrementPC:			pc_.full++;						continue;
							case CycleFetchOperandFromAddress:	read_mem(operand_, address_.full);	break;
							case CycleWriteOperandToAddress:	write_mem(operand_, address_.full);	break;
							case OperationCopyOperandFromA:		operand_ = a_;					continue;
							case OperationCopyOperandToA:		a_ = operand_;					continue;

#pragma mark - Branching

#define BRA(condition)	pc_.full++; if(condition) schedule_program(doBranch)

							case OperationBPL: BRA(!(negative_result_&0x80));				continue;
							case OperationBMI: BRA(negative_result_&0x80);					continue;
							case OperationBVC: BRA(!overflow_flag_);						continue;
							case OperationBVS: BRA(overflow_flag_);							continue;
							case OperationBCC: BRA(!carry_flag_);							continue;
							case OperationBCS: BRA(carry_flag_);							continue;
							case OperationBNE: BRA(zero_result_);							continue;
							case OperationBEQ: BRA(!zero_result_);							continue;

							case CycleAddSignedOperandToPC:
								nextAddress.full = (uint16_t)(pc_.full + (int8_t)operand_);
								pc_.bytes.low = nextAddress.bytes.low;
								if(nextAddress.bytes.high != pc_.bytes.high) {
									uint16_t halfUpdatedPc = pc_.full;
									pc_.full = nextAddress.full;
									throwaway_read(halfUpdatedPc);
									break;
								}
							continue;

#undef BRA

#pragma mark - Transfers

							case OperationTXA: zero_result_ = negative_result_ = a_ = x_;	continue;
							case OperationTYA: zero_result_ = negative_result_ = a_ = y_;	continue;
							case OperationTXS: s_ = x_;										continue;
							case OperationTAY: zero_result_ = negative_result_ = y_ = a_;	continue;
							case OperationTAX: zero_result_ = negative_result_ = x_ = a_;	continue;
							case OperationTSX: zero_result_ = negative_result_ = x_ = s_;	continue;

							case OperationARR:
								if(decimal_flag_) {
									a_ &= operand_;
									uint8_t unshiftedA = a_;
									a_ = (uint8_t)((a_ >> 1) | (carry_flag_ << 7));
									zero_result_ = negative_result_ = a_;
									overflow_flag_ = (a_^(a_ << 1))&Flag::Overflow;

									if((unshiftedA&0xf) + (unshiftedA&0x1) > 5) a_ = ((a_ + 6)&0xf) | (a_ & 0xf0);

									carry_flag_ = ((unshiftedA&0xf0) + (unshiftedA&0x10) > 0x50) ? 1 : 0;
									if(carry_flag_) a_ += 0x60;
								} else {
									a_ &= operand_;
									a_ = (uint8_t)((a_ >> 1) | (carry_flag_ << 7));
									negative_result_ = zero_result_ = a_;
									carry_flag_ = (a_ >> 6)&1;
									overflow_flag_ = (a_^(a_ << 1))&Flag::Overflow;
								}
							continue;

							case OperationSBX:
								x_ &= a_;
								uint16_t difference = x_ - operand_;
								x_ = (uint8_t)difference;
								negative_result_ = zero_result_ = x_;
								carry_flag_ = ((difference >> 8)&1)^1;
							continue;
						}

						if(ready_line_is_enabled_ && isReadOperation(nextBusOperation)) {
							ready_is_active_ = true;
							break;
						}
						bus_access();
					}
				}
			}

			cycles_left_to_run_ = number_of_cycles;
			schedule_programs_read_pointer_ = scheduleProgramsReadPointer;
			schedule_program_program_counter_ = scheduleProgramProgramCounter;
			next_address_ = nextAddress;
			next_bus_operation_ = nextBusOperation;
			bus_address_ = busAddress;
			bus_value_ = busValue;

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
				case Register::ProgramCounter:			return pc_.full;
				case Register::LastOperationAddress:	return last_operation_pc_.full;
				case Register::StackPointer:			return s_;
				case Register::Flags:					return get_flags();
				case Register::A:						return a_;
				case Register::X:						return x_;
				case Register::Y:						return y_;
				case Register::S:						return s_;
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
				case Register::ProgramCounter:	pc_.full = value;			break;
				case Register::StackPointer:	s_ = (uint8_t)value;		break;
				case Register::Flags:			set_flags((uint8_t)value);	break;
				case Register::A:				a_ = (uint8_t)value;		break;
				case Register::X:				x_ = (uint8_t)value;		break;
				case Register::Y:				y_ = (uint8_t)value;		break;
				case Register::S:				s_ = (uint8_t)value;		break;
				default: break;
			}
		}

		/*!
			Interrupts current execution flow to perform an RTS and, if the 6502 is currently jammed,
			to unjam it.
		*/
		void return_from_subroutine()
		{
			s_++;
			static_cast<T *>(this)->perform_bus_operation(CPU6502::BusOperation::Read, 0x100 | s_, &pc_.bytes.low); s_++;
			static_cast<T *>(this)->perform_bus_operation(CPU6502::BusOperation::Read, 0x100 | s_, &pc_.bytes.high);
			pc_.full++;

			if(is_jammed_) {
				scheduled_programs_[0] = scheduled_programs_[1] = scheduled_programs_[2] = scheduled_programs_[3] = nullptr;
			}
		}

		/*!
			Sets the current level of the RDY line.

			@param active @c true if the line is logically active; @c false otherwise.
		*/
		inline void set_ready_line(bool active)
		{
			if(active) {
				ready_line_is_enabled_ = true;
			} else {
				ready_line_is_enabled_ = false;
				ready_is_active_ = false;
			}
		}

		/*!
			Sets the current level of the RST line.

			@param active @c true if the line is logically active; @c false otherwise.
		*/
		inline void set_reset_line(bool active)
		{
			interrupt_requests_ = (interrupt_requests_ & ~InterruptRequestFlags::Reset) | (active ? InterruptRequestFlags::Reset : 0);
		}

		/*!
			Gets whether the 6502 would reset at the next opportunity.

			@returns @c true if the line is logically active; @c false otherwise.
		*/
		inline bool get_is_resetting()
		{
			return !!(interrupt_requests_ & (InterruptRequestFlags::Reset | InterruptRequestFlags::PowerOn));
		}

		/*!
			This emulation automatically sets itself up in power-on state at creation, which has the effect of triggering a
			reset at the first opportunity. Use @c set_power_on to disable that behaviour.
		*/
		inline void set_power_on(bool active)
		{
			interrupt_requests_ = (interrupt_requests_ & ~InterruptRequestFlags::PowerOn) | (active ? InterruptRequestFlags::PowerOn : 0);
		}

		/*!
			Sets the current level of the IRQ line.

			@param active @c true if the line is logically active; @c false otherwise.
		*/
		inline void set_irq_line(bool active)
		{
			irq_line_ = active ? Flag::Interrupt : 0;
		}

		/*!
			Sets the current level of the set overflow line.

			@param active @c true if the line is logically active; @c false otherwise.
		*/
		inline void set_overflow_line(bool active)
		{
			// a leading edge will set the overflow flag
			if(active && !set_overflow_line_is_enabled_)
				overflow_flag_ = Flag::Overflow;
			set_overflow_line_is_enabled_ = active;
		}

		/*!
			Sets the current level of the NMI line.

			@param active `true` if the line is logically active; `false` otherwise.
		*/
		inline void set_nmi_line(bool active)
		{
			// NMI is edge triggered, not level
			if(active && !nmi_line_is_enabled_)
				interrupt_requests_ |= InterruptRequestFlags::NMI;
			nmi_line_is_enabled_ = active;
		}

		/*!
			Queries whether the 6502 is now 'jammed'; i.e. has entered an invalid state
			such that it will not of itself perform any more meaningful processing.

			@returns @c true if the 6502 is jammed; @c false otherwise.
		*/
		inline bool is_jammed()
		{
			return is_jammed_;
		}

		/*!
			Installs a jam handler. Jam handlers are notified if a running 6502 jams.

			@param handler The class instance that will be this 6502's jam handler from now on.
		*/
		inline void set_jam_handler(JamHandler *handler)
		{
			jam_handler_ = handler;
		}
};

}

#endif /* CPU6502_cpp */
