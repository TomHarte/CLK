//
//  6502Implementation.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/09/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

/*
	Here lies the implementations of those methods declared in the CPU::MOS6502::Processor template, or declared
	as inline within CPU::MOS6502::ProcessorBase. So it's stuff that has to be in a header file, visible from
	6502.hpp, but it's implementation stuff.
*/

template <Personality personality, typename T, bool uses_ready_line> void Processor<personality, T, uses_ready_line>::run_for(const Cycles cycles) {
	static uint8_t throwaway_target;

	// These plus program below act to give the compiler permission to update these values
	// without touching the class storage (i.e. it explicitly says they need be completely up
	// to date in this stack frame only); which saves some complicated addressing
	RegisterPair16 nextAddress = next_address_;
	BusOperation nextBusOperation = next_bus_operation_;
	uint16_t busAddress = bus_address_;
	uint8_t *busValue = bus_value_;

#define checkSchedule() \
	if(!scheduled_program_counter_) {\
		if(interrupt_requests_) {\
			if(interrupt_requests_ & (InterruptRequestFlags::Reset | InterruptRequestFlags::PowerOn)) {\
				interrupt_requests_ &= ~InterruptRequestFlags::PowerOn;\
				scheduled_program_counter_ = operations_[size_t(OperationsSlot::Reset)];\
			} else if(interrupt_requests_ & InterruptRequestFlags::NMI) {\
				interrupt_requests_ &= ~InterruptRequestFlags::NMI;\
				scheduled_program_counter_ = operations_[size_t(OperationsSlot::NMI)];\
			} else if(interrupt_requests_ & InterruptRequestFlags::IRQ) {\
				scheduled_program_counter_ = operations_[size_t(OperationsSlot::IRQ)];\
			} \
		} else {\
			scheduled_program_counter_ = operations_[size_t(OperationsSlot::FetchDecodeExecute)];\
		}\
	}

#define bus_access() \
	interrupt_requests_ = (interrupt_requests_ & ~InterruptRequestFlags::IRQ) | irq_request_history_;	\
	irq_request_history_ = irq_line_ & inverse_interrupt_flag_;	\
	number_of_cycles -= bus_handler_.perform_bus_operation(nextBusOperation, busAddress, busValue);	\
	nextBusOperation = BusOperation::None;	\
	if(number_of_cycles <= Cycles(0)) break;

	checkSchedule();
	Cycles number_of_cycles = cycles + cycles_left_to_run_;

	while(number_of_cycles > Cycles(0)) {

		// Deal with a potential RDY state, if this 6502 has anything connected to ready.
		while(uses_ready_line && ready_is_active_ && number_of_cycles > Cycles(0)) {
			number_of_cycles -= bus_handler_.perform_bus_operation(BusOperation::Ready, busAddress, busValue);
		}

		// Deal with a potential STP state, if this 6502 implements STP.
		while(has_stpwai(personality) && stop_is_active_ && number_of_cycles > Cycles(0)) {
			number_of_cycles -= bus_handler_.perform_bus_operation(BusOperation::Ready, busAddress, busValue);
			if(interrupt_requests_ & InterruptRequestFlags::Reset) {
				stop_is_active_ = false;
				checkSchedule();
				break;
			}
		}

		// Deal with a potential WAI state, if this 6502 implements WAI.
		while(has_stpwai(personality) && wait_is_active_ && number_of_cycles > Cycles(0)) {
			number_of_cycles -= bus_handler_.perform_bus_operation(BusOperation::Ready, busAddress, busValue);
			interrupt_requests_ |= (irq_line_ & inverse_interrupt_flag_);
			if(interrupt_requests_ & InterruptRequestFlags::NMI || irq_line_) {
				wait_is_active_ = false;
				checkSchedule();
				break;
			}
		}

		if((!uses_ready_line || !ready_is_active_) && (!has_stpwai(personality) || (!wait_is_active_ && !stop_is_active_))) {
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

// MARK: - Fetch/Decode

					case CycleFetchOperation: {
						last_operation_pc_ = pc_;
						pc_.full++;
						read_op(operation_, last_operation_pc_.full);
					} break;

					case CycleFetchOperand:
						// This is supposed to produce the 65C02's 1-cycle NOPs; they're
						// treated as a special case because they break the rule that
						// governs everything else on the 6502: that two bytes will always
						// be fetched.
						if(
							!is_65c02(personality) ||
							(operation_&7) != 3 ||
							operation_ == 0xcb ||
							operation_ == 0xdb
						) {
							read_mem(operand_, pc_.full);
							break;
						} else {
							continue;
						}
					break;

					case OperationDecodeOperation:
						scheduled_program_counter_ = operations_[operation_];
					continue;

					case OperationMoveToNextProgram:
						scheduled_program_counter_ = nullptr;
						checkSchedule();
					continue;

#define push(v) {\
	uint16_t targetAddress = s_ | 0x100; s_--;\
	write_mem(v, targetAddress);\
}

					case CycleIncPCPushPCH:				pc_.full++;														[[fallthrough]];
					case CyclePushPCH:					push(pc_.halves.high);											break;
					case CyclePushPCL:					push(pc_.halves.low);											break;
					case CyclePushOperand:				push(operand_);													break;
					case CyclePushA:					push(a_);														break;
					case CyclePushX:					push(x_);														break;
					case CyclePushY:					push(y_);														break;
					case CycleNoWritePush: {
						uint16_t targetAddress = s_ | 0x100; s_--;
						read_mem(operand_, targetAddress);
					}
					break;

#undef push

					case CycleReadFromS:				throwaway_read(s_ | 0x100);										break;
					case CycleReadFromPC:				throwaway_read(pc_.full);										break;

					case OperationBRKPickVector:
						if(is_65c02(personality)) {
							nextAddress.full = 0xfffe;
						} else {
							// NMI can usurp BRK-vector operations on the pre-C 6502s.
							nextAddress.full = (interrupt_requests_ & InterruptRequestFlags::NMI) ? 0xfffa : 0xfffe;
							interrupt_requests_ &= ~InterruptRequestFlags::NMI;
						}
					continue;
					case OperationNMIPickVector:		nextAddress.full = 0xfffa;											continue;
					case OperationRSTPickVector:		nextAddress.full = 0xfffc;											continue;
					case CycleReadVectorLow:			read_mem(pc_.halves.low, nextAddress.full);							break;
					case CycleReadVectorHigh:			read_mem(pc_.halves.high, nextAddress.full+1);						break;
					case OperationSetIRQFlags:
						inverse_interrupt_flag_ = 0;
						if(is_65c02(personality)) decimal_flag_ = false;
					continue;
					case OperationSetNMIRSTFlags:
						if(is_65c02(personality)) decimal_flag_ = false;
					continue;

					case CyclePullPCL:					s_++; read_mem(pc_.halves.low, s_ | 0x100);							break;
					case CyclePullPCH:					s_++; read_mem(pc_.halves.high, s_ | 0x100);						break;
					case CyclePullA:					s_++; read_mem(a_, s_ | 0x100);										break;
					case CyclePullX:					s_++; read_mem(x_, s_ | 0x100);										break;
					case CyclePullY:					s_++; read_mem(y_, s_ | 0x100);										break;
					case CyclePullOperand:				s_++; read_mem(operand_, s_ | 0x100);								break;
					case OperationSetFlagsFromOperand:	set_flags(operand_);												continue;
					case OperationSetOperandFromFlagsWithBRKSet: operand_ = get_flags() | Flag::Break;						continue;
					case OperationSetOperandFromFlags:  operand_ = get_flags();												continue;
					case OperationSetFlagsFromA:		zero_result_ = negative_result_ = a_;								continue;
					case OperationSetFlagsFromX:		zero_result_ = negative_result_ = x_;								continue;
					case OperationSetFlagsFromY:		zero_result_ = negative_result_ = y_;								continue;

					case CycleIncrementPCAndReadStack:	pc_.full++; throwaway_read(s_ | 0x100);														break;
					case CycleReadPCLFromAddress:		read_mem(pc_.halves.low, address_.full);													break;
					case CycleReadPCHFromAddressLowInc:	address_.halves.low++; read_mem(pc_.halves.high, address_.full);							break;
					case CycleReadPCHFromAddressFixed:	if(!address_.halves.low) address_.halves.high++; read_mem(pc_.halves.high, address_.full);	break;
					case CycleReadPCHFromAddressInc:	address_.full++; read_mem(pc_.halves.high, address_.full);									break;

					case CycleReadAndIncrementPC: {
						uint16_t oldPC = pc_.full;
						pc_.full++;
						throwaway_read(oldPC);
					} break;

// MARK: - JAM, WAI, STP

					case OperationScheduleJam: {
						is_jammed_ = true;
						scheduled_program_counter_ = operations_[CPU::MOS6502::JamOpcode];
					} continue;

					case OperationScheduleStop:
						stop_is_active_ = true;
					break;

					case OperationScheduleWait:
						wait_is_active_ = true;
					break;

// MARK: - Bitwise

					case OperationORA:	a_ |= operand_;	negative_result_ = zero_result_ = a_;		continue;
					case OperationAND:	a_ &= operand_;	negative_result_ = zero_result_ = a_;		continue;
					case OperationEOR:	a_ ^= operand_;	negative_result_ = zero_result_ = a_;		continue;

// MARK: - Load and Store

					case OperationLDA:	a_ = negative_result_ = zero_result_ = operand_;			continue;
					case OperationLDX:	x_ = negative_result_ = zero_result_ = operand_;			continue;
					case OperationLDY:	y_ = negative_result_ = zero_result_ = operand_;			continue;
					case OperationLAX:	a_ = x_ = negative_result_ = zero_result_ = operand_;		continue;
					case OperationCopyOperandToA:		a_ = operand_;								continue;

					case OperationSTA:	operand_ = a_;											continue;
					case OperationSTX:	operand_ = x_;											continue;
					case OperationSTY:	operand_ = y_;											continue;
					case OperationSTZ:	operand_ = 0;											continue;
					case OperationSAX:	operand_ = a_ & x_;										continue;
					case OperationSHA:	operand_ = a_ & x_ & (address_.halves.high+1);			continue;
					case OperationSHX:	operand_ = x_ & (address_.halves.high+1);				continue;
					case OperationSHY:	operand_ = y_ & (address_.halves.high+1);				continue;
					case OperationSHS:	s_ = a_ & x_; operand_ = s_ & (address_.halves.high+1);	continue;

					case OperationLXA:
						a_ = x_ = (a_ | 0xee) & operand_;
						negative_result_ = zero_result_ = a_;
					continue;

// MARK: - Compare

					case OperationCMP: {
						const uint16_t temp16 = a_ - operand_;
						negative_result_ = zero_result_ = uint8_t(temp16);
						carry_flag_ = ((~temp16) >> 8)&1;
					} continue;
					case OperationCPX: {
						const uint16_t temp16 = x_ - operand_;
						negative_result_ = zero_result_ = uint8_t(temp16);
						carry_flag_ = ((~temp16) >> 8)&1;
					} continue;
					case OperationCPY: {
						const uint16_t temp16 = y_ - operand_;
						negative_result_ = zero_result_ = uint8_t(temp16);
						carry_flag_ = ((~temp16) >> 8)&1;
					} continue;

// MARK: - BIT, TSB, TRB

					case OperationBIT:
						zero_result_ = operand_ & a_;
						negative_result_ = operand_;
						overflow_flag_ = operand_&Flag::Overflow;
					continue;
					case OperationBITNoNV:
						zero_result_ = operand_ & a_;
					continue;
					case OperationTRB:
						zero_result_ = operand_ & a_;
						operand_ &= ~a_;
					continue;
					case OperationTSB:
						zero_result_ = operand_ & a_;
						operand_ |= a_;
					continue;

// MARK: - RMB and SMB

					case OperationRMB:
						operand_ &= ~(1 << (operation_ >> 4));
					continue;
					case OperationSMB:
						operand_ |= 1 << ((operation_ >> 4)&7);
					continue;

// MARK: - ADC/SBC (and INS)

					case OperationINS:
						operand_++;
						[[fallthrough]];
					case OperationSBC:
						if(decimal_flag_ && has_decimal_mode(personality)) {
							const uint16_t notCarry = carry_flag_ ^ 0x1;
							const uint16_t decimalResult = uint16_t(a_) - uint16_t(operand_) - notCarry;
							uint16_t temp16;

							temp16 = (a_&0xf) - (operand_&0xf) - notCarry;
							if(temp16 > 0xf) temp16 -= 0x6;
							temp16 = (temp16&0x0f) | ((temp16 > 0x0f) ? 0xfff0 : 0x00);
							temp16 += (a_&0xf0) - (operand_&0xf0);

							overflow_flag_ = ( ( (decimalResult^a_)&(~decimalResult^operand_) )&0x80) >> 1;
							negative_result_ = uint8_t(temp16);
							zero_result_ = uint8_t(decimalResult);

							if(temp16 > 0xff) temp16 -= 0x60;

							carry_flag_ = (temp16 > 0xff) ? 0 : Flag::Carry;
							a_ = uint8_t(temp16);

							if(is_65c02(personality)) {
								negative_result_ = zero_result_ = a_;
								read_mem(operand_, address_.full);
								break;
							}
							continue;
						} else {
							operand_ = ~operand_;
						}
						[[fallthrough]];

					case OperationADC:
						if(decimal_flag_ && has_decimal_mode(personality)) {
							const uint16_t decimalResult = uint16_t(a_) + uint16_t(operand_) + uint16_t(carry_flag_);

							uint8_t low_nibble = (a_ & 0xf) + (operand_ & 0xf) + carry_flag_;
							if(low_nibble >= 0xa) low_nibble = ((low_nibble + 0x6) & 0xf) + 0x10;
							uint16_t result = uint16_t(a_ & 0xf0) + uint16_t(operand_ & 0xf0) + uint16_t(low_nibble);
							negative_result_ = uint8_t(result);
							overflow_flag_ = (( (result^a_)&(result^operand_) )&0x80) >> 1;
							if(result >= 0xa0) result += 0x60;

							carry_flag_ = (result >> 8) ? 1 : 0;
							a_ = uint8_t(result);
							zero_result_ = uint8_t(decimalResult);

							if(is_65c02(personality)) {
								negative_result_ = zero_result_ = a_;
								read_mem(operand_, address_.full);
								break;
							}
						} else {
							const uint16_t result = uint16_t(a_) + uint16_t(operand_) + uint16_t(carry_flag_);
							overflow_flag_ = (( (result^a_)&(result^operand_) )&0x80) >> 1;
							negative_result_ = zero_result_ = a_ = uint8_t(result);
							carry_flag_ = (result >> 8)&1;
						}

						// fix up in case this was INS
						if(cycle == OperationINS) operand_ = ~operand_;
					continue;

// MARK: - Shifts and Rolls

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
						const uint8_t temp8 = uint8_t((operand_ << 1) | carry_flag_);
						carry_flag_ = operand_ >> 7;
						operand_ = negative_result_ = zero_result_ = temp8;
					} continue;

					case OperationRLA: {
						const uint8_t temp8 = uint8_t((operand_ << 1) | carry_flag_);
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
						const uint8_t temp8 = uint8_t((operand_ >> 1) | (carry_flag_ << 7));
						carry_flag_ = operand_ & 1;
						operand_ = negative_result_ = zero_result_ = temp8;
					} continue;

					case OperationRRA: {
						const uint8_t temp8 = uint8_t((operand_ >> 1) | (carry_flag_ << 7));
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
					case OperationINA: a_++; negative_result_ = zero_result_ = a_; continue;
					case OperationDEA: a_--; negative_result_ = zero_result_ = a_; continue;
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

// MARK: - Addressing Mode Work

#define page_crossing_stall_read()	\
	if(is_65c02(personality)) {	\
		throwaway_read(pc_.full - 1);	\
	} else {	\
		throwaway_read(address_.full);	\
	}

					case CycleAddXToAddressLow:
						nextAddress.full = address_.full + x_;
						address_.halves.low = nextAddress.halves.low;
						if(address_.halves.high != nextAddress.halves.high) {
							page_crossing_stall_read();
							break;
						}
					continue;
					case CycleAddXToAddressLowRead:
						nextAddress.full = address_.full + x_;
						address_.halves.low = nextAddress.halves.low;
						page_crossing_stall_read();
					break;
					case CycleAddYToAddressLow:
						nextAddress.full = address_.full + y_;
						address_.halves.low = nextAddress.halves.low;
						if(address_.halves.high != nextAddress.halves.high) {
							page_crossing_stall_read();
							break;
						}
					continue;
					case CycleAddYToAddressLowRead:
						nextAddress.full = address_.full + y_;
						address_.halves.low = nextAddress.halves.low;
						page_crossing_stall_read();
					break;

#undef page_crossing_stall_read

					case OperationCorrectAddressHigh:
						address_.full = nextAddress.full;
					continue;
					case CycleIncrementPCFetchAddressLowFromOperand:
						pc_.full++;
						read_mem(address_.halves.low, operand_);
					break;
					case CycleAddXToOperandFetchAddressLow:
						operand_ += x_;
						read_mem(address_.halves.low, operand_);
					break;
					case CycleFetchAddressLowFromOperand:
						read_mem(address_.halves.low, operand_);
					break;
					case CycleIncrementOperandFetchAddressHigh:
						operand_++;
						read_mem(address_.halves.high, operand_);
					break;
					case CycleIncrementPCReadPCHLoadPCL:
						pc_.full++;
						[[fallthrough]];
					case CycleReadPCHLoadPCL: {
						uint16_t oldPC = pc_.full;
						pc_.halves.low = operand_;
						read_mem(pc_.halves.high, oldPC);
					} break;

					case CycleReadAddressHLoadAddressL:
						address_.halves.low = operand_; pc_.full++;
						read_mem(address_.halves.high, pc_.full);
					break;

					case CycleLoadAddressAbsolute: {
						uint16_t nextPC = pc_.full+1;
						pc_.full += 2;
						address_.halves.low = operand_;
						read_mem(address_.halves.high, nextPC);
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

// MARK: - Branching

#define BRA(condition)	\
	pc_.full++; \
	if(condition) {	\
		scheduled_program_counter_ = operations_[size_t(OperationsSlot::DoBRA)];	\
	}

					case OperationBPL: BRA(!(negative_result_&0x80));				continue;
					case OperationBMI: BRA(negative_result_&0x80);					continue;
					case OperationBVC: BRA(!overflow_flag_);						continue;
					case OperationBVS: BRA(overflow_flag_);							continue;
					case OperationBCC: BRA(!carry_flag_);							continue;
					case OperationBCS: BRA(carry_flag_);							continue;
					case OperationBNE: BRA(zero_result_);							continue;
					case OperationBEQ: BRA(!zero_result_);							continue;
					case OperationBRA: BRA(true);									continue;

#undef BRA

					case CycleAddSignedOperandToPC:
						nextAddress.full = uint16_t(pc_.full + int8_t(operand_));
						pc_.halves.low = nextAddress.halves.low;
						if(nextAddress.halves.high != pc_.halves.high) {
							uint16_t halfUpdatedPc = pc_.full;
							pc_.full = nextAddress.full;
							throwaway_read(halfUpdatedPc);
							break;
						} else if(is_65c02(personality)) {
							// 65C02 modification to all branches: a branch that is taken but requires only a single cycle
							// to target its destination skips any pending interrupts.
							// Cf. http://forum.6502.org/viewtopic.php?f=4&t=1634
							scheduled_program_counter_ = operations_[size_t(OperationsSlot::FetchDecodeExecute)];
						}
					continue;

					case CycleFetchFromHalfUpdatedPC: {
						uint16_t halfUpdatedPc = uint16_t(((pc_.halves.low + int8_t(operand_)) & 0xff) | (pc_.halves.high << 8));
						throwaway_read(halfUpdatedPc);
					} break;

					case OperationAddSignedOperandToPC16:
						pc_.full = uint16_t(pc_.full + int8_t(operand_));
					continue;

					case OperationBBRBBS: {
						// To reach here, the 6502 has (i) read the operation; (ii) read the first operand;
						// and (iii) read from the corresponding zero page.
						const uint8_t mask = uint8_t(1 << ((operation_ >> 4)&7));
						if((operand_ & mask) == ((operation_ & 0x80) ? mask : 0)) {
							scheduled_program_counter_ = operations_[size_t(OperationsSlot::DoBBRBBS)];
						} else {
							scheduled_program_counter_ = operations_[size_t(OperationsSlot::DoNotBBRBBS)];
						}
					} break;

// MARK: - Transfers

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
							a_ = uint8_t((a_ >> 1) | (carry_flag_ << 7));
							zero_result_ = negative_result_ = a_;
							overflow_flag_ = (a_^(a_ << 1))&Flag::Overflow;

							if((unshiftedA&0xf) + (unshiftedA&0x1) > 5) a_ = ((a_ + 6)&0xf) | (a_ & 0xf0);

							carry_flag_ = ((unshiftedA&0xf0) + (unshiftedA&0x10) > 0x50) ? 1 : 0;
							if(carry_flag_) a_ += 0x60;
						} else {
							a_ &= operand_;
							a_ = uint8_t((a_ >> 1) | (carry_flag_ << 7));
							negative_result_ = zero_result_ = a_;
							carry_flag_ = (a_ >> 6)&1;
							overflow_flag_ = (a_^(a_ << 1))&Flag::Overflow;
						}
					continue;

					case OperationSBX:
						x_ &= a_;
						uint16_t difference = x_ - operand_;
						x_ = uint8_t(difference);
						negative_result_ = zero_result_ = x_;
						carry_flag_ = ((difference >> 8)&1)^1;
					continue;
				}

				if(has_stpwai(personality) && (stop_is_active_ || wait_is_active_)) {
					break;
				}
				if(uses_ready_line && ready_line_is_enabled_ && (is_65c02(personality) || isReadOperation(nextBusOperation))) {
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

	bus_handler_.flush();
}

template <Personality personality, typename T, bool uses_ready_line> void Processor<personality, T, uses_ready_line>::set_ready_line(bool active) {
	assert(uses_ready_line);
	if(active) {
		ready_line_is_enabled_ = true;
	} else {
		ready_line_is_enabled_ = false;
		ready_is_active_ = false;
	}
}

void ProcessorBase::set_reset_line(bool active) {
	interrupt_requests_ = (interrupt_requests_ & ~InterruptRequestFlags::Reset) | (active ? InterruptRequestFlags::Reset : 0);
}

bool ProcessorBase::get_is_resetting() const {
	return interrupt_requests_ & (InterruptRequestFlags::Reset | InterruptRequestFlags::PowerOn);
}

void ProcessorBase::set_power_on(bool active) {
	interrupt_requests_ = (interrupt_requests_ & ~InterruptRequestFlags::PowerOn) | (active ? InterruptRequestFlags::PowerOn : 0);
}

void ProcessorBase::set_irq_line(bool active) {
	irq_line_ = active ? Flag::Interrupt : 0;
}

void ProcessorBase::set_overflow_line(bool active) {
	// a leading edge will set the overflow flag
	if(active && !set_overflow_line_is_enabled_)
		overflow_flag_ = Flag::Overflow;
	set_overflow_line_is_enabled_ = active;
}

void ProcessorBase::set_nmi_line(bool active) {
	// NMI is edge triggered, not level
	if(active && !nmi_line_is_enabled_)
		interrupt_requests_ |= InterruptRequestFlags::NMI;
	nmi_line_is_enabled_ = active;
}

uint8_t ProcessorStorage::get_flags() const {
	return carry_flag_ | overflow_flag_ | (inverse_interrupt_flag_ ^ Flag::Interrupt) | (negative_result_ & 0x80) | (zero_result_ ? 0 : Flag::Zero) | Flag::Always | decimal_flag_;
}

void ProcessorStorage::set_flags(uint8_t flags) {
	carry_flag_				= flags		& Flag::Carry;
	negative_result_		= flags		& Flag::Sign;
	zero_result_			= (~flags)	& Flag::Zero;
	overflow_flag_			= flags		& Flag::Overflow;
	inverse_interrupt_flag_	= (~flags)	& Flag::Interrupt;
	decimal_flag_			= flags		& Flag::Decimal;
}
