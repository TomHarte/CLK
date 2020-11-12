//
//  IWM.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/05/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "IWM.hpp"

#include "../../Outputs/Log.hpp"

using namespace Apple;

namespace  {
	constexpr int CA0		= 1 << 0;
	constexpr int CA1		= 1 << 1;
	constexpr int CA2		= 1 << 2;
	constexpr int LSTRB		= 1 << 3;
	constexpr int ENABLE	= 1 << 4;
	constexpr int DRIVESEL	= 1 << 5;	/* This means drive select, like on the original Disk II. */
	constexpr int Q6		= 1 << 6;
	constexpr int Q7		= 1 << 7;
	constexpr int SEL		= 1 << 8;	/* This is an additional input, not available on a Disk II, with a confusingly-similar name to SELECT but a distinct purpose. */
}

IWM::IWM(int clock_rate) :
	clock_rate_(clock_rate) {}

// MARK: - Bus accessors

uint8_t IWM::read(int address) {
	access(address);

	// Per Inside Macintosh:
	//
	// "Before you can read from any of the disk registers you must set up the state of the IWM so that it
	// can pass the data through to the MC68000's address space where you'll be able to read it. To do that,
	// you must first turn off Q7 by reading or writing dBase+q7L. Then turn on Q6 by accessing dBase+q6H.
	// After that, the IWM will be able to pass data from the disk's RD/SENSE line through to you."
	//
	// My understanding:
	//
	//	Q6 = 1, Q7 = 0 reads the status register. The meaning of the top 'SENSE' bit is then determined by
	//	the CA0,1,2 and SEL switches as described in Inside Macintosh, summarised above as RD/SENSE.

	if(address&1) {
		return 0xff;
	}

	switch(state_ & (Q6 | Q7 | ENABLE)) {
		default:
			LOG("[IWM] Invalid read\n");
		return 0xff;

		// "Read all 1s".
//			printf("Reading all 1s\n");
//		return 0xff;

		case 0:
		case ENABLE: {				/* Read data register. Zeroing afterwards is a guess. */
			const auto result = data_register_;

			if(data_register_ & 0x80) {
//				printf("\n\nIWM:%02x\n\n", data_register_);
//				printf(".");
				data_register_ = 0;
			}
//			LOG("Reading data register: " << PADHEX(2) << int(result));

			return result;
		}

		case Q6: case Q6|ENABLE: {
			/*
				[If A = 0], Read status register:

				bits 0-4: same as mode register.
				bit 5: 1 = either /ENBL1 or /ENBL2 is currently low.
				bit 6: 1 = MZ (reserved for future compatibility; should always be read as 0).
				bit 7: 1 = SENSE input high; 0 = SENSE input low.

				(/ENBL1 is low when the first drive's motor is on; /ENBL2 is low when the second drive's motor is on.
				If the 1-second timer is enabled, motors remain on for one second after being programmatically disabled.)
			*/

			return uint8_t(
				(mode_&0x1f) |
				((state_ & ENABLE) ? 0x20 : 0x00) |
				(sense() & 0x80)
			);
		} break;

		case Q7: case Q7|ENABLE:
			/*
				Read write-handshake register:

				bits 0-5: reserved for future use (currently read as 1).
				bit 6: 1 = write state (0 = underrun has occurred; 1 = no underrun so far).
				bit 7: 1 = write data buffer ready for data (1 = ready; 0 = busy).
			*/
//			LOG("Reading write handshake: " << PADHEX(2) << (0x3f | write_handshake_));
		return 0x3f | write_handshake_;
	}

	return 0xff;
}

void IWM::write(int address, uint8_t input) {
	access(address);

	switch(state_ & (Q6 | Q7 | ENABLE)) {
		default: break;

		case Q7|Q6:
			/*
				Write mode register:

				bit 0: 1 = latch mode (should be set in asynchronous mode).
				bit 1: 0 = synchronous handshake protocol; 1 = asynchronous.
				bit 2: 0 = 1-second on-board timer enable; 1 = timer disable.
				bit 3: 0 = slow mode; 1 = fast mode.
				bit 4: 0 = 7Mhz; 1 = 8Mhz (7 or 8 mHz clock descriptor).
				bit 5: 1 = test mode; 0 = normal operation.
				bit 6: 1 = MZ-reset.
				bit 7: reserved for future expansion.
			*/

			mode_ = input;

			switch(mode_ & 0x18) {
				case 0x00:		bit_length_ = Cycles(24);		break;	// slow mode, 7Mhz
				case 0x08:		bit_length_ = Cycles(12);		break;	// fast mode, 7Mhz
				case 0x10:		bit_length_ = Cycles(32);		break;	// slow mode, 8Mhz
				case 0x18:		bit_length_ = Cycles(16);		break;	// fast mode, 8Mhz
			}
			LOG("IWM mode is now " << PADHEX(2) << int(mode_));
		break;

		case Q7|Q6|ENABLE:	// Write data register.
			next_output_ = input;
			write_handshake_ &= ~0x80;
		break;
	}
}

// MARK: - Switch access

void IWM::access(int address) {
	// Keep a record of switch state; bits in state_
	// should correlate with the anonymous namespace constants
	// defined at the top of this file — CA0, CA1, etc.
	address &= 0xf;
	const auto mask = 1 << (address >> 1);
	const auto old_state = state_;

	if(address & 1) {
		state_ |= mask;
	} else {
		state_ &= ~mask;
	}

	// React appropriately to ENABLE and DRIVESEL changes, and changes into/out of write mode.
	if(old_state != state_) {
		push_drive_state();

		switch(mask) {
			default: break;

			case ENABLE:
				if(address & 1) {
					if(drives_[active_drive_]) drives_[active_drive_]->set_enabled(true);
				} else {
					// If the 1-second delay is enabled, set up a timer for that.
					if(!(mode_ & 4)) {
						cycles_until_disable_ = Cycles(clock_rate_);
					} else {
						if(drives_[active_drive_]) drives_[active_drive_]->set_enabled(false);
					}
				}
			break;

			case DRIVESEL: {
				const int new_drive = address & 1;
				if(new_drive != active_drive_) {
					if(drives_[active_drive_]) drives_[active_drive_]->set_enabled(false);
					active_drive_ = new_drive;
					if(drives_[active_drive_]) {
						drives_[active_drive_]->set_enabled(state_ & ENABLE || (cycles_until_disable_ > Cycles(0)));
						push_drive_state();
					}
				}
			} break;

			case Q6:
			case Q7:
				select_shift_mode();
			break;
		}
	}
}

void IWM::set_select(bool enabled) {
	// Store SEL as an extra state bit.
	if(enabled) state_ |= SEL;
	else state_ &= ~SEL;
	push_drive_state();
}

void IWM::push_drive_state() {
	if(drives_[active_drive_])  {
		const uint8_t drive_control_lines =
			((state_ & CA0) ? IWMDrive::CA0 : 0) |
			((state_ & CA1) ? IWMDrive::CA1 : 0) |
			((state_ & CA2) ? IWMDrive::CA2 : 0) |
			((state_ & SEL) ? IWMDrive::SEL : 0) |
			((state_ & LSTRB) ? IWMDrive::LSTRB : 0);
		drives_[active_drive_]->set_control_lines(drive_control_lines);
	}
}

// MARK: - Active logic

void IWM::run_for(const Cycles cycles) {
	// Check for a timeout of the motor-off timer.
	if(cycles_until_disable_ > Cycles(0)) {
		cycles_until_disable_ -= cycles;
		if(cycles_until_disable_ <= Cycles(0)) {
			cycles_until_disable_ = Cycles(0);
			if(drives_[active_drive_])
				drives_[active_drive_]->set_enabled(false);
		}
	}

	// Activity otherwise depends on mode and motor state.
	auto integer_cycles = cycles.as_integral();
	switch(shift_mode_) {
		case ShiftMode::Reading: {
			// Per the IWM patent, column 7, around line 35 onwards: "The expected time
			// is widened by approximately one-half an interval before and after the
			// expected time since the data is not precisely spaced when read due to
			// variations in drive speed and other external factors". The error_margin
			// here implements the 'after' part of that contract.
			const auto error_margin = Cycles(bit_length_.as_integral() >> 1);

			if(drive_is_rotating_[active_drive_]) {
				while(integer_cycles--) {
					drives_[active_drive_]->run_for(Cycles(1));
					++cycles_since_shift_;
					if(cycles_since_shift_ == bit_length_ + error_margin) {
						propose_shift(0);
					}
				}
			} else {
				while(cycles_since_shift_ + integer_cycles >= bit_length_ + error_margin) {
					const auto run_length = bit_length_ + error_margin - cycles_since_shift_;
					integer_cycles -= run_length.as_integral();
					cycles_since_shift_ += run_length;
					propose_shift(0);
				}
				cycles_since_shift_ += Cycles(integer_cycles);
			}
		} break;

		case ShiftMode::Writing:
			while(cycles_since_shift_ + integer_cycles >= bit_length_) {
				const auto cycles_until_write = bit_length_ - cycles_since_shift_;
				if(drives_[active_drive_]) {
					drives_[active_drive_]->run_for(cycles_until_write);

					// Output a flux transition if the top bit is set.
					drives_[active_drive_]->write_bit(shift_register_ & 0x80);
				}
				shift_register_ <<= 1;

				integer_cycles -= cycles_until_write.as_integral();
				cycles_since_shift_ = Cycles(0);

				--output_bits_remaining_;
				if(!output_bits_remaining_) {
					if(!(write_handshake_ & 0x80)) {
						shift_register_ = next_output_;
						output_bits_remaining_ = 8;
//						LOG("Next byte: " << PADHEX(2) << int(shift_register_));
					} else {
						write_handshake_ &= ~0x40;
						if(drives_[active_drive_]) drives_[active_drive_]->end_writing();
						LOG("Overrun; done.");
						output_bits_remaining_ = 1;
					}

					// Either way, the IWM is ready for more data.
					write_handshake_ |= 0x80;
				}
			}

			// Either some bits were output, in which case cycles_since_shift_ is no 0 and
			// integer_cycles is some number less than bit_length_, or none were and
			// cycles_since_shift_ + integer_cycles is less than bit_length, and the new
			// part should be accumulated.
			cycles_since_shift_ += integer_cycles;

			if(drives_[active_drive_] && integer_cycles) {
				drives_[active_drive_]->run_for(cycles_since_shift_);
			}
		break;

		case ShiftMode::CheckingWriteProtect:
			if(integer_cycles < 8) {
				shift_register_ = (shift_register_ >> integer_cycles) | (sense() & (0xff << (8 - integer_cycles)));
			} else {
				shift_register_ = sense();
			}
			[[fallthrough]];

		default:
			if(drive_is_rotating_[active_drive_]) drives_[active_drive_]->run_for(cycles);
		break;
	}
}

void IWM::select_shift_mode() {
	// Don't allow an ongoing write to be interrupted.
	if(shift_mode_ == ShiftMode::Writing && drives_[active_drive_] && drives_[active_drive_]->is_writing()) return;

	const auto old_shift_mode = shift_mode_;

	switch(state_ & (Q6|Q7)) {
		default:	shift_mode_ = ShiftMode::CheckingWriteProtect;		break;
		case 0:		shift_mode_ = ShiftMode::Reading;					break;
		case Q7:
			// "The IWM is put into the write state by a transition from the write protect sense state to the
			// write load state".
			if(shift_mode_ == ShiftMode::CheckingWriteProtect) shift_mode_ = ShiftMode::Writing;
		break;
	}

	// If writing mode just began, set the drive into write mode and cue up the first output byte.
	if(old_shift_mode != ShiftMode::Writing && shift_mode_ == ShiftMode::Writing) {
		if(drives_[active_drive_]) drives_[active_drive_]->begin_writing(Storage::Time(1, clock_rate_ / bit_length_.as_integral()), false);
		shift_register_ = next_output_;
		write_handshake_ |= 0x80 | 0x40;
		output_bits_remaining_ = 8;
		LOG("Seeding output with " << PADHEX(2) << shift_register_);
	}
}

uint8_t IWM::sense() {
	return drives_[active_drive_] ? (drives_[active_drive_]->read() ? 0xff : 0x00) : 0xff;
}

void IWM::process_event(const Storage::Disk::Drive::Event &event) {
	if(shift_mode_ != ShiftMode::Reading) return;

	switch(event.type) {
		case Storage::Disk::Track::Event::IndexHole: return;
		case Storage::Disk::Track::Event::FluxTransition:
			propose_shift(1);
		break;
	}
}

void IWM::propose_shift(uint8_t bit) {
	// TODO: synchronous mode.

//	LOG("Shifting input");

	// See above for text from the IWM patent, column 7, around line 35 onwards.
	// The error_margin here implements the 'before' part of that contract.
	//
	// Basic effective logic: if at least 1 is fozund in the bit_length_ cycles centred
	// on the current expected bit delivery time as implied by cycles_since_shift_,
	// shift in a 1 and start a new window wherever the first found 1 was.
	//
	// If no 1s are found, shift in a 0 and don't alter expectations as to window placement.
	const auto error_margin = Cycles(bit_length_.as_integral() >> 1);
	if(bit && cycles_since_shift_ < error_margin) return;

	shift_register_ = uint8_t((shift_register_ << 1) | bit);
	if(shift_register_ & 0x80) {
		data_register_ = shift_register_;
		shift_register_ = 0;
	}

	if(bit)
		cycles_since_shift_ = Cycles(0);
	else
		cycles_since_shift_ -= bit_length_;
}

void IWM::set_drive(int slot, IWMDrive *drive) {
	drives_[slot] = drive;
	if(drive) {
		drive->set_event_delegate(this);
		drive->set_clocking_hint_observer(this);
	} else {
		drive_is_rotating_[slot] = false;
	}
}

void IWM::set_component_prefers_clocking(ClockingHint::Source *component, ClockingHint::Preference clocking) {
	const bool is_rotating = clocking != ClockingHint::Preference::None;

	if(drives_[0] && component == static_cast<ClockingHint::Source *>(drives_[0])) {
		drive_is_rotating_[0] = is_rotating;
	} else if(drives_[1] && component == static_cast<ClockingHint::Source *>(drives_[1])) {
		drive_is_rotating_[1] = is_rotating;
	}
}

void IWM::set_activity_observer(Activity::Observer *observer) {
	if(drives_[0]) drives_[0]->set_activity_observer(observer, "Internal Floppy", true);
	if(drives_[1]) drives_[1]->set_activity_observer(observer, "External Floppy", true);
}
