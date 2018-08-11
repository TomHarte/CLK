//
//  6502Storage.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/09/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef MOS6502Storage_h
#define MOS6502Storage_h

/*!
	A repository for all the internal state of a CPU::MOS6502::Processor; extracted into a separate base
	class in order to remove it from visibility within the main 6502.hpp.
*/
class ProcessorStorage {
	protected:
		ProcessorStorage(Personality);

		/*
			This emulation functions by decomposing instructions into micro programs, consisting of the micro operations
			as per the enum below. Each micro op takes at most one cycle. By convention, those called CycleX take a cycle
			to perform whereas those called OperationX occur for free (so, in effect, their cost is loaded onto the next cycle).
		*/
		enum MicroOp {
			CycleFetchOperation,						CycleFetchOperand,					OperationDecodeOperation,				CycleIncPCPushPCH,
			CyclePushPCH,								CyclePushPCL,						CyclePushA,								CyclePushOperand,
			CyclePushX,									CyclePushY,							OperationSetI,

			OperationBRKPickVector,						OperationNMIPickVector,				OperationRSTPickVector,
			CycleReadVectorLow,							CycleReadVectorHigh,

			CycleReadFromS,								CycleReadFromPC,
			CyclePullOperand,							CyclePullPCL,						CyclePullPCH,							CyclePullA,
			CyclePullX,									CyclePullY,
			CycleNoWritePush,
			CycleReadAndIncrementPC,					CycleIncrementPCAndReadStack,		CycleIncrementPCReadPCHLoadPCL,			CycleReadPCHLoadPCL,
			CycleReadAddressHLoadAddressL,

			CycleReadPCLFromAddress,					CycleReadPCHFromAddressLowInc,		CycleReadPCHFromAddressFixed,			CycleReadPCHFromAddressInc,

			CycleLoadAddressAbsolute,
			OperationLoadAddressZeroPage,				CycleLoadAddessZeroX,				CycleLoadAddessZeroY,					CycleAddXToAddressLow,
			CycleAddYToAddressLow,						CycleAddXToAddressLowRead,			OperationCorrectAddressHigh,			CycleAddYToAddressLowRead,
			OperationMoveToNextProgram,					OperationIncrementPC,
			CycleFetchOperandFromAddress,				CycleWriteOperandToAddress,			OperationCopyOperandFromA,				OperationCopyOperandToA,
			CycleIncrementPCFetchAddressLowFromOperand,	CycleAddXToOperandFetchAddressLow,	CycleIncrementOperandFetchAddressHigh,	OperationDecrementOperand,
			CycleFetchAddressLowFromOperand,
			OperationIncrementOperand,					OperationORA,						OperationAND,							OperationEOR,
			OperationINS,								OperationADC,						OperationSBC,							OperationLDA,
			OperationLDX,								OperationLDY,						OperationLAX,							OperationSTA,
			OperationSTX,								OperationSTY,						OperationSAX,							OperationSHA,
			OperationSHX,								OperationSHY,						OperationSHS,							OperationCMP,
			OperationCPX,								OperationCPY,						OperationBIT,							OperationASL,
			OperationASO,								OperationROL,						OperationRLA,							OperationLSR,
			OperationLSE,								OperationASR,						OperationROR,							OperationRRA,
			OperationCLC,								OperationCLI,						OperationCLV,							OperationCLD,
			OperationSEC,								OperationSEI,						OperationSED,

			OperationINC,								OperationDEC,						OperationINX,							OperationDEX,
			OperationINY,								OperationDEY,						OperationINA,							OperationDEA,

			OperationBPL,								OperationBMI,						OperationBVC,							OperationBVS,
			OperationBCC,								OperationBCS,						OperationBNE,							OperationBEQ,
			OperationBRA,								OperationBBRBBS,

			OperationTXA,								OperationTYA,						OperationTXS,							OperationTAY,
			OperationTAX,								OperationTSX,

			OperationARR,								OperationSBX,						OperationLXA,							OperationANE,
			OperationANC,								OperationLAS,

			CycleFetchFromHalfUpdatedPC,				CycleAddSignedOperandToPC,			OperationAddSignedOperandToPC16,

			OperationSetFlagsFromOperand,				OperationSetOperandFromFlagsWithBRKSet,
			OperationSetOperandFromFlags,
			OperationSetFlagsFromA,						OperationSetFlagsFromX,				OperationSetFlagsFromY,
			CycleScheduleJam
		};

		using InstructionList = MicroOp[10];
		InstructionList operations_[256];

		const MicroOp *scheduled_program_counter_ = nullptr;

		/*
			Storage for the 6502 registers; F is stored as individual flags.
		*/
		RegisterPair pc_, last_operation_pc_;
		uint8_t a_, x_, y_, s_ = 0;
		uint8_t carry_flag_, negative_result_, zero_result_, decimal_flag_, overflow_flag_, inverse_interrupt_flag_ = 0;

		/*
			Temporary state for the micro programs.
		*/
		uint8_t operation_, operand_;
		RegisterPair address_, next_address_;

		/*
			Temporary storage allowing a common dispatch point for calling perform_bus_operation;
			possibly deferring is no longer of value.
		*/
		BusOperation next_bus_operation_ = BusOperation::None;
		uint16_t bus_address_;
		uint8_t *bus_value_;

		/*!
			Gets the flags register.

			@see set_flags

			@returns The current value of the flags register.
		*/
		inline uint8_t get_flags();

		/*!
			Sets the flags register.

			@see set_flags

			@param flags The new value of the flags register.
		*/
		inline void set_flags(uint8_t flags);

		bool is_jammed_ = false;
		Cycles cycles_left_to_run_;

		enum InterruptRequestFlags: uint8_t {
			Reset		= 0x80,
			IRQ			= Flag::Interrupt,
			NMI			= 0x20,

			PowerOn		= 0x10,
		};
		uint8_t interrupt_requests_ = InterruptRequestFlags::PowerOn;

		bool ready_is_active_ = false;
		bool ready_line_is_enabled_ = false;

		uint8_t irq_line_ = 0, irq_request_history_ = 0;
		bool nmi_line_is_enabled_ = false, set_overflow_line_is_enabled_ = false;

		/*!
			Gets the program representing an RST response.

			@returns The program representing an RST response.
		*/
		inline const MicroOp *get_reset_program();

		/*!
			Gets the program representing an IRQ response.

			@returns The program representing an IRQ response.
		*/
		inline const MicroOp *get_irq_program();

		/*!
			Gets the program representing an NMI response.

			@returns The program representing an NMI response.
		*/
		inline const MicroOp *get_nmi_program();
};

#endif /* _502Storage_h */
