//
//  MFP68901.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/10/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "MFP68901.hpp"

#define LOG_PREFIX "[MFP] "
//#define NDEBUG
#include "../../Outputs/Log.hpp"

using namespace Motorola::MFP68901;

uint8_t MFP68901::read(int address) {
	address &= 0x1f;
	switch(address) {
		case 0x00:
			LOG("Read: general purpose IO " << PADHEX(2) << int(gpip_input_ | gpip_direction_));
		return gpip_input_ | gpip_direction_;
		case 0x01:
			LOG("Read: active edge " << PADHEX(2) << int(gpip_active_edge_));
		return gpip_active_edge_;
		case 0x02:
			LOG("Read: data direction "  << PADHEX(2) << int(gpip_direction_));
		return gpip_direction_;
		case 0x03:
			LOG("Read: interrupt enable A");
		return uint8_t(interrupt_enable_ >> 8);
		case 0x04:
			LOG("Read: interrupt enable B");
		return uint8_t(interrupt_enable_);
		case 0x05:
			LOG("Read: interrupt pending A");
		return uint8_t(interrupt_pending_ >> 8);
		case 0x06:
			LOG("Read: interrupt pending B");
		return uint8_t(interrupt_pending_);
		case 0x07:
			LOG("Read: interrupt in-service A");
		return uint8_t(interrupt_in_service_ >> 8);
		case 0x08:
			LOG("Read: interrupt in-service B");
		return uint8_t(interrupt_in_service_);
		case 0x09:
			LOG("Read: interrupt mask A");
		return uint8_t(interrupt_mask_ >> 8);
		case 0x0a:
			LOG("Read: interrupt mask B");
		return uint8_t(interrupt_mask_);
		case 0x0b:
			LOG("Read: vector");
		return interrupt_vector_;
		case 0x0c:		LOG("Read: timer A control");			break;
		case 0x0d:		LOG("Read: timer B control");			break;
		case 0x0e:		LOG("Read: timers C/D control");		break;
		case 0x0f:	case 0x10:	case 0x11:	case 0x12:
			return get_timer_data(address - 0xf);
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
	switch(address) {
		case 0x00:
			LOG("Write: general purpose IO " << PADHEX(2) << int(value));
			gpip_output_ = value;
		break;
		case 0x01:
			LOG("Write: active edge " << PADHEX(2) << int(value));
			gpip_active_edge_ = value;
			reevaluate_gpip_interrupts();
		break;
		case 0x02:
			LOG("Write: data direction " << PADHEX(2) << int(value));
			gpip_direction_ = value;
			reevaluate_gpip_interrupts();
		break;
		case 0x03:
			LOG("Write: interrupt enable A " << PADHEX(2) << int(value));
			interrupt_enable_ = (interrupt_enable_ & 0x00ff) | (value << 8);
			update_interrupts();
		break;
		case 0x04:
			LOG("Write: interrupt enable B " << PADHEX(2) << int(value));
			interrupt_enable_ = (interrupt_enable_ & 0xff00) | value;
			update_interrupts();
		break;
		case 0x05:
			LOG("Write: interrupt pending A " << PADHEX(2) << int(value));
			interrupt_pending_ = (interrupt_pending_ & 0x00ff) | (value << 8);
			interrupt_in_service_ &= 0x00ff | (value << 8);
			update_interrupts();
		break;
		case 0x06:
			LOG("Write: interrupt pending B " << PADHEX(2) << int(value));
			interrupt_pending_ = (interrupt_pending_ & 0xff00) | value;
			interrupt_in_service_ &= 0xff00 | value;
			update_interrupts();
		break;
		case 0x07:		LOG("Write: interrupt in-service A (no-op?) " << PADHEX(2) << int(value));	break;
		case 0x08:		LOG("Write: interrupt in-service B (no-op?) " << PADHEX(2) << int(value));	break;
		case 0x09:
			LOG("Write: interrupt mask A " << PADHEX(2) << int(value));
			interrupt_mask_ = (interrupt_mask_ & 0x00ff) | (value << 8);
			update_interrupts();
		break;
		case 0x0a:
			LOG("Write: interrupt mask B " << PADHEX(2) << int(value));
			interrupt_mask_ = (interrupt_mask_ & 0xff00) | value;
			update_interrupts();
		break;
		case 0x0b:
			LOG("Write: vector " << PADHEX(2) << int(value));
			interrupt_vector_ = value;
		break;
		case 0x0c:
		case 0x0d: {
			const auto timer = address - 0xc;
			const bool reset = value & 0x10;
			switch(value & 0xf) {
				case 0x0:	set_timer_mode(timer, TimerMode::Stopped, 0, reset);		break;
				case 0x1:	set_timer_mode(timer, TimerMode::Delay, 4, reset);			break;
				case 0x2:	set_timer_mode(timer, TimerMode::Delay, 10, reset);			break;
				case 0x3:	set_timer_mode(timer, TimerMode::Delay, 16, reset);			break;
				case 0x4:	set_timer_mode(timer, TimerMode::Delay, 50, reset);			break;
				case 0x5:	set_timer_mode(timer, TimerMode::Delay, 64, reset);			break;
				case 0x6:	set_timer_mode(timer, TimerMode::Delay, 100, reset);		break;
				case 0x7:	set_timer_mode(timer, TimerMode::Delay, 200, reset);		break;
				case 0x8:	set_timer_mode(timer, TimerMode::EventCount, 0, reset);		break;
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
			switch(value & 7) {
				case 0:	set_timer_mode(3, TimerMode::Stopped, 0, false);	break;
				case 1:	set_timer_mode(3, TimerMode::Delay, 4, false);		break;
				case 2:	set_timer_mode(3, TimerMode::Delay, 10, false);		break;
				case 3:	set_timer_mode(3, TimerMode::Delay, 16, false);		break;
				case 4:	set_timer_mode(3, TimerMode::Delay, 50, false);		break;
				case 5:	set_timer_mode(3, TimerMode::Delay, 64, false);		break;
				case 6:	set_timer_mode(3, TimerMode::Delay, 100, false);	break;
				case 7:	set_timer_mode(3, TimerMode::Delay, 200, false);	break;
			}
			switch((value >> 4) & 7) {
				case 0:	set_timer_mode(2, TimerMode::Stopped, 0, false);	break;
				case 1:	set_timer_mode(2, TimerMode::Delay, 4, false);		break;
				case 2:	set_timer_mode(2, TimerMode::Delay, 10, false);		break;
				case 3:	set_timer_mode(2, TimerMode::Delay, 16, false);		break;
				case 4:	set_timer_mode(2, TimerMode::Delay, 50, false);		break;
				case 5:	set_timer_mode(2, TimerMode::Delay, 64, false);		break;
				case 6:	set_timer_mode(2, TimerMode::Delay, 100, false);	break;
				case 7:	set_timer_mode(2, TimerMode::Delay, 200, false);	break;
			}
		break;
		case 0x0f:	case 0x10:	case 0x11:	case 0x12:
			set_timer_data(address - 0xf, value);
		break;
		case 0x13:		LOG("Write: sync character generator");	break;
		case 0x14:		LOG("Write: USART control");			break;
		case 0x15:		LOG("Write: receiver status");			break;
		case 0x16:		LOG("Write: transmitter status");		break;
		case 0x17:		LOG("Write: USART data");				break;
	}
}

void MFP68901::run_for(HalfCycles time) {
	cycles_left_ += time;

	// TODO: this is the stupidest possible implementation. Improve.
	int cycles = cycles_left_.flush<Cycles>().as_int();
	while(cycles--) {
		for(int c = 0; c < 4; ++c) {
			if(timers_[c].mode >= TimerMode::Delay) {
				--timers_[c].divisor;
				if(!timers_[c].divisor) {
					timers_[c].divisor = timers_[c].prescale;
					decrement_timer(c);
				}
			}
		}
	}
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
		decrement_timer(channel);
	}
}

void MFP68901::decrement_timer(int timer) {
	--timers_[timer].value;
	if(!timers_[timer].value) {
		// TODO: interrupt. Reload, possibly.
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
	const uint8_t gpip_state = gpip_input_ ^ gpip_active_edge_;

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
	interrupt_in_service_ |= interrupt;
	update_interrupts();
}

void MFP68901::end_interrupts(int interrupt) {
	interrupt_in_service_ &= ~interrupt;
	update_interrupts();
}

void MFP68901::update_interrupts() {
	const bool old_interrupt_line = interrupt_line_;
	interrupt_pending_ = interrupt_in_service_ & interrupt_enable_;
	interrupt_line_ = interrupt_pending_ & interrupt_mask_;

	if(interrupt_delegate_ && interrupt_line_ != old_interrupt_line) {
		interrupt_delegate_->mfp68901_did_change_interrupt_status(this);
	}
}

bool MFP68901::get_interrupt_line() {
	return interrupt_line_;
}

uint8_t MFP68901::acknowledge_interrupt() {
	uint8_t selected_interrupt = 15;
	uint16_t interrupt_mask = 0x8000;
	while(!(interrupt_pending_ & interrupt_mask) && interrupt_mask) {
		interrupt_mask >>= 1;
		--selected_interrupt;
	}
	end_interrupts(interrupt_mask);
	return (interrupt_vector_ & 0xf0) | selected_interrupt;
}

void MFP68901::set_interrupt_delegate(InterruptDelegate *delegate) {
	interrupt_delegate_ = delegate;
}
