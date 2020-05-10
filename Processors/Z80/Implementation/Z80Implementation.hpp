//
//  Z80Implementation.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/09/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

template <	class T,
			bool uses_bus_request,
			bool uses_wait_line> Processor <T, uses_bus_request, uses_wait_line>
				::Processor(T &bus_handler) :
					bus_handler_(bus_handler) {
	install_default_instruction_set();
}

template <	class T,
			bool uses_bus_request,
			bool uses_wait_line> void Processor <T, uses_bus_request, uses_wait_line>
				::run_for(const HalfCycles cycles) {
#define advance_operation() \
	pc_increment_ = 1;	\
	if(last_request_status_) {	\
		halt_mask_ = 0xff;	\
		if(last_request_status_ & (Interrupt::PowerOn | Interrupt::Reset)) {	\
			request_status_ &= ~Interrupt::PowerOn;	\
			scheduled_program_counter_ = reset_program_.data();	\
		} else if(last_request_status_ & Interrupt::NMI) {	\
			request_status_ &= ~Interrupt::NMI;	\
			scheduled_program_counter_ = nmi_program_.data();	\
		} else if(last_request_status_ & Interrupt::IRQ) {	\
			scheduled_program_counter_ = irq_program_[interrupt_mode_].data();	\
		}	\
	} else {	\
		current_instruction_page_ = &base_page_;	\
		scheduled_program_counter_ = base_page_.fetch_decode_execute_data;	\
	}

	number_of_cycles_ += cycles;
	if(!scheduled_program_counter_) {
		advance_operation();
	}

	while(1) {

		do_bus_acknowledge:
		while(uses_bus_request && bus_request_line_) {
			static PartialMachineCycle bus_acknowledge_cycle = {PartialMachineCycle::BusAcknowledge, HalfCycles(2), nullptr, nullptr, false};
			number_of_cycles_ -= bus_handler_.perform_machine_cycle(bus_acknowledge_cycle) + HalfCycles(1);
			if(!number_of_cycles_) {
				bus_handler_.flush();
				return;
			}
		}

		while(true) {
			const MicroOp *const operation = scheduled_program_counter_;
			scheduled_program_counter_++;

#define set_did_compute_flags()	\
	flag_adjustment_history_ |= 1;

#define set_parity(v)	\
	parity_overflow_result_ = uint8_t(v^1);\
	parity_overflow_result_ ^= parity_overflow_result_ >> 4;\
	parity_overflow_result_ ^= parity_overflow_result_ << 2;\
	parity_overflow_result_ ^= parity_overflow_result_ >> 1;

			switch(operation->type) {
				case MicroOp::BusOperation:
					if(number_of_cycles_ < operation->machine_cycle.length) {
						scheduled_program_counter_--;
						bus_handler_.flush();
						return;
					}
					if(uses_wait_line && operation->machine_cycle.was_requested) {
						if(wait_line_) {
							scheduled_program_counter_--;
						} else {
							continue;
						}
					}
					number_of_cycles_ -= operation->machine_cycle.length;
					last_request_status_ = request_status_;
					number_of_cycles_ -= bus_handler_.perform_machine_cycle(operation->machine_cycle);
					if(uses_bus_request && bus_request_line_) goto do_bus_acknowledge;
				break;
				case MicroOp::MoveToNextProgram:
					advance_operation();
				break;
				case MicroOp::DecodeOperation:
					refresh_addr_ = ir_;
					ir_.halves.low = (ir_.halves.low & 0x80) | ((ir_.halves.low + current_instruction_page_->r_step) & 0x7f);
					pc_.full += pc_increment_ & uint16_t(halt_mask_);
					scheduled_program_counter_ = current_instruction_page_->instructions[operation_ & halt_mask_];
					flag_adjustment_history_ <<= 1;
				break;
				case MicroOp::DecodeOperationNoRChange:
					refresh_addr_ = ir_;
					pc_.full += pc_increment_ & uint16_t(halt_mask_);
					scheduled_program_counter_ = current_instruction_page_->instructions[operation_ & halt_mask_];
				break;

				case MicroOp::Increment8NoFlags:	++ *static_cast<uint8_t *>(operation->source);			break;
				case MicroOp::Increment16:			++ *static_cast<uint16_t *>(operation->source);			break;
				case MicroOp::IncrementPC:			pc_.full += pc_increment_;								break;
				case MicroOp::Decrement16:			-- *static_cast<uint16_t *>(operation->source);			break;
				case MicroOp::Move8:				*static_cast<uint8_t *>(operation->destination) = *static_cast<uint8_t *>(operation->source);		break;
				case MicroOp::Move16:				*static_cast<uint16_t *>(operation->destination) = *static_cast<uint16_t *>(operation->source);		break;

				case MicroOp::AssembleAF:
					temp16_.halves.high = a_;
					temp16_.halves.low = get_flags();
				break;
				case MicroOp::DisassembleAF:
					a_ = temp16_.halves.high;
					set_flags(temp16_.halves.low);
				break;

// MARK: - Logical

#define set_logical_flags(hf)	\
	sign_result_ = zero_result_ = bit53_result_ = a_;	\
	set_parity(a_);	\
	half_carry_result_ = hf;	\
	subtract_flag_ = 0;	\
	carry_result_ = 0;	\
	set_did_compute_flags();

				case MicroOp::And:
					a_ &= *static_cast<uint8_t *>(operation->source);
					set_logical_flags(Flag::HalfCarry);
				break;

				case MicroOp::Or:
					a_ |= *static_cast<uint8_t *>(operation->source);
					set_logical_flags(0);
				break;

				case MicroOp::Xor:
					a_ ^= *static_cast<uint8_t *>(operation->source);
					set_logical_flags(0);
				break;

#undef set_logical_flags

				case MicroOp::CPL:
					a_ ^= 0xff;
					subtract_flag_ = Flag::Subtract;
					half_carry_result_ = Flag::HalfCarry;
					bit53_result_ = a_;
					set_did_compute_flags();
				break;

				case MicroOp::CCF:
					half_carry_result_ = uint8_t(carry_result_ << 4);
					carry_result_ ^= Flag::Carry;
					subtract_flag_ = 0;
					if(flag_adjustment_history_&2) {
						bit53_result_ = a_;
					} else {
						bit53_result_ |= a_;
					}
					set_did_compute_flags();
				break;

				case MicroOp::SCF:
					carry_result_ = Flag::Carry;
					half_carry_result_ = 0;
					subtract_flag_ = 0;
					if(flag_adjustment_history_&2) {
						bit53_result_ = a_;
					} else {
						bit53_result_ |= a_;
					}
					set_did_compute_flags();
				break;

// MARK: - Flow control

				case MicroOp::DJNZ:
					bc_.halves.high--;
					if(!bc_.halves.high) {
						advance_operation();
					}
				break;

				case MicroOp::CalculateRSTDestination:
					memptr_.full = operation_ & 0x38;
				break;

// MARK: - 8-bit arithmetic

#define set_arithmetic_flags(sub, b53)	\
	sign_result_ = zero_result_ = uint8_t(result);	\
	carry_result_ = uint8_t(result >> 8);	\
	half_carry_result_ = uint8_t(half_result);	\
	parity_overflow_result_ = uint8_t(overflow >> 5);	\
	subtract_flag_ = sub;	\
	bit53_result_ = uint8_t(b53);	\
	set_did_compute_flags();

				case MicroOp::CP8: {
					const uint8_t value = *static_cast<uint8_t *>(operation->source);
					const int result = a_ - value;
					const int half_result = (a_&0xf) - (value&0xf);

					// overflow for a subtraction is when the signs were originally
					// different and the result is different again
					const int overflow = (value^a_) & (result^a_);

					// the 5 and 3 flags come from the operand, atypically
					set_arithmetic_flags(Flag::Subtract, value);
				} break;

				case MicroOp::SUB8: {
					const uint8_t value = *static_cast<uint8_t *>(operation->source);
					const int result = a_ - value;
					const int half_result = (a_&0xf) - (value&0xf);

					// overflow for a subtraction is when the signs were originally
					// different and the result is different again
					const int overflow = (value^a_) & (result^a_);

					a_ = uint8_t(result);
					set_arithmetic_flags(Flag::Subtract, result);
				} break;

				case MicroOp::SBC8: {
					const uint8_t value = *static_cast<uint8_t *>(operation->source);
					const int result = a_ - value - (carry_result_ & Flag::Carry);
					const int half_result = (a_&0xf) - (value&0xf) - (carry_result_ & Flag::Carry);

					// overflow for a subtraction is when the signs were originally
					// different and the result is different again
					const int overflow = (value^a_) & (result^a_);

					a_ = uint8_t(result);
					set_arithmetic_flags(Flag::Subtract, result);
				} break;

				case MicroOp::ADD8: {
					const uint8_t value = *static_cast<uint8_t *>(operation->source);
					const int result = a_ + value;
					const int half_result = (a_&0xf) + (value&0xf);

					// overflow for addition is when the signs were originally
					// the same and the result is different
					const int overflow = ~(value^a_) & (result^a_);

					a_ = uint8_t(result);
					set_arithmetic_flags(0, result);
				} break;

				case MicroOp::ADC8: {
					const uint8_t value = *static_cast<uint8_t *>(operation->source);
					const int result = a_ + value + (carry_result_ & Flag::Carry);
					const int half_result = (a_&0xf) + (value&0xf) + (carry_result_ & Flag::Carry);

					// overflow for addition is when the signs were originally
					// the same and the result is different
					const int overflow = ~(value^a_) & (result^a_);

					a_ = uint8_t(result);
					set_arithmetic_flags(0, result);
				} break;

#undef set_arithmetic_flags

				case MicroOp::NEG: {
					const int overflow = (a_ == 0x80);
					const int result = -a_;
					const int halfResult = -(a_&0xf);

					a_ = uint8_t(result);
					bit53_result_ = sign_result_ = zero_result_ = a_;
					parity_overflow_result_ = overflow ? Flag::Overflow : 0;
					subtract_flag_ = Flag::Subtract;
					carry_result_ = uint8_t(result >> 8);
					half_carry_result_ = uint8_t(halfResult);
					set_did_compute_flags();
				} break;

				case MicroOp::Increment8: {
					const uint8_t value = *static_cast<uint8_t *>(operation->source);
					const int result = value + 1;

					// with an increment, overflow occurs if the sign changes from
					// positive to negative
					const int overflow = (value ^ result) & ~value;
					const int half_result = (value&0xf) + 1;

					*static_cast<uint8_t *>(operation->source) = uint8_t(result);

					// sign, zero and 5 & 3 are set directly from the result
					bit53_result_ = sign_result_ = zero_result_ = uint8_t(result);
					half_carry_result_ = uint8_t(half_result);
					parity_overflow_result_ = uint8_t(overflow >> 5);
					subtract_flag_ = 0;
					set_did_compute_flags();
				} break;

				case MicroOp::Decrement8: {
					const uint8_t value = *static_cast<uint8_t *>(operation->source);
					const int result = value - 1;

					// with a decrement, overflow occurs if the sign changes from
					// negative to positive
					const int overflow = (value ^ result) & value;
					const int half_result = (value&0xf) - 1;

					*static_cast<uint8_t *>(operation->source) = uint8_t(result);

					// sign, zero and 5 & 3 are set directly from the result
					bit53_result_ = sign_result_ = zero_result_ = uint8_t(result);
					half_carry_result_ = uint8_t(half_result);
					parity_overflow_result_ = uint8_t(overflow >> 5);
					subtract_flag_ = Flag::Subtract;
					set_did_compute_flags();
				} break;

				case MicroOp::DAA: {
					const int lowNibble = a_ & 0xf;
					const int highNibble = a_ >> 4;
					int amountToAdd = 0;

					if(carry_result_ & Flag::Carry) {
						amountToAdd = (lowNibble > 0x9 || (half_carry_result_ & Flag::HalfCarry)) ? 0x66 : 0x60;
					} else {
						if(half_carry_result_ & Flag::HalfCarry) {
							if(lowNibble > 0x9)
								amountToAdd = (highNibble > 0x8) ? 0x66 : 0x06;
							else
								amountToAdd = (highNibble > 0x9) ? 0x66 : 0x06;
						} else {
							if(lowNibble > 0x9)
								amountToAdd = (highNibble > 0x8) ? 0x66 : 0x06;
							else
								amountToAdd = (highNibble > 0x9) ? 0x60 : 0x00;
						}
					}

					if(!(carry_result_ & Flag::Carry)) {
						if(lowNibble > 0x9) {
							if(highNibble > 0x8) carry_result_ = Flag::Carry;
						} else {
							if(highNibble > 0x9) carry_result_ = Flag::Carry;
						}
					}

					if(subtract_flag_) {
						a_ -= amountToAdd;
						half_carry_result_ = ((half_carry_result_ & Flag::HalfCarry) && lowNibble < 0x6) ? Flag::HalfCarry : 0;
					} else {
						a_ += amountToAdd;
						half_carry_result_ = (lowNibble > 0x9) ? Flag::HalfCarry : 0;
					}

					sign_result_ = zero_result_ = bit53_result_ = a_;

					set_parity(a_);
					set_did_compute_flags();
				} break;

// MARK: - 16-bit arithmetic

				case MicroOp::ADD16: {
					memptr_.full = *static_cast<uint16_t *>(operation->destination);
					const uint16_t sourceValue = *static_cast<uint16_t *>(operation->source);
					const uint16_t destinationValue = memptr_.full;
					const int result = sourceValue + destinationValue;
					const int halfResult = (sourceValue&0xfff) + (destinationValue&0xfff);

					bit53_result_ = uint8_t(result >> 8);
					carry_result_ = uint8_t(result >> 16);
					half_carry_result_ = uint8_t(halfResult >> 8);
					subtract_flag_ = 0;
					set_did_compute_flags();

					*static_cast<uint16_t *>(operation->destination) = uint16_t(result);
					memptr_.full++;
				} break;

				case MicroOp::ADC16: {
					memptr_.full = *static_cast<uint16_t *>(operation->destination);
					const uint16_t sourceValue = *static_cast<uint16_t *>(operation->source);
					const uint16_t destinationValue = memptr_.full;
					const int result = sourceValue + destinationValue + (carry_result_ & Flag::Carry);
					const int halfResult = (sourceValue&0xfff) + (destinationValue&0xfff) + (carry_result_ & Flag::Carry);

					const int overflow = (result ^ destinationValue) & ~(destinationValue ^ sourceValue);

					bit53_result_	=
					sign_result_	= uint8_t(result >> 8);
					zero_result_	= uint8_t(result | sign_result_);
					subtract_flag_	= 0;
					carry_result_	= uint8_t(result >> 16);
					half_carry_result_ = uint8_t(halfResult >> 8);
					parity_overflow_result_ = uint8_t(overflow >> 13);
					set_did_compute_flags();

					*static_cast<uint16_t *>(operation->destination) = uint16_t(result);
					memptr_.full++;
				} break;

				case MicroOp::SBC16: {
					memptr_.full = *static_cast<uint16_t *>(operation->destination);
					const uint16_t sourceValue = *static_cast<uint16_t *>(operation->source);
					const uint16_t destinationValue = memptr_.full;
					const int result = destinationValue - sourceValue - (carry_result_ & Flag::Carry);
					const int halfResult = (destinationValue&0xfff) - (sourceValue&0xfff) - (carry_result_ & Flag::Carry);

					// subtraction, so parity rules are:
					// signs of operands were different,
					// sign of result is different
					const int overflow = (result ^ destinationValue) & (sourceValue ^ destinationValue);

					bit53_result_	=
					sign_result_	= uint8_t(result >> 8);
					zero_result_	= uint8_t(result | sign_result_);
					subtract_flag_	= Flag::Subtract;
					carry_result_	= uint8_t(result >> 16);
					half_carry_result_ = uint8_t(halfResult >> 8);
					parity_overflow_result_ = uint8_t(overflow >> 13);
					set_did_compute_flags();

					*static_cast<uint16_t *>(operation->destination) = uint16_t(result);
					memptr_.full++;
				} break;

// MARK: - Conditionals

#define decline_conditional()	\
	if(operation->source) {		\
		scheduled_program_counter_ = (MicroOp *)operation->source;	\
	} else {	\
		advance_operation();	\
	}

				case MicroOp::TestNZ:	if(!zero_result_)								{ decline_conditional(); }		break;
				case MicroOp::TestZ:	if(zero_result_)								{ decline_conditional(); }		break;
				case MicroOp::TestNC:	if(carry_result_ & Flag::Carry)					{ decline_conditional(); }		break;
				case MicroOp::TestC:	if(!(carry_result_ & Flag::Carry))				{ decline_conditional(); }		break;
				case MicroOp::TestPO:	if(parity_overflow_result_ & Flag::Parity)		{ decline_conditional(); }		break;
				case MicroOp::TestPE:	if(!(parity_overflow_result_ & Flag::Parity))	{ decline_conditional(); }		break;
				case MicroOp::TestP:	if(sign_result_ & Flag::Sign)					{ decline_conditional(); }		break;
				case MicroOp::TestM:	if(!(sign_result_ & Flag::Sign))				{ decline_conditional(); }		break;

#undef decline_conditional

// MARK: - Exchange

#define swap(a, b)	temp = a.full; a.full = b.full; b.full = temp;

				case MicroOp::ExDEHL: {
					uint16_t temp;
					swap(de_, hl_);
				} break;

				case MicroOp::ExAFAFDash: {
					const uint8_t a = a_;
					const uint8_t f = get_flags();
					set_flags(afDash_.halves.low);
					a_ = afDash_.halves.high;
					afDash_.halves.high = a;
					afDash_.halves.low = f;
				} break;

				case MicroOp::EXX: {
					uint16_t temp;
					swap(de_, deDash_);
					swap(bc_, bcDash_);
					swap(hl_, hlDash_);
				} break;

#undef swap

// MARK: - Repetition

#define REPEAT(test)	\
	if(test) {	\
		pc_.full -= 2;	\
		memptr_.full = pc_.full + 1;	\
	} else {	\
		advance_operation();		\
	}

#define LDxR_STEP(dir)	\
	bc_.full--;	\
	de_.full += dir;	\
	hl_.full += dir;	\
	const uint8_t sum = a_ + temp8_;	\
	bit53_result_ = uint8_t((sum&0x8) | ((sum & 0x02) << 4));	\
	subtract_flag_ = 0;	\
	half_carry_result_ = 0;	\
	parity_overflow_result_ = bc_.full ? Flag::Parity : 0;	\
	set_did_compute_flags();

				case MicroOp::LDDR: {
					LDxR_STEP(-1);
					REPEAT(bc_.full);
				} break;

				case MicroOp::LDIR: {
					LDxR_STEP(1);
					REPEAT(bc_.full);
				} break;

				case MicroOp::LDD: {
					LDxR_STEP(-1);
				} break;

				case MicroOp::LDI: {
					LDxR_STEP(1);
				} break;

#undef LDxR_STEP

#define CPxR_STEP(dir)		\
	hl_.full += dir;		\
	memptr_.full += dir;	\
	bc_.full--;				\
	\
	uint8_t result = a_ - temp8_;	\
	const uint8_t halfResult = (a_&0xf) - (temp8_&0xf);	\
	\
	parity_overflow_result_ =  bc_.full ? Flag::Parity : 0;	\
	half_carry_result_ = halfResult;	\
	subtract_flag_ = Flag::Subtract;	\
	sign_result_ = zero_result_ = result;	\
	\
	result -= (halfResult >> 4)&1;	\
	bit53_result_ = uint8_t((result&0x8) | ((result&0x2) << 4));	\
	set_did_compute_flags();

				case MicroOp::CPDR: {
					CPxR_STEP(-1);
					REPEAT(bc_.full && sign_result_);
				} break;

				case MicroOp::CPIR: {
					CPxR_STEP(1);
					REPEAT(bc_.full && sign_result_);
				} break;

				case MicroOp::CPD: {
					CPxR_STEP(-1);
				} break;

				case MicroOp::CPI: {
					CPxR_STEP(1);
				} break;

#undef CPxR_STEP

#undef REPEAT
#define REPEAT(test)	\
	if(test) {	\
		pc_.full -= 2;	\
	} else {	\
		advance_operation();		\
	}

#define INxR_STEP(dir)	\
	memptr_.full = uint16_t(bc_.full + dir);	\
	bc_.halves.high--;	\
	hl_.full += dir;	\
	\
	sign_result_ = zero_result_ = bit53_result_ = bc_.halves.high;	\
	subtract_flag_ = (temp8_ >> 6) & Flag::Subtract;	\
	\
	const int next_bc = bc_.halves.low + dir;	\
	int summation = temp8_ + (next_bc&0xff);	\
	\
	if(summation > 0xff) {	\
		carry_result_ = Flag::Carry;	\
		half_carry_result_ = Flag::HalfCarry;	\
	} else {	\
		carry_result_ = 0;	\
		half_carry_result_ = 0;	\
	}	\
	\
	summation = (summation&7) ^ bc_.halves.high;	\
	set_parity(summation);	\
	set_did_compute_flags();

				case MicroOp::INDR: {
					INxR_STEP(-1);
					REPEAT(bc_.halves.high);
				} break;

				case MicroOp::INIR: {
					INxR_STEP(1);
					REPEAT(bc_.halves.high);
				} break;

				case MicroOp::IND: {
					INxR_STEP(-1);
				} break;

				case MicroOp::INI: {
					INxR_STEP(1);
				} break;

#undef INxR_STEP

#define OUTxR_STEP(dir)	\
	bc_.halves.high--;	\
	memptr_.full = uint16_t(bc_.full + dir);	\
	hl_.full += dir;	\
	\
	sign_result_ = zero_result_ = bit53_result_ = bc_.halves.high;	\
	subtract_flag_ = (temp8_ >> 6) & Flag::Subtract;	\
	\
	int summation = temp8_ + hl_.halves.low;	\
	if(summation > 0xff) {	\
		carry_result_ = Flag::Carry;	\
		half_carry_result_ = Flag::HalfCarry;	\
	} else {	\
		carry_result_ = half_carry_result_ = 0;	\
	}	\
	\
	summation = (summation&7) ^ bc_.halves.high;	\
	set_parity(summation);	\
	set_did_compute_flags();

				case MicroOp::OUT_R:
					REPEAT(bc_.halves.high);
				break;

				case MicroOp::OUTD: {
					OUTxR_STEP(-1);
				} break;

				case MicroOp::OUTI: {
					OUTxR_STEP(1);
				} break;

#undef OUTxR_STEP

// MARK: - Bit Manipulation

				case MicroOp::BIT: {
					const uint8_t result = *static_cast<uint8_t *>(operation->source) & (1 << ((operation_ >> 3)&7));

					// Leak MEMPTR into bits 5 and 3 if this is either BIT n,(HL) or BIT n,(IX/IY+d).
					if(current_instruction_page_->is_indexed || ((operation_&0x07) == 6)) {
						bit53_result_ = memptr_.halves.high;
					} else {
						bit53_result_ = *static_cast<uint8_t *>(operation->source);
					}

					sign_result_ = zero_result_ = result;
					half_carry_result_ = Flag::HalfCarry;
					subtract_flag_ = 0;
					parity_overflow_result_ = result ? 0 : Flag::Parity;
					set_did_compute_flags();
				} break;

				case MicroOp::RES:
					*static_cast<uint8_t *>(operation->source) &= ~(1 << ((operation_ >> 3)&7));
				break;

				case MicroOp::SET:
					*static_cast<uint8_t *>(operation->source) |= (1 << ((operation_ >> 3)&7));
				break;

// MARK: - Rotation and shifting

#define set_rotate_flags()	\
	bit53_result_ = a_;	\
	carry_result_ = new_carry;	\
	subtract_flag_ = half_carry_result_ = 0;	\
	set_did_compute_flags();

				case MicroOp::RLA: {
					const uint8_t new_carry = a_ >> 7;
					a_ = uint8_t((a_ << 1) | (carry_result_ & Flag::Carry));
					set_rotate_flags();
				} break;

				case MicroOp::RRA: {
					const uint8_t new_carry = a_ & 1;
					a_ = uint8_t((a_ >> 1) | (carry_result_ << 7));
					set_rotate_flags();
				} break;

				case MicroOp::RLCA: {
					const uint8_t new_carry = a_ >> 7;
					a_ = uint8_t((a_ << 1) | new_carry);
					set_rotate_flags();
				} break;

				case MicroOp::RRCA: {
					const uint8_t new_carry = a_ & 1;
					a_ = uint8_t((a_ >> 1) | (new_carry << 7));
					set_rotate_flags();
				} break;

#undef set_rotate_flags

#define set_shift_flags()	\
	sign_result_ = zero_result_ = bit53_result_ = *static_cast<uint8_t *>(operation->source);	\
	set_parity(sign_result_);	\
	half_carry_result_ = 0;	\
	subtract_flag_ = 0;	\
	set_did_compute_flags();

				case MicroOp::RLC:
					carry_result_ = *static_cast<uint8_t *>(operation->source) >> 7;
					*static_cast<uint8_t *>(operation->source) = uint8_t((*static_cast<uint8_t *>(operation->source) << 1) | carry_result_);
					set_shift_flags();
				break;

				case MicroOp::RRC:
					carry_result_ = *static_cast<uint8_t *>(operation->source);
					*static_cast<uint8_t *>(operation->source) = uint8_t((*static_cast<uint8_t *>(operation->source) >> 1) | (carry_result_ << 7));
					set_shift_flags();
				break;

				case MicroOp::RL: {
					const uint8_t next_carry = *static_cast<uint8_t *>(operation->source) >> 7;
					*static_cast<uint8_t *>(operation->source) = uint8_t((*static_cast<uint8_t *>(operation->source) << 1) | (carry_result_ & Flag::Carry));
					carry_result_ = next_carry;
					set_shift_flags();
				} break;

				case MicroOp::RR: {
					const uint8_t next_carry = *static_cast<uint8_t *>(operation->source);
					*static_cast<uint8_t *>(operation->source) = uint8_t((*static_cast<uint8_t *>(operation->source) >> 1) | (carry_result_ << 7));
					carry_result_ = next_carry;
					set_shift_flags();
				} break;

				case MicroOp::SLA:
					carry_result_ = *static_cast<uint8_t *>(operation->source) >> 7;
					*static_cast<uint8_t *>(operation->source) = uint8_t(*static_cast<uint8_t *>(operation->source) << 1);
					set_shift_flags();
				break;

				case MicroOp::SRA:
					carry_result_ = *static_cast<uint8_t *>(operation->source);
					*static_cast<uint8_t *>(operation->source) = uint8_t((*static_cast<uint8_t *>(operation->source) >> 1) | (*static_cast<uint8_t *>(operation->source) & 0x80));
					set_shift_flags();
				break;

				case MicroOp::SLL:
					carry_result_ = *static_cast<uint8_t *>(operation->source) >> 7;
					*static_cast<uint8_t *>(operation->source) = uint8_t(*static_cast<uint8_t *>(operation->source) << 1) | 1;
					set_shift_flags();
				break;

				case MicroOp::SRL:
					carry_result_ = *static_cast<uint8_t *>(operation->source);
					*static_cast<uint8_t *>(operation->source) = uint8_t((*static_cast<uint8_t *>(operation->source) >> 1));
					set_shift_flags();
				break;

#undef set_shift_flags

#define set_decimal_rotate_flags()	\
	subtract_flag_ = 0;	\
	half_carry_result_ = 0;	\
	set_parity(a_);	\
	bit53_result_ = zero_result_ = sign_result_ = a_;	\
	set_did_compute_flags();

				case MicroOp::RRD: {
					memptr_.full = hl_.full + 1;
					const uint8_t low_nibble = a_ & 0xf;
					a_ = (a_ & 0xf0) | (temp8_ & 0xf);
					temp8_ = uint8_t((temp8_ >> 4) | (low_nibble << 4));
					set_decimal_rotate_flags();
				} break;

				case MicroOp::RLD: {
					memptr_.full = hl_.full + 1;
					const uint8_t low_nibble = a_ & 0xf;
					a_ = (a_ & 0xf0) | (temp8_ >> 4);
					temp8_ = uint8_t((temp8_ << 4) | low_nibble);
					set_decimal_rotate_flags();
				} break;

#undef set_decimal_rotate_flags


// MARK: - Interrupt state

				case MicroOp::EI:
					iff1_ = iff2_ = true;
					if(irq_line_) request_status_ |= Interrupt::IRQ;
				break;

				case MicroOp::DI:
					iff1_ = iff2_ = false;
					request_status_ &= ~Interrupt::IRQ;
				break;

				case MicroOp::IM:
					switch(operation_ & 0x18) {
						case 0x00:	interrupt_mode_ = 0;	break;
						case 0x08:	interrupt_mode_ = 0;	break;	// IM 0/1
						case 0x10:	interrupt_mode_ = 1;	break;
						case 0x18:	interrupt_mode_ = 2;	break;
					}
				break;

// MARK: - Input and Output

				case MicroOp::SetInFlags:
					subtract_flag_ = half_carry_result_ = 0;
					sign_result_ = zero_result_ = bit53_result_ = *static_cast<uint8_t *>(operation->source);
					set_parity(sign_result_);
					set_did_compute_flags();
					++memptr_.full;
				break;

				case MicroOp::SetOutFlags:
					memptr_.full = bc_.full + 1;
				break;

				case MicroOp::SetAFlags:
					subtract_flag_ = half_carry_result_ = 0;
					parity_overflow_result_ = iff2_ ? Flag::Parity : 0;
					sign_result_ = zero_result_ = bit53_result_ = a_;
					set_did_compute_flags();
				break;

				case MicroOp::SetZero:
					temp8_ = 0;
				break;

// MARK: - Special-case Flow

				case MicroOp::BeginIRQMode0:
					pc_increment_ = 0;			// deliberate fallthrough
				case MicroOp::BeginIRQ:
					iff2_ = iff1_ = false;
					request_status_ &= ~Interrupt::IRQ;
					temp16_.full = 0x38;
				break;

				case MicroOp::BeginNMI:
					iff2_ = iff1_;
					iff1_ = false;
					request_status_ &= ~Interrupt::IRQ;
				break;

				case MicroOp::JumpTo66:
					pc_.full = 0x66;
				break;

				case MicroOp::RETN:
					iff1_ = iff2_;
					if(irq_line_ && iff1_) request_status_ |= Interrupt::IRQ;
					memptr_ = pc_;
				break;

				case MicroOp::HALT:
					halt_mask_ = 0x00;
				break;

// MARK: - Interrupt handling

				case MicroOp::Reset:
					iff1_ = iff2_ = false;
					interrupt_mode_ = 0;
					pc_.full = 0;
					sp_.full = 0xffff;
					a_ = 0xff;
					set_flags(0xff);
					ir_.full = 0;
				break;

// MARK: - Internal bookkeeping

				case MicroOp::SetInstructionPage:
					current_instruction_page_ = (InstructionPage *)operation->source;
					scheduled_program_counter_ = current_instruction_page_->fetch_decode_execute_data;
				break;

				case MicroOp::CalculateIndexAddress:
					memptr_.full = uint16_t(*static_cast<uint16_t *>(operation->source) + int8_t(temp8_));
				break;

				case MicroOp::SetAddrAMemptr:
					memptr_.full = uint16_t(((*static_cast<uint16_t *>(operation->source) + 1)&0xff) + (a_ << 8));
				break;

				case MicroOp::IndexedPlaceHolder:
				return;
			}
#undef set_parity
		}

	}
}

template <	class T,
			bool uses_bus_request,
			bool uses_wait_line> void Processor <T, uses_bus_request, uses_wait_line>
				::set_bus_request_line(bool value) {
	assert(uses_bus_request);
	bus_request_line_ = value;
}

template <	class T,
			bool uses_bus_request,
			bool uses_wait_line> bool Processor <T, uses_bus_request, uses_wait_line>
				::get_bus_request_line() {
	return bus_request_line_;
}

template <	class T,
			bool uses_bus_request,
			bool uses_wait_line> void Processor <T, uses_bus_request, uses_wait_line>
				::set_wait_line(bool value) {
	assert(uses_wait_line);
	wait_line_ = value;
}

template <	class T,
			bool uses_bus_request,
			bool uses_wait_line> bool Processor <T, uses_bus_request, uses_wait_line>
				::get_wait_line() {
	return wait_line_;
}

#define isTerminal(n)	(n == MicroOp::MoveToNextProgram || n == MicroOp::DecodeOperation || n == MicroOp::DecodeOperationNoRChange)

template <	class T,
			bool uses_bus_request,
			bool uses_wait_line> void Processor <T, uses_bus_request, uses_wait_line>
				::assemble_page(InstructionPage &target, InstructionTable &table, bool add_offsets) {
	std::size_t number_of_micro_ops = 0;
	std::size_t lengths[256];

	// Count number of micro-ops required.
	for(int c = 0; c < 256; c++) {
		std::size_t length = 0;
		while(!isTerminal(table[c][length].type)) length++;
		length++;
		lengths[c] = length;
		number_of_micro_ops += length;
	}

	// Allocate a landing area.
	std::vector<std::size_t> operation_indices;
	target.all_operations.reserve(number_of_micro_ops);
	target.instructions.resize(256, nullptr);

	// Copy in all programs, recording where they go.
	std::size_t destination = 0;
	for(std::size_t c = 0; c < 256; c++) {
		operation_indices.push_back(target.all_operations.size());
		for(std::size_t t = 0; t < lengths[c];) {
			// Skip zero-length bus cycles.
			if(table[c][t].type == MicroOp::BusOperation && table[c][t].machine_cycle.length.as_integral() == 0) {
				t++;
				continue;
			}

			// Skip optional waits if this instance doesn't use the wait line.
			if(table[c][t].machine_cycle.was_requested && !uses_wait_line) {
				t++;
				continue;
			}

			// If an index placeholder is hit then drop it, and if offsets aren't being added,
			// then also drop the indexing that follows, which is assumed to be everything
			// up to and including the next ::CalculateIndexAddress. Coupled to the INDEX() macro.
			if(table[c][t].type == MicroOp::IndexedPlaceHolder) {
				t++;
				if(!add_offsets) {
					while(table[c][t].type != MicroOp::CalculateIndexAddress) t++;
					t++;
				}
			}
			target.all_operations.emplace_back(table[c][t]);
			destination++;
			t++;
		}
	}

	// Since the vector won't change again, it's now safe to set pointers.
	std::size_t c = 0;
	for(std::size_t index : operation_indices) {
		target.instructions[c] = &target.all_operations[index];
		c++;
	}
}

template <	class T,
			bool uses_bus_request,
			bool uses_wait_line> void Processor <T, uses_bus_request, uses_wait_line>
		::copy_program(const MicroOp *source, std::vector<MicroOp> &destination) {
	std::size_t length = 0;
	while(!isTerminal(source[length].type)) length++;
	std::size_t pointer = 0;
	while(true) {
		// TODO: This test is duplicated from assemble_page; can a better factoring be found?
		// Skip optional waits if this instance doesn't use the wait line.
		if(source[pointer].machine_cycle.was_requested && !uses_wait_line) {
			pointer++;
			continue;
		}

		destination.emplace_back(source[pointer]);
		if(isTerminal(source[pointer].type)) break;
		pointer++;
	}
}

#undef isTerminal

bool ProcessorBase::get_halt_line() {
	return halt_mask_ == 0x00;
}

void ProcessorBase::set_interrupt_line(bool value, HalfCycles offset) {
	if(irq_line_ == value) return;

	// IRQ requests are level triggered and masked.
	irq_line_ = value;
	if(irq_line_ && iff1_) {
		request_status_ |= Interrupt::IRQ;
	} else {
		request_status_ &= ~Interrupt::IRQ;
	}

	// If this change happened at least one cycle ago then: (i) we're promised that this is a machine
	// cycle per the contract on supplying an offset; and (ii) that means it happened before the lines
	// were sampled. So adjust the most recent sample.
	if(offset <= HalfCycles(-2)) {
		last_request_status_ = (last_request_status_ & ~Interrupt::IRQ) | (request_status_ & Interrupt::IRQ);
	}
}

bool ProcessorBase::get_interrupt_line() {
	return irq_line_;
}

void ProcessorBase::set_non_maskable_interrupt_line(bool value, HalfCycles offset) {
	// NMIs are edge triggered, so react to changes only, particularly changes to active.
	if(nmi_line_ != value) {
		nmi_line_ = value;
		if(value) {
			// request_status_ holds a bit mask of interrupt requests pending; since
			// this was a new transition, an NMI is now definitely pending.
			request_status_ |= Interrupt::NMI;

			// If the caller is indicating that this request actually happened in the
			// recent past, and it happened long enough ago that it should have been
			// spotted during this instruction* then change history to ensure that an
			// NMI happens next.
			//
			// * it actually doesn't matter what sort of bus cycle is currently ongoing;
			// either it is that which terminates a whole instruction, in which case this
			// test is correct, or it isn't, in which case setting request_status_ above
			// was sufficient and the below is merely redundant.
			if(offset <= HalfCycles(-2)) {
				last_request_status_ |= Interrupt::NMI;
			}
		}
	}
}

bool ProcessorBase::get_non_maskable_interrupt_line() {
	return nmi_line_;
}

void ProcessorBase::set_reset_line(bool value) {
	// Reset requests are level triggered and cannot be masked.
	if(value) request_status_ |= Interrupt::Reset;
	else request_status_ &= ~Interrupt::Reset;
}
