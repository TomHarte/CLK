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

template <Personality personality, typename T, bool uses_ready_line>
void Processor<personality, T, uses_ready_line>::run_for(const Cycles cycles) {

	const auto check_schedule = [&] {
		if(!scheduled_program_counter_) {
			if(interrupt_requests_) {
				if(interrupt_requests_ & (InterruptRequestFlags::Reset | InterruptRequestFlags::PowerOn)) {
					interrupt_requests_ &= ~InterruptRequestFlags::PowerOn;
					scheduled_program_counter_ = operations_[size_t(OperationsSlot::Reset)];
				} else if(interrupt_requests_ & InterruptRequestFlags::NMI) {
					interrupt_requests_ &= ~InterruptRequestFlags::NMI;
					scheduled_program_counter_ = operations_[size_t(OperationsSlot::NMI)];
				} else if(interrupt_requests_ & InterruptRequestFlags::IRQ) {
					scheduled_program_counter_ = operations_[size_t(OperationsSlot::IRQ)];
				}
			} else {
				scheduled_program_counter_ = operations_[size_t(OperationsSlot::FetchDecodeExecute)];
			}
		}
	};

	check_schedule();
	Cycles number_of_cycles = cycles + cycles_left_to_run_;

	const auto bus_access = [&]() -> bool {
		interrupt_requests_ = (interrupt_requests_ & ~InterruptRequestFlags::IRQ) | irq_request_history_;
		irq_request_history_ = irq_line_ & flags_.inverse_interrupt;
		number_of_cycles -= bus_handler_.perform_bus_operation(next_bus_operation_, bus_address_, bus_value_);
		next_bus_operation_ = BusOperation::None;
		return number_of_cycles <= Cycles(0);
	};

	const auto read_op = [&](uint8_t &val, uint16_t address) {
		next_bus_operation_ = BusOperation::ReadOpcode;
		bus_address_ = address;
		bus_value_ = &val;
		val = 0xff;
	};

	const auto read_mem = [&](uint8_t &val, uint16_t address) {
		next_bus_operation_ = BusOperation::Read;
		bus_address_ = address;
		bus_value_ = &val;
		val = 0xff;
	};

	const auto throwaway_read = [&](uint16_t address) {
		next_bus_operation_ = BusOperation::Read;
		bus_address_ = address;
		bus_value_ = &bus_throwaway_;
		bus_throwaway_ = 0xff;
	};

	const auto write_mem = [&](uint8_t &val, uint16_t address) {
		next_bus_operation_ = BusOperation::Write;
		bus_address_ = address;
		bus_value_ = &val;
	};

	const auto push = [&](uint8_t &val) {
		const uint16_t targetAddress = s_ | 0x100;
		--s_;
		write_mem(val, targetAddress);
	};

	const auto page_crossing_stall_read = [&] {
		if(is_65c02(personality)) {
			throwaway_read(pc_.full - 1);
		} else {
			throwaway_read(address_.full);
		}
	};

	const auto bra = [&](bool condition) {
		++pc_.full;
		if(condition) {
			scheduled_program_counter_ = operations_[size_t(OperationsSlot::DoBRA)];
		}
	};

	while(number_of_cycles > Cycles(0)) {

		// Deal with a potential RDY state, if this 6502 has anything connected to ready.
		while(uses_ready_line && ready_is_active_ && number_of_cycles > Cycles(0)) {
			number_of_cycles -= bus_handler_.perform_bus_operation(BusOperation::Ready, bus_address_, bus_value_);
		}

		// Deal with a potential STP state, if this 6502 implements STP.
		while(has_stpwai(personality) && stop_is_active_ && number_of_cycles > Cycles(0)) {
			number_of_cycles -= bus_handler_.perform_bus_operation(BusOperation::Ready, bus_address_, bus_value_);
			if(interrupt_requests_ & InterruptRequestFlags::Reset) {
				stop_is_active_ = false;
				check_schedule();
				break;
			}
		}

		// Deal with a potential WAI state, if this 6502 implements WAI.
		while(has_stpwai(personality) && wait_is_active_ && number_of_cycles > Cycles(0)) {
			number_of_cycles -= bus_handler_.perform_bus_operation(BusOperation::Ready, bus_address_, bus_value_);
			interrupt_requests_ |= (irq_line_ & flags_.inverse_interrupt);
			if(interrupt_requests_ & InterruptRequestFlags::NMI || irq_line_) {
				wait_is_active_ = false;
				check_schedule();
				break;
			}
		}

		if((!uses_ready_line || !ready_is_active_) && (!has_stpwai(personality) || (!wait_is_active_ && !stop_is_active_))) {
			if(next_bus_operation_ != BusOperation::None) {
				if(bus_access()) break;
			}

			while(1) {

				const MicroOp cycle = *scheduled_program_counter_;
				scheduled_program_counter_++;

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
						check_schedule();
					continue;

					case CycleIncPCPushPCH:				pc_.full++;														[[fallthrough]];
					case CyclePushPCH:					push(pc_.halves.high);											break;
					case CyclePushPCL:					push(pc_.halves.low);											break;
					case CyclePushOperand:				push(operand_);													break;
					case CyclePushA:					push(a_);														break;
					case CyclePushX:					push(x_);														break;
					case CyclePushY:					push(y_);														break;
					case CycleNoWritePush: {
						uint16_t targetAddress = s_ | 0x100;
						--s_;
						read_mem(operand_, targetAddress);
					}
					break;

					case CycleReadFromS:				throwaway_read(s_ | 0x100);										break;
					case CycleReadFromPC:				throwaway_read(pc_.full);										break;

					case OperationBRKPickVector:
						if(is_65c02(personality)) {
							next_address_.full = 0xfffe;
						} else {
							// NMI can usurp BRK-vector operations on the pre-C 6502s.
							next_address_.full = (interrupt_requests_ & InterruptRequestFlags::NMI) ? 0xfffa : 0xfffe;
							interrupt_requests_ &= ~InterruptRequestFlags::NMI;
						}
					continue;
					case OperationNMIPickVector:		next_address_.full = 0xfffa;										continue;
					case OperationRSTPickVector:		next_address_.full = 0xfffc;										continue;
					case CycleReadVectorLow:			read_mem(pc_.halves.low, next_address_.full);						break;
					case CycleReadVectorHigh:			read_mem(pc_.halves.high, next_address_.full+1);					break;
					case OperationSetIRQFlags:
						flags_.inverse_interrupt = 0;
						if(is_65c02(personality)) flags_.decimal = 0;
					continue;
					case OperationSetNMIRSTFlags:
						if(is_65c02(personality)) flags_.decimal = 0;
					continue;

					case CyclePullPCL:					s_++; read_mem(pc_.halves.low, s_ | 0x100);			break;
					case CyclePullPCH:					s_++; read_mem(pc_.halves.high, s_ | 0x100);		break;
					case CyclePullA:					s_++; read_mem(a_, s_ | 0x100);						break;
					case CyclePullX:					s_++; read_mem(x_, s_ | 0x100);						break;
					case CyclePullY:					s_++; read_mem(y_, s_ | 0x100);						break;
					case CyclePullOperand:				s_++; read_mem(operand_, s_ | 0x100);				break;
					case OperationSetFlagsFromOperand:	set_flags(operand_);								continue;
					case OperationSetOperandFromFlagsWithBRKSet: operand_ = flags_.get();					continue;
					case OperationSetOperandFromFlags:	operand_ = flags_.get() & ~Flag::Break;				continue;
					case OperationSetFlagsFromA:		flags_.set_nz(a_);									continue;
					case OperationSetFlagsFromX:		flags_.set_nz(x_);									continue;
					case OperationSetFlagsFromY:		flags_.set_nz(y_);									continue;

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

					case OperationORA:	a_ |= operand_;	flags_.set_nz(a_);		continue;
					case OperationAND:	a_ &= operand_;	flags_.set_nz(a_);		continue;
					case OperationEOR:	a_ ^= operand_;	flags_.set_nz(a_);		continue;

// MARK: - Load and Store

					case OperationLDA:	flags_.set_nz(a_ = operand_);			continue;
					case OperationLDX:	flags_.set_nz(x_ = operand_);			continue;
					case OperationLDY:	flags_.set_nz(y_ = operand_);			continue;
					case OperationLAX:	flags_.set_nz(a_ = x_ = operand_);		continue;
					case OperationCopyOperandToA:		a_ = operand_;			continue;

					case OperationSTA:	operand_ = a_;											continue;
					case OperationSTX:	operand_ = x_;											continue;
					case OperationSTY:	operand_ = y_;											continue;
					case OperationSTZ:	operand_ = 0;											continue;
					case OperationSAX:	operand_ = a_ & x_;										continue;

					// For the next four, intended effect is:
					//
					//	CPU calculates what address would be if a page boundary is crossed. The high byte of that
					//	takes part in the AND. If the page boundary is actually crossed then the total AND takes
					//	the place of the intended high byte.
					//
					// Within this implementation, there's a bit of after-the-effect judgment on whether a page
					// boundary was crossed.
					case OperationSHA:
						if(address_.full != next_address_.full) {
							address_.halves.high = operand_ = a_ & x_ & address_.halves.high;
						} else {
							operand_ = a_ & x_ & (address_.halves.high + 1);
						}
					continue;
					case OperationSHX:
						if(address_.full != next_address_.full) {
							address_.halves.high = operand_ = x_ & address_.halves.high;
						} else {
							operand_ = x_ & (address_.halves.high + 1);
						}
					continue;
					case OperationSHY:
						if(address_.full != next_address_.full) {
							address_.halves.high = operand_ = y_ & address_.halves.high;
						} else {
							operand_ = y_ & (address_.halves.high + 1);
						}
					continue;
					case OperationSHS:
						if(address_.full != next_address_.full) {
							s_ = a_ & x_;
							address_.halves.high = operand_ = s_ & address_.halves.high;
						} else {
							s_ = a_ & x_;
							operand_ = s_ & (address_.halves.high + 1);
						}
					continue;

					case OperationLXA:
						a_ = x_ = (a_ | 0xee) & operand_;
						flags_.set_nz(a_);
					continue;

// MARK: - Compare

					case OperationCMP: {
						const uint16_t temp16 = a_ - operand_;
						flags_.set_nz(uint8_t(temp16));
						flags_.carry = ((~temp16) >> 8)&1;
					} continue;
					case OperationCPX: {
						const uint16_t temp16 = x_ - operand_;
						flags_.set_nz(uint8_t(temp16));
						flags_.carry = ((~temp16) >> 8)&1;
					} continue;
					case OperationCPY: {
						const uint16_t temp16 = y_ - operand_;
						flags_.set_nz(uint8_t(temp16));
						flags_.carry = ((~temp16) >> 8)&1;
					} continue;

// MARK: - BIT, TSB, TRB

					case OperationBIT:
						flags_.zero_result = operand_ & a_;
						flags_.negative_result = operand_;
						flags_.overflow = operand_ & Flag::Overflow;
					continue;
					case OperationBITNoNV:
						flags_.zero_result = operand_ & a_;
					continue;
					case OperationTRB:
						flags_.zero_result = operand_ & a_;
						operand_ &= ~a_;
					continue;
					case OperationTSB:
						flags_.zero_result = operand_ & a_;
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
						++operand_;
						[[fallthrough]];
					case OperationSBC:
						operand_ = ~operand_;

						if(flags_.decimal && has_decimal_mode(personality)) {
							uint8_t result = a_ + operand_ + flags_.carry;

							// All flags are set based only on the decimal result.
							flags_.zero_result = result;
							flags_.carry = Numeric::carried_out<true, 7>(a_, operand_, result);
							flags_.negative_result = result;
							flags_.overflow = (( (result ^ a_) & (result ^ operand_) ) & 0x80) >> 1;

							// General SBC logic:
							//
							// Because the range of valid numbers starts at 0, any subtraction that should have
							// caused decimal carry and which requires a digit fix up will definitely have caused
							// binary carry: the subtraction will have crossed zero and gone into negative numbers.
							//
							// So just test for carry (well, actually borrow, which is !carry).

							// The bottom nibble is adjusted if there was borrow into the top nibble;
							// on a 6502 additional borrow isn't propagated but on a 65C02 it is.
							// This difference affects invalid BCD numbers only — valid numbers will
							// never be less than -9 so adding 10 will always generate carry.
							if(!Numeric::carried_in<4>(a_, operand_, result)) {
								if constexpr (is_65c02(personality)) {
									result += 0xfa;
								} else {
									result = (result & 0xf0) | ((result + 0xfa) & 0xf);
								}
							}

							// The top nibble is adjusted only if there was borrow out of the whole byte.
							if(!flags_.carry) {
								result += 0xa0;
							}

							a_ = result;

							// fix up in case this was INS.
							if(cycle == OperationINS) operand_ = ~operand_;

							if constexpr (is_65c02(personality)) {
								// 65C02 fix: set the N and Z flags based on the final, decimal result.
								// Read into `operation_` for the sake of reading somewhere; the value isn't
								// used and INS will write `operand_` back to memory.
								flags_.set_nz(a_);
								read_mem(operation_, address_.full);
								break;
							}
							continue;
						}
						[[fallthrough]];

					case OperationADC:
						if(flags_.decimal && has_decimal_mode(personality)) {
							uint8_t result = a_ + operand_ + flags_.carry;
							flags_.zero_result = result;
							flags_.carry = Numeric::carried_out<true, 7>(a_, operand_, result);

							// General ADC logic:
							//
							// Detecting decimal carry means finding occasions when two digits added together totalled
							// more than 9. Within each four-bit window that means testing the digit itself and also
							// testing for carry — e.g. 5 + 5 = 0xA, which is detectable only by the value of the final
							// digit, but 9 + 9 = 0x18, which is detectable only by spotting the carry.

							// Only a single bit of carry can flow from the bottom nibble to the top.
							//
							// So if that carry already happened, fix up the bottom without permitting another;
							// otherwise permit the carry to happen (and check whether carry then rippled out of bit 7).
							if(Numeric::carried_in<4>(a_, operand_, result)) {
								result = (result & 0xf0) | ((result + 0x06) & 0x0f);
							} else if((result & 0xf) > 0x9) {
								flags_.carry |= result >= 0x100 - 0x6;
								result += 0x06;
							}

							// 6502 quirk: N and V are set before the full result is computed but
							// after the low nibble has been corrected.
							flags_.negative_result = result;
							flags_.overflow = (( (result ^ a_) & (result ^ operand_) ) & 0x80) >> 1;

							// i.e. fix high nibble if there was carry out of bit 7 already, or if the
							// top nibble is too large (in which case there will be carry after the fix-up).
							flags_.carry |= result >= 0xa0;
							if(flags_.carry) {
								result += 0x60;
							}

							a_ = result;

							if constexpr (is_65c02(personality)) {
								// 65C02 fix: N and Z are set correctly based on the final BCD result, at the cost of
								// an extra cycle.
								flags_.set_nz(a_);
								read_mem(operand_, address_.full);
								break;
							}
						} else {
							const uint16_t result = uint16_t(a_) + uint16_t(operand_) + uint16_t(flags_.carry);
							flags_.overflow = (( (result^a_)&(result^operand_) )&0x80) >> 1;
							flags_.set_nz(a_ = uint8_t(result));
							flags_.carry = (result >> 8)&1;
						}

						// fix up in case this was INS.
						if(cycle == OperationINS) operand_ = ~operand_;
					continue;

// MARK: - Shifts and Rolls

					case OperationASL:
						flags_.carry = operand_ >> 7;
						operand_ <<= 1;
						flags_.set_nz(operand_);
					continue;

					case OperationASO:
						flags_.carry = operand_ >> 7;
						operand_ <<= 1;
						a_ |= operand_;
						flags_.set_nz(a_);
					continue;

					case OperationROL: {
						const uint8_t temp8 = uint8_t((operand_ << 1) | flags_.carry);
						flags_.carry = operand_ >> 7;
						flags_.set_nz(operand_ = temp8);
					} continue;

					case OperationRLA: {
						const uint8_t temp8 = uint8_t((operand_ << 1) | flags_.carry);
						flags_.carry = operand_ >> 7;
						operand_ = temp8;
						a_ &= operand_;
						flags_.set_nz(a_);
					} continue;

					case OperationLSR:
						flags_.carry = operand_ & 1;
						operand_ >>= 1;
						flags_.set_nz(operand_);
					continue;

					case OperationLSE:
						flags_.carry = operand_ & 1;
						operand_ >>= 1;
						a_ ^= operand_;
						flags_.set_nz(a_);
					continue;

					case OperationASR:
						a_ &= operand_;
						flags_.carry = a_ & 1;
						a_ >>= 1;
						flags_.set_nz(a_);
					continue;

					case OperationROR: {
						const uint8_t temp8 = uint8_t((operand_ >> 1) | (flags_.carry << 7));
						flags_.carry = operand_ & 1;
						flags_.set_nz(operand_ = temp8);
					} continue;

					case OperationRRA: {
						const uint8_t temp8 = uint8_t((operand_ >> 1) | (flags_.carry << 7));
						flags_.carry = operand_ & 1;
						operand_ = temp8;
					} continue;

					case OperationDecrementOperand: operand_--; continue;
					case OperationIncrementOperand: operand_++; continue;

					case OperationCLC: flags_.carry = 0;							continue;
					case OperationCLI: flags_.inverse_interrupt = Flag::Interrupt;	continue;
					case OperationCLV: flags_.overflow = 0;							continue;
					case OperationCLD: flags_.decimal = 0;							continue;

					case OperationSEC: flags_.carry = Flag::Carry;		continue;
					case OperationSEI: flags_.inverse_interrupt = 0;	continue;
					case OperationSED: flags_.decimal = Flag::Decimal;	continue;

					case OperationINC: operand_++; flags_.set_nz(operand_);		continue;
					case OperationDEC: operand_--; flags_.set_nz(operand_);		continue;
					case OperationINA: a_++; flags_.set_nz(a_);					continue;
					case OperationDEA: a_--; flags_.set_nz(a_);					continue;
					case OperationINX: x_++; flags_.set_nz(x_);					continue;
					case OperationDEX: x_--; flags_.set_nz(x_);					continue;
					case OperationINY: y_++; flags_.set_nz(y_);					continue;
					case OperationDEY: y_--; flags_.set_nz(y_);					continue;

					case OperationANE:
						a_ = (a_ | 0xee) & operand_ & x_;
						flags_.set_nz(a_);
					continue;

					case OperationANC:
						a_ &= operand_;
						flags_.set_nz(a_);
						flags_.carry = a_ >> 7;
					continue;

					case OperationLAS:
						a_ = x_ = s_ = s_ & operand_;
						flags_.set_nz(a_);
					continue;

// MARK: - Addressing Mode Work

					case CycleAddXToAddressLow:
						next_address_.full = address_.full + x_;
						address_.halves.low = next_address_.halves.low;
						if(address_.halves.high != next_address_.halves.high) {
							page_crossing_stall_read();
							break;
						}
					continue;
					case CycleAddYToAddressLow:
						next_address_.full = address_.full + y_;
						address_.halves.low = next_address_.halves.low;
						if(address_.halves.high != next_address_.halves.high) {
							page_crossing_stall_read();
							break;
						}
					continue;

					case CycleAddXToAddressLowRead:
						next_address_.full = address_.full + x_;
						address_.halves.low = next_address_.halves.low;

						// Cf. https://groups.google.com/g/comp.sys.apple2/c/RuTGaRxu5Iw/m/uyFLEsF8ceIJ
						//
						// STA abs,X has been fixed for the PX (page-crossing) case by adding a dummy read of the
						// program counter, so the change was rW -> W. In the non-PX case it still reads the destination
						// address, so there is no change: RW -> RW.
						if(!is_65c02(personality) || next_address_.full == address_.full) {
							throwaway_read(address_.full);
						} else {
							throwaway_read(pc_.full - 1);
						}
					break;
					case CycleAddYToAddressLowRead:
						next_address_.full = address_.full + y_;
						address_.halves.low = next_address_.halves.low;

						// A similar rule as for above applies; this one adjusts (abs, y) addressing.

						if(!is_65c02(personality) || next_address_.full == address_.full) {
							throwaway_read(address_.full);
						} else {
							throwaway_read(pc_.full - 1);
						}
					break;

					case OperationCorrectAddressHigh:
						// Preserve the uncorrected address in next_address_ (albeit that it's
						// now a misnomer) as some of the more obscure illegal operations end
						// up acting differently if an adjustment was necessary and therefore need
						// a crumb trail to test for that.
						std::swap(address_.full, next_address_.full);
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

					case OperationIncrementPC:			pc_.full++;							continue;
					case CycleFetchOperandFromAddress:	read_mem(operand_, address_.full);	break;
					case CycleWriteOperandToAddress:	write_mem(operand_, address_.full);	break;

// MARK: - Branching

					case OperationBPL: bra(!(flags_.negative_result&0x80));			continue;
					case OperationBMI: bra(flags_.negative_result&0x80);			continue;
					case OperationBVC: bra(!flags_.overflow);						continue;
					case OperationBVS: bra(flags_.overflow);						continue;
					case OperationBCC: bra(!flags_.carry);							continue;
					case OperationBCS: bra(flags_.carry);							continue;
					case OperationBNE: bra(flags_.zero_result);						continue;
					case OperationBEQ: bra(!flags_.zero_result);					continue;
					case OperationBRA: bra(true);									continue;

					case CycleAddSignedOperandToPC:
						next_address_.full = uint16_t(pc_.full + int8_t(operand_));
						pc_.halves.low = next_address_.halves.low;
						if(next_address_.halves.high != pc_.halves.high) {
							const uint16_t half_updated_pc = pc_.full;
							pc_.full = next_address_.full;
							throwaway_read(half_updated_pc);
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

					case OperationTXA: flags_.set_nz(a_ = x_);	continue;
					case OperationTYA: flags_.set_nz(a_ = y_);	continue;
					case OperationTXS: s_ = x_;					continue;
					case OperationTAY: flags_.set_nz(y_ = a_);	continue;
					case OperationTAX: flags_.set_nz(x_ = a_);	continue;
					case OperationTSX: flags_.set_nz(x_ = s_);	continue;

					case OperationARR:
						if(flags_.decimal && has_decimal_mode(personality)) {
							a_ &= operand_;
							uint8_t unshiftedA = a_;
							a_ = uint8_t((a_ >> 1) | (flags_.carry << 7));
							flags_.set_nz(a_);
							flags_.overflow = (a_^(a_ << 1))&Flag::Overflow;

							if((unshiftedA&0xf) + (unshiftedA&0x1) > 5) a_ = ((a_ + 6)&0xf) | (a_ & 0xf0);

							flags_.carry = ((unshiftedA&0xf0) + (unshiftedA&0x10) > 0x50) ? 1 : 0;
							if(flags_.carry) a_ += 0x60;
						} else {
							a_ &= operand_;
							a_ = uint8_t((a_ >> 1) | (flags_.carry << 7));
							flags_.set_nz(a_);
							flags_.carry = (a_ >> 6)&1;
							flags_.overflow = (a_^(a_ << 1))&Flag::Overflow;
						}
					continue;

					case OperationSBX:
						x_ &= a_;
						uint16_t difference = x_ - operand_;
						x_ = uint8_t(difference);
						flags_.set_nz(x_);
						flags_.carry = ((difference >> 8)&1)^1;
					continue;
				}

				if(has_stpwai(personality) && (stop_is_active_ || wait_is_active_)) {
					break;
				}
				if(uses_ready_line && ready_line_is_enabled_ && (is_65c02(personality) || isReadOperation(next_bus_operation_))) {
					ready_is_active_ = true;
					break;
				}
				if(bus_access()) break;
			}
		}
	}

	cycles_left_to_run_ = number_of_cycles;
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
	irq_line_ = active ? MOS6502Esque::Flag::Interrupt : 0;
}

void ProcessorBase::set_overflow_line(bool active) {
	// a leading edge will set the overflow flag
	if(active && !set_overflow_line_is_enabled_)
		flags_.overflow = MOS6502Esque::Flag::Overflow;
	set_overflow_line_is_enabled_ = active;
}

void ProcessorBase::set_nmi_line(bool active) {
	// NMI is edge triggered, not level
	if(active && !nmi_line_is_enabled_)
		interrupt_requests_ |= InterruptRequestFlags::NMI;
	nmi_line_is_enabled_ = active;
}

uint8_t ProcessorStorage::get_flags() const {
	return flags_.get();
}

void ProcessorStorage::set_flags(uint8_t flags) {
	flags_.set(flags);
}

bool ProcessorBase::is_jammed() const {
	return is_jammed_;
}

uint16_t ProcessorBase::value_of(Register r) const {
	switch (r) {
		case Register::ProgramCounter:			return pc_.full;
		case Register::LastOperationAddress:	return last_operation_pc_.full;
		case Register::StackPointer:			return s_;
		case Register::Flags:					return get_flags();
		case Register::A:						return a_;
		case Register::X:						return x_;
		case Register::Y:						return y_;
		default: return 0;
	}
}

void ProcessorBase::set_value_of(Register r, uint16_t value) {
	switch (r) {
		case Register::ProgramCounter:	pc_.full = value;			break;
		case Register::StackPointer:	s_ = uint8_t(value);		break;
		case Register::Flags:			set_flags(uint8_t(value));	break;
		case Register::A:				a_ = uint8_t(value);		break;
		case Register::X:				x_ = uint8_t(value);		break;
		case Register::Y:				y_ = uint8_t(value);		break;
		default: break;
	}
}

void ProcessorBase::restart_operation_fetch() {
	scheduled_program_counter_ = nullptr;
	next_bus_operation_ = BusOperation::None;
}
