//
//  DiskII.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/04/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "DiskII.hpp"

#include <cstdio>

using namespace Apple;

namespace  {
	const uint8_t input_command = 0x1;	// i.e. Q6
	const uint8_t input_mode = 0x2;		// i.e. Q7
	const uint8_t input_flux = 0x4;
}

DiskII::DiskII() :
	inputs_(input_command),
	drives_{{2045454, 300, 1}, {2045454, 300, 1}}
{
	drives_[0].set_sleep_observer(this);
	drives_[1].set_sleep_observer(this);
}

void DiskII::set_control(Control control, bool on) {
	int previous_stepper_mask = stepper_mask_;
	switch(control) {
		case Control::P0: stepper_mask_ = (stepper_mask_ & 0xe) | (on ? 0x1 : 0x0);	break;
		case Control::P1: stepper_mask_ = (stepper_mask_ & 0xd) | (on ? 0x2 : 0x0);	break;
		case Control::P2: stepper_mask_ = (stepper_mask_ & 0xb) | (on ? 0x4 : 0x0);	break;
		case Control::P3: stepper_mask_ = (stepper_mask_ & 0x7) | (on ? 0x8 : 0x0);	break;

		case Control::Motor:
			// TODO: does the motor control trigger both motors at once?
			drives_[0].set_motor_on(on);
			drives_[1].set_motor_on(on);
		break;
	}

//	printf("%0x: Set control %d %s\n", stepper_mask_, control, on ? "on" : "off");

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

void DiskII::set_mode(Mode mode) {
//	printf("Set mode %d\n", mode);
	inputs_ = (inputs_ & ~input_mode) | ((mode == Mode::Write) ? input_mode : 0);
	set_controller_can_sleep();
}

void DiskII::select_drive(int drive) {
//	printf("Select drive %d\n", drive);
	active_drive_ = drive & 1;
	drives_[active_drive_].set_event_delegate(this);
	drives_[active_drive_^1].set_event_delegate(nullptr);
}

void DiskII::set_data_register(uint8_t value) {
//	printf("Set data register (?)\n");
	inputs_ |= input_command;
	data_register_ = value;
	set_controller_can_sleep();
}

uint8_t DiskII::get_shift_register() {
//	if(shift_register_ & 0x80) printf("[%02x] ", shift_register_);
	inputs_ &= ~input_command;
	set_controller_can_sleep();
	return shift_register_;
}

void DiskII::run_for(const Cycles cycles) {
/*
... address the P6 ROM with an index byte built up as:
+-------+-------+-------+-------+-------+-------+-------+-------+
| STATE | STATE | STATE | PULSE |  Q7   |  Q6   |  SR   | STATE |
| bit 0 | bit 2 | bit 3 |       |       |       |  MSB  | bit 1 |
+-------+-------+-------+-------+-------+-------+-------+-------+
    7       6       5       4       3       2       1       0

...

The bytes in the P6 ROM has the high four bits reversed compared to the BAPD charts, so you will have to reverse them after fetching the byte.

*/
	if(is_sleeping()) return;

	int integer_cycles = cycles.as_int();

	if(!controller_can_sleep_) {
		while(integer_cycles--) {
			const int address =
				(inputs_ << 2) |
				((shift_register_&0x80) >> 6) |
				((state_&0x2) >> 1) |
				((state_&0x1) << 7) |
				((state_&0x4) << 4) |
				((state_&0x8) << 2);
			inputs_ |= input_flux;

			const uint8_t update = state_machine_[static_cast<std::size_t>(address)];
			state_ = update >> 4;
			state_ = ((state_ & 0x8) ? 0x1 : 0x0) | ((state_ & 0x4) ? 0x2 : 0x0) | ((state_ & 0x2) ? 0x4 : 0x0) | ((state_ & 0x1) ? 0x8 : 0x0);

			uint8_t command = update & 0xf;
			switch(command) {
				case 0x0:	shift_register_ = 0;													break;	// clear
				case 0x9:	shift_register_ = static_cast<uint8_t>(shift_register_ << 1);			break;	// shift left, bringing in a zero
				case 0xd:	shift_register_ = static_cast<uint8_t>((shift_register_ << 1) | 1);		break;	// shift left, bringing in a one
				case 0xb:	shift_register_ = data_register_;										break;	// load
				case 0xa:
					shift_register_ = (shift_register_ >> 1) | (is_write_protected() ? 0x80 : 0x00);
				break;	// shift right, bringing in write protected status
				default: break;
			}

			// TODO: surely there's a less heavyweight solution than this?
			if(!drive_is_sleeping_[0]) drives_[0].run_for(Cycles(1));
			if(!drive_is_sleeping_[1]) drives_[1].run_for(Cycles(1));
		}
	} else {
		if(!drive_is_sleeping_[0]) drives_[0].run_for(cycles);
		if(!drive_is_sleeping_[1]) drives_[1].run_for(cycles);
	}

	set_controller_can_sleep();
}

void DiskII::set_controller_can_sleep() {
	// Permit the controller to sleep if it's in sense write protect mode, and the shift register
	// has already filled with the result of shifting eight times.
	controller_can_sleep_ =
		(inputs_ == (input_command | input_flux)) &&
		(shift_register_ == (is_write_protected() ? 0xff : 0x00));
	if(is_sleeping()) update_sleep_observer();
}

bool DiskII::is_write_protected() {
	return true;
}

void DiskII::set_state_machine(const std::vector<uint8_t> &state_machine) {
	state_machine_ = state_machine;
	// TODO: shuffle ordering here?
}

void DiskII::set_disk(const std::shared_ptr<Storage::Disk::Disk> &disk, int drive) {
	drives_[drive].set_disk(disk);
}

void DiskII::process_event(const Storage::Disk::Track::Event &event) {
	if(event.type == Storage::Disk::Track::Event::FluxTransition) {
		inputs_ &= ~input_flux;
		set_controller_can_sleep();
	}
}

void DiskII::set_component_is_sleeping(Sleeper *component, bool is_sleeping) {
	drive_is_sleeping_[0] = drives_[0].is_sleeping();
	drive_is_sleeping_[1] = drives_[1].is_sleeping();
	update_sleep_observer();
}

bool DiskII::is_sleeping() {
	return controller_can_sleep_ && drive_is_sleeping_[0] && drive_is_sleeping_[1];
}

void DiskII::set_register(int address, uint8_t value) {
	trigger_address(address, value);
}

uint8_t DiskII::get_register(int address) {
	return trigger_address(address, 0xff);
}

uint8_t DiskII::trigger_address(int address, uint8_t value) {
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

		case 0x8:	set_control(Control::Motor, false);	break;
		case 0x9:	set_control(Control::Motor, true);	break;

		case 0xa:	select_drive(0);					break;
		case 0xb:	select_drive(1);					break;

		case 0xc:	return get_shift_register();
		case 0xd:	set_data_register(value);			break;

		case 0xe:	set_mode(Mode::Read);				break;
		case 0xf:	set_mode(Mode::Write);				break;
	}
	return 0xff;
}

