//
//  MFP68901.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/10/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "MFP68901.hpp"

#include <cstring>

#define LOG_PREFIX "[MFP] "
//#define NDEBUG
#include "../../Outputs/Log.hpp"

using namespace Motorola::MFP68901;

ClockingHint::Preference MFP68901::preferred_clocking() {
	// Rule applied: if any timer is actively running and permitted to produce an
	// interrupt, request real-time running.
	return
		(timers_[0].mode >= TimerMode::Delay && interrupt_enable_&Interrupt::TimerA) ||
		(timers_[1].mode >= TimerMode::Delay && interrupt_enable_&Interrupt::TimerB) ||
		(timers_[2].mode >= TimerMode::Delay && interrupt_enable_&Interrupt::TimerC) ||
		(timers_[3].mode >= TimerMode::Delay && interrupt_enable_&Interrupt::TimerD)
			? ClockingHint::Preference::RealTime : ClockingHint::Preference::JustInTime;
}

uint8_t MFP68901::read(int address) {
	address &= 0x1f;

	// Interrupt block: various bits of state can be read, all passively.
	if(address >= 0x03 && address <= 0x0b) {
		const int shift = (address&1) << 3;
		switch(address) {
			case 0x03:	case 0x04:	return uint8_t(interrupt_enable_ >> shift);
			case 0x05:	case 0x06:	return uint8_t(interrupt_pending_ >> shift);
			case 0x07:	case 0x08:	return uint8_t(interrupt_in_service_ >> shift);
			case 0x09:	case 0x0a:	return uint8_t(interrupt_mask_ >> shift);
			case 0x0b:	return interrupt_vector_;

			default: break;
		}
	}

	switch(address) {
		// GPIP block: input, and configured active edge and direction values.
		case 0x00:	return (gpip_input_ & ~gpip_direction_) | (gpip_output_ & gpip_direction_);
		case 0x01:	return gpip_active_edge_;
		case 0x02:	return gpip_direction_;

		/* Interrupt block dealt with above. */
		default: break;

		// Timer block: read back A, B and C/D control, and read current timer values.
		case 0x0c:	case 0x0d:	return timer_ab_control_[address - 0xc];
		case 0x0e:				return timer_cd_control_;
		case 0x0f:	case 0x10:
		case 0x11:	case 0x12:	return get_timer_data(address - 0xf);

		// USART block: TODO.
		case 0x13:		LOG("Read: sync character generator");	break;
		case 0x14:		LOG("Read: USART control");				break;
		case 0x15:		LOG("Read: receiver status");			break;
		case 0x16:		LOG("Read: transmitter status");		break;
		case 0x17:		LOG("Read: USART data");				break;
	}
	return 0x00;
}

void MFP68901::write(int address, uint8_t value) {
	address &= 0x1f;

	// Interrupt block: enabled and masked interrupts can be set; pending and in-service interrupts can be masked.
	if(address >= 0x03 && address <= 0x0b) {
		const int shift = (address&1) << 3;
		const int preserve = 0xff00 >> shift;
		const int word_value = value << shift;

		switch(address) {
			default: break;
			case 0x03: case 0x04:	// Adjust enabled interrupts; disabled ones also cease to be pending.
				interrupt_enable_ = (interrupt_enable_ & preserve) | word_value;
				interrupt_pending_ &= interrupt_enable_;
			break;
			case 0x05: case 0x06:	// Resolve pending interrupts.
				interrupt_pending_ &= (preserve | word_value);
			break;
			case 0x07: case 0x08:	// Resolve in-service interrupts.
				interrupt_in_service_ &= (preserve | word_value);
			break;
			case 0x09: case 0x0a:	// Adjust interrupt mask.
				interrupt_mask_ = (interrupt_mask_ & preserve) | word_value;
			break;
			case 0x0b:				// Set the interrupt vector, possibly changing end-of-interrupt mode.
				interrupt_vector_ = value;

				// If automatic end-of-interrupt mode has now been enabled, clear
				// the in-process mask and re-evaluate.
				if(interrupt_vector_ & 0x08) return;
				interrupt_in_service_ = 0;
			break;
		}

		// Whatever just happened may have affected the state of the interrupt line.
		update_interrupts();
		update_clocking_observer();
		return;
	}

	const int timer_prescales[] = {
		1, 4, 10, 16, 50, 64, 100, 200
	};

	switch(address) {
		// GPIP block: output and configuration of active edge and direction values.
		case 0x00:
			gpip_output_ = value;
		break;
		case 0x01:
			gpip_active_edge_ = value;
			reevaluate_gpip_interrupts();
		break;
		case 0x02:
			gpip_direction_ = value;
			reevaluate_gpip_interrupts();
		break;

		/* Interrupt block dealt with above. */
		default: break;

		// Timer block.
		case 0x0c:
		case 0x0d: {
			const auto timer = address - 0xc;
			const bool reset = value & 0x10;
			timer_ab_control_[timer] = value;
			switch(value & 0xf) {
				case 0x0:	set_timer_mode(timer, TimerMode::Stopped, 1, reset);		break;
				case 0x1:	set_timer_mode(timer, TimerMode::Delay, 4, reset);			break;
				case 0x2:	set_timer_mode(timer, TimerMode::Delay, 10, reset);			break;
				case 0x3:	set_timer_mode(timer, TimerMode::Delay, 16, reset);			break;
				case 0x4:	set_timer_mode(timer, TimerMode::Delay, 50, reset);			break;
				case 0x5:	set_timer_mode(timer, TimerMode::Delay, 64, reset);			break;
				case 0x6:	set_timer_mode(timer, TimerMode::Delay, 100, reset);		break;
				case 0x7:	set_timer_mode(timer, TimerMode::Delay, 200, reset);		break;
				case 0x8:	set_timer_mode(timer, TimerMode::EventCount, 1, reset);		break;
				case 0x9:	set_timer_mode(timer, TimerMode::PulseWidth, 4, reset);		break;
				case 0xa:	set_timer_mode(timer, TimerMode::PulseWidth, 10, reset);	break;
				case 0xb:	set_timer_mode(timer, TimerMode::PulseWidth, 16, reset);	break;
				case 0xc:	set_timer_mode(timer, TimerMode::PulseWidth, 50, reset);	break;
				case 0xd:	set_timer_mode(timer, TimerMode::PulseWidth, 64, reset);	break;
				case 0xe:	set_timer_mode(timer, TimerMode::PulseWidth, 100, reset);	break;
				case 0xf:	set_timer_mode(timer, TimerMode::PulseWidth, 200, reset);	break;
			}
		} break;
		case 0x0e:
			timer_cd_control_ = value;
			set_timer_mode(3, (value & 7) ? TimerMode::Delay : TimerMode::Stopped, timer_prescales[value & 7], false);
			set_timer_mode(2, ((value >> 4) & 7) ? TimerMode::Delay : TimerMode::Stopped, timer_prescales[(value >> 4) & 7], false);
		break;
		case 0x0f:	case 0x10:	case 0x11:	case 0x12:
			set_timer_data(address - 0xf, value);
		break;

		// USART block: TODO.
		case 0x13:		LOG("Write: sync character generator");	break;
		case 0x14:		LOG("Write: USART control");			break;
		case 0x15:		LOG("Write: receiver status");			break;
		case 0x16:		LOG("Write: transmitter status");		break;
		case 0x17:		LOG("Write: USART data");				break;
	}

	update_clocking_observer();
}

void MFP68901::run_for(HalfCycles time) {
	cycles_left_ += time;

	// TODO: this is the stupidest possible implementation. Improve.
	int cycles = int(cycles_left_.flush<Cycles>().as_integral());
	while(cycles--) {
		for(int c = 0; c < 4; ++c) {
			if(timers_[c].mode >= TimerMode::Delay) {
				--timers_[c].divisor;
				if(!timers_[c].divisor) {
					timers_[c].divisor = timers_[c].prescale;
					decrement_timer(c, 1);
				}
			}
		}
	}
//	const int cycles = int(cycles_left_.flush<Cycles>().as_integral());
//	for(int c = 0; c < 4; ++c) {
//		if(timers_[c].mode >= TimerMode::Delay) {
//			const int dividend = (cycles + timers_[c].prescale - timers_[c].divisor);
//			const int decrements = dividend / timers_[c].prescale;
//			timers_[c].divisor = timers_[c].prescale - (dividend % timers_[c].prescale);
//			if(decrements) decrement_timer(c, decrements);
//		}
//	}
}

HalfCycles MFP68901::get_next_sequence_point() {
	return HalfCycles(-1);
}

// MARK: - Timers

void MFP68901::set_timer_mode(int timer, TimerMode mode, int prescale, bool reset_timer) {
	timers_[timer].mode = mode;
	timers_[timer].prescale = prescale;
	if(reset_timer) {
		timers_[timer].divisor = prescale;
		timers_[timer].value = timers_[timer].reload_value;
	}
}

void MFP68901::set_timer_data(int timer, uint8_t value) {
	if(timers_[timer].mode == TimerMode::Stopped) {
		timers_[timer].value = value;
	}
	timers_[timer].reload_value = value;
}

uint8_t MFP68901::get_timer_data(int timer) {
	return timers_[timer].value;
}

void MFP68901::set_timer_event_input(int channel, bool value) {
	if(timers_[channel].event_input == value) return;

	timers_[channel].event_input = value;
	if(timers_[channel].mode == TimerMode::EventCount && !value) {	/* TODO: which edge is counted? "as defined by the associated Interrupt Channel’s edge bit"?  */
		decrement_timer(channel, 1);
	}
}

void MFP68901::decrement_timer(int timer, int amount) {
	while(amount--) {
		--timers_[timer].value;
		if(timers_[timer].value < 1) {
			switch(timer) {
				case 0: begin_interrupts(Interrupt::TimerA);	break;
				case 1: begin_interrupts(Interrupt::TimerB);	break;
				case 2: begin_interrupts(Interrupt::TimerC);	break;
				case 3: begin_interrupts(Interrupt::TimerD);	break;
			}
			if(timers_[timer].mode == TimerMode::Delay) {
				timers_[timer].value += timers_[timer].reload_value;	// TODO: properly.
			}
		}
	}
}

// MARK: - GPIP
void MFP68901::set_port_input(uint8_t input) {
	gpip_input_ = input;
	reevaluate_gpip_interrupts();
}

uint8_t MFP68901::get_port_output() {
	return 0xff;
}

void MFP68901::reevaluate_gpip_interrupts() {
	const uint8_t gpip_state = (gpip_input_ & ~gpip_direction_) ^ gpip_active_edge_;

	// An interrupt is detected on any falling edge.
	const uint8_t new_interrupt_mask = (gpip_state ^ gpip_interrupt_state_) & gpip_interrupt_state_;
	if(new_interrupt_mask) {
		begin_interrupts(
			(new_interrupt_mask & 0x0f) |
			((new_interrupt_mask & 0x30) << 2) |
			((new_interrupt_mask & 0xc0) << 8)
		);
	}
	gpip_interrupt_state_ = gpip_state;
}

// MARK: - Interrupts

void MFP68901::begin_interrupts(int interrupt) {
	interrupt_pending_ |= interrupt & interrupt_enable_;
	update_interrupts();
}

void MFP68901::end_interrupts(int interrupt) {
	interrupt_pending_ &= ~interrupt;
	update_interrupts();
}

void MFP68901::update_interrupts() {
	const auto old_interrupt_line = interrupt_line_;
	const auto firing_interrupts = interrupt_pending_ & interrupt_mask_;

	if(!firing_interrupts) {
		interrupt_line_ = false;
	} else {
		if(interrupt_vector_ & 0x8) {
			// Software interrupt mode: permit only if neither this interrupt
			// nor a higher interrupt is currently in service.
			const int highest_bit = msb16(firing_interrupts);
			interrupt_line_ = !(interrupt_in_service_ & ~(highest_bit + highest_bit - 1));
		} else {
			// Auto-interrupt mode; just signal.
			interrupt_line_ = true;
		}
	}

	// Update the delegate if necessary.
	if(interrupt_delegate_ && interrupt_line_ != old_interrupt_line) {
		if(interrupt_line_)
			LOG("Generating interrupt: " << std::hex << interrupt_pending_ << " / " << std::hex << interrupt_mask_ << " : " << std::hex << interrupt_in_service_);
		interrupt_delegate_->mfp68901_did_change_interrupt_status(this);
	}
}

bool MFP68901::get_interrupt_line() {
	return interrupt_line_;
}

int MFP68901::acknowledge_interrupt() {
	if(!(interrupt_pending_ & interrupt_mask_)) {
		return NoAcknowledgement;
	}

	const int mask = msb16(interrupt_pending_ & interrupt_mask_);

	// Clear the pending bit regardless.
	interrupt_pending_ &= ~mask;

	// If this is software interrupt mode, set the in-service bit.
	if(interrupt_vector_ & 0x8) {
		interrupt_in_service_ |= mask;
	}

	update_interrupts();

	int selected = 0;
	while((1 << selected) != mask) ++selected;
	LOG("Interrupt acknowledged: " << selected);
	return (interrupt_vector_ & 0xf0) | uint8_t(selected);
}

void MFP68901::set_interrupt_delegate(InterruptDelegate *delegate) {
	interrupt_delegate_ = delegate;
}
