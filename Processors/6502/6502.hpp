//
//  6502.hpp
//  CLK
//
//  Created by Thomas Harte on 09/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#ifndef MOS6502_cpp
#define MOS6502_cpp

#include <cstdio>
#include <cstdint>

#include "../RegisterSizes.hpp"
#include "../../ClockReceiver/ClockReceiver.hpp"

namespace CPU {
namespace MOS6502 {

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
#define isReadOperation(v)	(v == CPU::MOS6502::BusOperation::Read || v == CPU::MOS6502::BusOperation::ReadOpcode)

/*!
	An opcode that is guaranteed to cause the CPU to jam.
*/
extern const uint8_t JamOpcode;

class ProcessorBase {
	protected:
		/*
			This emulation functions by decomposing instructions into micro programs, consisting of the micro operations
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

		static const MicroOp operations[256][10];
};

/*!
	@abstact An abstract base class for emulation of a 6502 processor via the curiously recurring template pattern/f-bounded polymorphism.

	@discussion Subclasses should implement @c perform_bus_operation(BusOperation operation, uint16_t address, uint8_t *value) in
	order to provide the bus on which the 6502 operates and @c flush(), which is called upon completion of a continuous run
	of cycles to allow a subclass to bring any on-demand activities up to date.

	Additional functionality can be provided by the host machine by providing a jam handler and inserting jam opcodes where appropriate;
	that will cause call outs when the program counter reaches those addresses. @c return_from_subroutine can be used to exit from a
	jammed state.
*/
template <class T> class Processor: public ProcessorBase {
	private:
		const MicroOp *scheduled_program_counter_;

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
			Temporary storage allowing a common dispatch point for calling perform_bus_operation;
			possibly deferring is no longer of value.
		*/
		BusOperation next_bus_operation_;
		uint16_t bus_address_;
		uint8_t *bus_value_;

		/*!
			Gets the flags register.

			@see set_flags

			@returns The current value of the flags register.
		*/
		uint8_t get_flags() {
			return carry_flag_ | overflow_flag_ | (inverse_interrupt_flag_ ^ Flag::Interrupt) | (negative_result_ & 0x80) | (zero_result_ ? 0 : Flag::Zero) | Flag::Always | decimal_flag_;
		}

		/*!
			Sets the flags register.

			@see set_flags

			@param flags The new value of the flags register.
		*/
		void set_flags(uint8_t flags) {
			carry_flag_				= flags		& Flag::Carry;
			negative_result_		= flags		& Flag::Sign;
			zero_result_			= (~flags)	& Flag::Zero;
			overflow_flag_			= flags		& Flag::Overflow;
			inverse_interrupt_flag_	= (~flags)	& Flag::Interrupt;
			decimal_flag_			= flags		& Flag::Decimal;
		}

		bool is_jammed_;
		Cycles cycles_left_to_run_;

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
				is_jammed_(false),
				ready_line_is_enabled_(false),
				ready_is_active_(false),
				inverse_interrupt_flag_(0),
				irq_request_history_(0),
				s_(0),
				next_bus_operation_(BusOperation::None),
				interrupt_requests_(InterruptRequestFlags::PowerOn),
				irq_line_(0),
				nmi_line_is_enabled_(false),
				set_overflow_line_is_enabled_(false),
				scheduled_program_counter_(nullptr) {
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
			If it is a read operation then @c value will be seeded with the value 0xff.

			@param cycles The number of cycles to run the 6502 for.
		*/
		void run_for(const Cycles &cycles) {
			static const MicroOp doBranch[] = {
				CycleReadFromPC,
				CycleAddSignedOperandToPC,
				OperationMoveToNextProgram
			};
			static uint8_t throwaway_target;
			static const MicroOp fetch_decode_execute[] = {
				CycleFetchOperation,
				CycleFetchOperand,
				OperationDecodeOperation
			};

			// These plus program below act to give the compiler permission to update these values
			// without touching the class storage (i.e. it explicitly says they need be completely up
			// to date in this stack frame only); which saves some complicated addressing
			RegisterPair nextAddress = next_address_;
			BusOperation nextBusOperation = next_bus_operation_;
			uint16_t busAddress = bus_address_;
			uint8_t *busValue = bus_value_;

#define checkSchedule(op) \
	if(!scheduled_program_counter_) {\
		if(interrupt_requests_) {\
			if(interrupt_requests_ & (InterruptRequestFlags::Reset | InterruptRequestFlags::PowerOn)) {\
				interrupt_requests_ &= ~InterruptRequestFlags::PowerOn;\
				scheduled_program_counter_ = get_reset_program();\
			} else if(interrupt_requests_ & InterruptRequestFlags::NMI) {\
				interrupt_requests_ &= ~InterruptRequestFlags::NMI;\
				scheduled_program_counter_ = get_nmi_program();\
			} else if(interrupt_requests_ & InterruptRequestFlags::IRQ) {\
				scheduled_program_counter_ = get_irq_program();\
			} \
		} else {\
			scheduled_program_counter_ = fetch_decode_execute;\
		}\
		op;\
	}

#define bus_access() \
	interrupt_requests_ = (interrupt_requests_ & ~InterruptRequestFlags::IRQ) | irq_request_history_;	\
	irq_request_history_ = irq_line_ & inverse_interrupt_flag_;	\
	number_of_cycles -= static_cast<T *>(this)->perform_bus_operation(nextBusOperation, busAddress, busValue);	\
	nextBusOperation = BusOperation::None;	\
	if(number_of_cycles <= Cycles(0)) break;

			checkSchedule();
			Cycles number_of_cycles = cycles + cycles_left_to_run_;

			while(number_of_cycles > Cycles(0)) {

				while (ready_is_active_ && number_of_cycles > Cycles(0)) {
					number_of_cycles -= static_cast<T *>(this)->perform_bus_operation(BusOperation::Ready, busAddress, busValue);
				}

				if(!ready_is_active_) {
					if(nextBusOperation != BusOperation::None) {
						bus_access();
					}

					while(1) {

						const MicroOp cycle = *scheduled_program_counter_;
						scheduled_program_counter_++;

#define read_op(val, addr)		nextBusOperation = BusOperation::ReadOpcode;	busAddress = addr;		busValue = &val;				val = 0xff
#define read_mem(val, addr)		nextBusOperation = BusOperation::Read;			busAddress = addr;		busValue = &val;				val	= 0xff
#define throwaway_read(addr)	nextBusOperation = BusOperation::Read;			busAddress = addr;		busValue = &throwaway_target;	throwaway_target = 0xff
#define write_mem(val, addr)	nextBusOperation = BusOperation::Write;			busAddress = addr;		busValue = &val

						switch(cycle) {

#pragma mark - Fetch/Decode

							case CycleFetchOperation: {
								last_operation_pc_ = pc_;
//								printf("%04x\n", pc_.full);
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
								scheduled_program_counter_ = operations[operation_];
							continue;

							case OperationMoveToNextProgram:
								scheduled_program_counter_ = nullptr;
								checkSchedule();
							continue;

#define push(v) {\
			uint16_t targetAddress = s_ | 0x100; s_--;\
			write_mem(v, targetAddress);\
		}

							case CycleIncPCPushPCH:				pc_.full++;														// deliberate fallthrough
							case CyclePushPCH:					push(pc_.bytes.high);											break;
							case CyclePushPCL:					push(pc_.bytes.low);											break;
							case CyclePushOperand:				push(operand_);													break;
							case CyclePushA:					push(a_);														break;
							case CycleNoWritePush: {
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
								scheduled_program_counter_ = operations[CPU::MOS6502::JamOpcode];
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

#define BRA(condition)	pc_.full++; if(condition) scheduled_program_counter_ = doBranch

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
			next_address_ = nextAddress;
			next_bus_operation_ = nextBusOperation;
			bus_address_ = busAddress;
			bus_value_ = busValue;

			static_cast<T *>(this)->flush();
		}

		/*!
			Called to announce the end of a run_for period, allowing deferred work to take place.

			Users of the 6502 template may override this.
		*/
		void flush() {}

		/*!
			Gets the value of a register.

			@see set_value_of_register

			@param r The register to set.
			@returns The value of the register. 8-bit registers will be returned as unsigned.
		*/
		uint16_t get_value_of_register(Register r) {
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
		void set_value_of_register(Register r, uint16_t value) {
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
		void return_from_subroutine() {
			s_++;
			static_cast<T *>(this)->perform_bus_operation(MOS6502::BusOperation::Read, 0x100 | s_, &pc_.bytes.low); s_++;
			static_cast<T *>(this)->perform_bus_operation(MOS6502::BusOperation::Read, 0x100 | s_, &pc_.bytes.high);
			pc_.full++;

			if(is_jammed_) {
				scheduled_program_counter_ = nullptr;
			}
		}

		/*!
			Sets the current level of the RDY line.

			@param active @c true if the line is logically active; @c false otherwise.
		*/
		inline void set_ready_line(bool active) {
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
		inline void set_reset_line(bool active) {
			interrupt_requests_ = (interrupt_requests_ & ~InterruptRequestFlags::Reset) | (active ? InterruptRequestFlags::Reset : 0);
		}

		/*!
			Gets whether the 6502 would reset at the next opportunity.

			@returns @c true if the line is logically active; @c false otherwise.
		*/
		inline bool get_is_resetting() {
			return !!(interrupt_requests_ & (InterruptRequestFlags::Reset | InterruptRequestFlags::PowerOn));
		}

		/*!
			This emulation automatically sets itself up in power-on state at creation, which has the effect of triggering a
			reset at the first opportunity. Use @c set_power_on to disable that behaviour.
		*/
		inline void set_power_on(bool active) {
			interrupt_requests_ = (interrupt_requests_ & ~InterruptRequestFlags::PowerOn) | (active ? InterruptRequestFlags::PowerOn : 0);
		}

		/*!
			Sets the current level of the IRQ line.

			@param active @c true if the line is logically active; @c false otherwise.
		*/
		inline void set_irq_line(bool active) {
			irq_line_ = active ? Flag::Interrupt : 0;
		}

		/*!
			Sets the current level of the set overflow line.

			@param active @c true if the line is logically active; @c false otherwise.
		*/
		inline void set_overflow_line(bool active) {
			// a leading edge will set the overflow flag
			if(active && !set_overflow_line_is_enabled_)
				overflow_flag_ = Flag::Overflow;
			set_overflow_line_is_enabled_ = active;
		}

		/*!
			Sets the current level of the NMI line.

			@param active `true` if the line is logically active; `false` otherwise.
		*/
		inline void set_nmi_line(bool active) {
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
		inline bool is_jammed() {
			return is_jammed_;
		}
};

}
}

#endif /* MOS6502_cpp */
