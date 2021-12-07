//
//  DiskII.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/04/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#include "DiskII.hpp"

#include <cstdio>
#include <cstring>

using namespace Apple;

namespace  {
	const uint8_t input_command = 0x4;	// i.e. Q6
	const uint8_t input_mode = 0x8;		// i.e. Q7
	const uint8_t input_flux = 0x1;
}

DiskII::DiskII(int clock_rate) :
	clock_rate_(clock_rate),
	inputs_(input_command),
	drives_{
		Storage::Disk::Drive{clock_rate, 300, 1},
		Storage::Disk::Drive{clock_rate, 300, 1}
	}
{
	drives_[0].set_clocking_hint_observer(this);
	drives_[1].set_clocking_hint_observer(this);
	drives_[active_drive_].set_event_delegate(this);
}

void DiskII::set_control(Control control, bool on) {
	int previous_stepper_mask = stepper_mask_;
	switch(control) {
		case Control::P0: stepper_mask_ = (stepper_mask_ & 0xe) | (on ? 0x1 : 0x0);	break;
		case Control::P1: stepper_mask_ = (stepper_mask_ & 0xd) | (on ? 0x2 : 0x0);	break;
		case Control::P2: stepper_mask_ = (stepper_mask_ & 0xb) | (on ? 0x4 : 0x0);	break;
		case Control::P3: stepper_mask_ = (stepper_mask_ & 0x7) | (on ? 0x8 : 0x0);	break;

		case Control::Motor:
			motor_is_enabled_ = on;
			drives_[active_drive_].set_motor_on(on);
		return;
	}

	// If the stepper magnet selections have changed, and any is on, see how
	// that moves the head.
	if(previous_stepper_mask ^ stepper_mask_ && stepper_mask_) {
		// Convert from a representation of bits set to the centre of pull.
		int direction = 0;
		if(stepper_mask_&1) direction += (((stepper_position_ - 0) + 4)&7) - 4;
		if(stepper_mask_&2) direction += (((stepper_position_ - 2) + 4)&7) - 4;
		if(stepper_mask_&4) direction += (((stepper_position_ - 4) + 4)&7) - 4;
		if(stepper_mask_&8) direction += (((stepper_position_ - 6) + 4)&7) - 4;
		const int bits_set = (stepper_mask_&1) + ((stepper_mask_ >> 1)&1) + ((stepper_mask_ >> 2)&1) + ((stepper_mask_ >> 3)&1);
		direction /= bits_set;

		// Compare to the stepper position to decide whether that pulls in the current cog notch,
		// or grabs a later one.
		drives_[active_drive_].step(Storage::Disk::HeadPosition(-direction, 4));
		stepper_position_ = (stepper_position_ - direction + 8) & 7;
	}
}

void DiskII::select_drive(int drive) {
	if((drive&1) == active_drive_) return;

	drives_[active_drive_].set_event_delegate(this);
	drives_[active_drive_^1].set_event_delegate(nullptr);

	drives_[active_drive_].set_motor_on(false);
	active_drive_ = drive & 1;
	drives_[active_drive_].set_motor_on(motor_is_enabled_);
}

// The read pulse is controlled by a special IC that outputs a 1us pulse for every field reversal on the disk.

void DiskII::run_for(const Cycles cycles) {
	if(preferred_clocking() == ClockingHint::Preference::None) return;

	auto integer_cycles = cycles.as_integral();
	while(integer_cycles--) {
		const int address = (state_ & 0xf0) | inputs_ | ((shift_register_&0x80) >> 6);
		if(flux_duration_) {
			--flux_duration_;
			if(!flux_duration_) inputs_ |= input_flux;
		}
		state_ = state_machine_[size_t(address)];
		switch(state_ & 0xf) {
			default:	shift_register_ = 0;										break;	// clear
			case 0x8:																break;	// nop

			case 0x9:	shift_register_ = uint8_t(shift_register_ << 1);			break;	// shift left, bringing in a zero
			case 0xd:	shift_register_ = uint8_t((shift_register_ << 1) | 1);		break;	// shift left, bringing in a one

			case 0xa:	// shift right, bringing in write protected status
				shift_register_ = (shift_register_ >> 1) | (is_write_protected() ? 0x80 : 0x00);

				// If the controller is in the sense write protect loop but the register will never change,
				// short circuit further work and return now.
				if(shift_register_ == (is_write_protected() ? 0xff : 0x00)) {
					if(!drive_is_sleeping_[0]) drives_[0].run_for(Cycles(integer_cycles));
					if(!drive_is_sleeping_[1]) drives_[1].run_for(Cycles(integer_cycles));
					decide_clocking_preference();
					return;
				}
			break;
			case 0xb:	shift_register_ = data_input_;								break;	// load data register from data bus
		}

		// Currently writing?
		if(inputs_&input_mode) {
			// state_ & 0x80 should be the current level sent to the disk;
			// therefore transitions in that bit should become flux transitions
			drives_[active_drive_].write_bit(!!((state_ ^ address) & 0x80));
		}

		// TODO: surely there's a less heavyweight solution than inline updates?
		if(!drive_is_sleeping_[0]) drives_[0].run_for(Cycles(1));
		if(!drive_is_sleeping_[1]) drives_[1].run_for(Cycles(1));
	}

	// Per comp.sys.apple2.programmer there is a delay between the controller
	// motor switch being flipped and the drive motor actually switching off.
	// This models that, accepting overrun as a risk.
	if(motor_off_time_ >= 0) {
		motor_off_time_ -= cycles.as_integral();
		if(motor_off_time_ < 0) {
			set_control(Control::Motor, false);
		}
	}
	decide_clocking_preference();
}

void DiskII::decide_clocking_preference() {
	ClockingHint::Preference prior_preference = clocking_preference_;

	// If in read mode, clocking is either:
	//
	//	just-in-time, if drives are running or the shift register has any 1s in it and shifting may occur, or a flux event hasn't yet passed; or
	//	none, given that drives are not running, the shift register has already emptied or stopped and there's no flux about to be received.
	if(!(inputs_ & ~input_flux)) {
		const bool is_stuck_at_nop =
			!flux_duration_ && state_machine_[(state_ & 0xf0) | inputs_ | ((shift_register_&0x80) >> 6)] == state_  && (state_ &0xf) == 0x8;

		clocking_preference_ =
			(drive_is_sleeping_[0] && drive_is_sleeping_[1] && (!shift_register_ || is_stuck_at_nop) && (inputs_&input_flux))
				? ClockingHint::Preference::None : ClockingHint::Preference::JustInTime;
	}

	// If in writing mode, clocking is real time.
	if(inputs_ & input_mode) {
		clocking_preference_ = ClockingHint::Preference::RealTime;
	}

	// If in sense-write-protect mode, clocking is just-in-time if the shift register hasn't yet filled with the value that
	// corresponds to the current write protect status. Otherwise it is none.
	if((inputs_ & ~input_flux) == input_command) {
		clocking_preference_ = (shift_register_ == (is_write_protected() ? 0xff : 0x00)) ? ClockingHint::Preference::None : ClockingHint::Preference::JustInTime;
	}

	// Announce a change if there was one.
	if(prior_preference != clocking_preference_)
		update_clocking_observer();
}

bool DiskII::is_write_protected() {
	return !!(stepper_mask_ & 2) | drives_[active_drive_].get_is_read_only();
}

void DiskII::set_state_machine(const std::vector<uint8_t> &state_machine) {
	/*
		An unadulterated P6 ROM read returns values with an address formed as:

			state b0, state b2, state b3, pulse, Q7, Q6, shift, state b1

		... and has the top nibble of each value stored in the ROM reflected.
		Beneath Apple Pro-DOS uses a different order and several of the
		online copies are reformatted into that order.

		So the code below remaps into Beneath Apple Pro-DOS order if the
		supplied state machine isn't already in that order.
	*/

	if(state_machine[0] != 0x18) {
		for(size_t source_address = 0; source_address < 256; ++source_address) {
			// Remap into Beneath Apple Pro-DOS address form.
			const size_t destination_address =
				((source_address&0x20) ? 0x80 : 0x00) |
				((source_address&0x40) ? 0x40 : 0x00) |
				((source_address&0x01) ? 0x20 : 0x00) |
				((source_address&0x80) ? 0x10 : 0x00) |
				((source_address&0x08) ? 0x08 : 0x00) |
				((source_address&0x04) ? 0x04 : 0x00) |
				((source_address&0x02) ? 0x02 : 0x00) |
				((source_address&0x10) ? 0x01 : 0x00);

			// Store.
			const uint8_t source_value = state_machine[source_address];
			state_machine_[destination_address] =
				((source_value & 0x80) ? 0x10 : 0x0) |
				((source_value & 0x40) ? 0x20 : 0x0) |
				((source_value & 0x20) ? 0x40 : 0x0) |
				((source_value & 0x10) ? 0x80 : 0x0) |
				(source_value & 0x0f);
		}
	} else {
		for(size_t source_address = 0; source_address < 256; ++source_address) {
			// Reshuffle ordering of bytes only, to retain indexing by the high nibble.
			const size_t destination_address =
				((source_address&0x80) ? 0x80 : 0x00) |
				((source_address&0x40) ? 0x40 : 0x00) |
				((source_address&0x01) ? 0x20 : 0x00) |
				((source_address&0x20) ? 0x10 : 0x00) |
				((source_address&0x08) ? 0x08 : 0x00) |
				((source_address&0x04) ? 0x04 : 0x00) |
				((source_address&0x02) ? 0x02 : 0x00) |
				((source_address&0x10) ? 0x01 : 0x00);

			state_machine_[destination_address] = state_machine[source_address];
		}
	}
}

void DiskII::set_disk(const std::shared_ptr<Storage::Disk::Disk> &disk, int drive) {
	drives_[drive].set_disk(disk);
}

void DiskII::process_event(const Storage::Disk::Drive::Event &event) {
	if(event.type == Storage::Disk::Track::Event::FluxTransition) {
		inputs_ &= ~input_flux;
		flux_duration_ = 2;	// Upon detection of a flux transition, the flux flag should stay set for 1us. Emulate that as two cycles.
		decide_clocking_preference();
	}
}

void DiskII::set_component_prefers_clocking(ClockingHint::Source *, ClockingHint::Preference) {
	drive_is_sleeping_[0] = drives_[0].preferred_clocking() == ClockingHint::Preference::None;
	drive_is_sleeping_[1] = drives_[1].preferred_clocking() == ClockingHint::Preference::None;
	decide_clocking_preference();
}

ClockingHint::Preference DiskII::preferred_clocking() const {
	return clocking_preference_;
}

void DiskII::set_data_input(uint8_t input) {
	data_input_ = input;
}

int DiskII::read_address(int address) {
	switch(address & 0xf) {
		default:
		case 0x0:	set_control(Control::P0, false);	break;
		case 0x1:	set_control(Control::P0, true);		break;
		case 0x2:	set_control(Control::P1, false);	break;
		case 0x3:	set_control(Control::P1, true);		break;
		case 0x4:	set_control(Control::P2, false);	break;
		case 0x5:	set_control(Control::P2, true);		break;
		case 0x6:	set_control(Control::P3, false);	break;
		case 0x7:	set_control(Control::P3, true);		break;

		case 0x8:
			shift_register_ = 0;
			motor_off_time_ = clock_rate_;
		break;
		case 0x9:
			set_control(Control::Motor, true);
			motor_off_time_ = -1;
		break;

		case 0xa:	select_drive(0);					break;
		case 0xb:	select_drive(1);					break;

		case 0xc:	inputs_ &= ~input_command;			break;
		case 0xd:	inputs_ |= input_command;			break;
		case 0xe:
			if(inputs_ & input_mode)
				drives_[active_drive_].end_writing();
			inputs_ &= ~input_mode;
		break;
		case 0xf:
			if(!(inputs_ & input_mode))
				drives_[active_drive_].begin_writing(Storage::Time(1, int(clock_rate_)), false);
			inputs_ |= input_mode;
		break;
	}
	decide_clocking_preference();
	return (address & 1) ? 0xff : shift_register_;
}

void DiskII::set_activity_observer(Activity::Observer *observer) {
	drives_[0].set_activity_observer(observer, "Drive 1", true);
	drives_[1].set_activity_observer(observer, "Drive 2", true);
}

Storage::Disk::Drive &DiskII::get_drive(int index) {
	return drives_[index];
}
