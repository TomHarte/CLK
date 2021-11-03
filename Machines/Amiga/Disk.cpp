//
//  Disk.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 02/11/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Chipset.hpp"

#define LOG_PREFIX "[Disk] "
#include "../../Outputs/Log.hpp"

using namespace Amiga;

// MARK: - Disk DMA.

void Chipset::DiskDMA::enqueue(uint16_t value, bool matches_sync) {
	if(matches_sync && state_ == State::WaitingForSync) {
		state_ = State::Reading;
		return;
	}

	if(state_ == State::Reading) {
		buffer_[buffer_write_ & 3] = value;
		if(buffer_write_ == buffer_read_ + 4) {
			++buffer_read_;
		}
		++buffer_write_;
	}
}

void Chipset::DiskDMA::set_control(uint16_t control) {
	sync_with_word_ = control & 0x400;
}

void Chipset::DiskDMA::set_length(uint16_t value) {
	if(value == last_set_length_) {
		dma_enable_ = value & 0x8000;
		write_ = value & 0x4000;
		length_ = value & 0x3fff;
		buffer_read_ = buffer_write_ = 0;

		if(dma_enable_) {
			LOG("Disk DMA " << (write_ ? "write" : "read") << " of " << length_ << " to " << PADHEX(8) << pointer_[0]);
		}

		state_ = sync_with_word_ ? State::WaitingForSync : State::Reading;
	}

	last_set_length_ = value;
}

bool Chipset::DiskDMA::advance() {
	if(!dma_enable_) return false;

	if(!write_) {
		if(length_ && buffer_read_ != buffer_write_) {
			ram_[pointer_[0] & ram_mask_] = buffer_[buffer_read_ & 3];
			++pointer_[0];
			++buffer_read_;
			--length_;

			if(!length_) {
				chipset_.posit_interrupt(InterruptFlag::DiskBlock);
				state_ = State::Inactive;
			}

			return true;
		}
	}

	return false;
}

// MARK: - Disk Controller.

Chipset::DiskController::DiskController(Cycles clock_rate, Chipset &chipset, DiskDMA &disk_dma, CIAB &cia) :
	Storage::Disk::Controller(clock_rate),
	chipset_(chipset),
	disk_dma_(disk_dma),
	cia_(cia) {

	// Add four drives.
	for(int c = 0; c < 4; c++) {
		emplace_drive(clock_rate.as<int>(), 300, 2, Storage::Disk::Drive::ReadyType::IBMRDY);
	}
}

void Chipset::DiskController::process_input_bit(int value) {
	data_ = uint16_t((data_ << 1) | value);
	++bit_count_;

	const bool sync_matches = data_ == sync_word_;
	if(sync_matches) {
		chipset_.posit_interrupt(InterruptFlag::DiskSyncMatch);

		if(sync_with_word_) {
			bit_count_ = 0;
		}
	}

	if(!(bit_count_ & 15)) {
		disk_dma_.enqueue(data_, sync_matches);
	}
}

void Chipset::DiskController::set_sync_word(uint16_t value) {
	LOG("Set disk sync word to " << PADHEX(4) << value);
	sync_word_ = value;
}

void Chipset::DiskController::set_control(uint16_t control) {
	// b13 and b14: precompensation length specifier
	// b12: 0 => GCR precompensation; 1 => MFM.
	// b10: 1 => enable use of word sync; 0 => disable.
	// b9: 1 => sync on MSB (Disk II style, presumably?); 0 => don't.
	// b8: 1 => 2µs per bit; 0 => 4µs.

	sync_with_word_ = control & 0x400;

	Storage::Time bit_length;
	bit_length.length = 1;
	bit_length.clock_rate = (control & 0x100) ? 500000 : 250000;
	set_expected_bit_length(bit_length);

	LOG((sync_with_word_ ? "Will" : "Won't") << " sync with word; bit length is " << ((control & 0x100) ? "short" : "long"));
}

void Chipset::DiskController::process_index_hole() {
	// Pulse the CIA flag input.
	//
	// TODO: rectify once drives do an actual index pulse, with length.
	cia_.set_flag_input(true);
	cia_.set_flag_input(false);

	// Resync word output. Experimental!!
	bit_count_ = 0;
}

void Chipset::DiskController::set_mtr_sel_side_dir_step(uint8_t value) {
	// b7: /MTR
	// b6: /SEL3
	// b5: /SEL2
	// b4: /SEL1
	// b3: /SEL0
	// b2: /SIDE
	// b1: DIR
	// b0: /STEP

	// Select active drive.
	set_drive(((value >> 3) & 0x0f) ^ 0x0f);

	// "[The MTR] signal is nonstandard on the Amiga system.
	// Each drive will latch the motor signal at the time its
	// select signal turns on." — The Hardware Reference Manual.
	const auto difference = int(previous_select_ ^ value);
	previous_select_ = value;

	// Check for changes in the SEL line per drive.
	const bool motor_on = !(value & 0x80);
	const int side = (value & 0x04) ? 0 : 1;
	const bool did_step = difference & value & 0x01;
	const auto direction = Storage::Disk::HeadPosition(
		(value & 0x02) ? -1 : 1
	);

	for(int c = 0; c < 4; c++) {
		auto &drive = get_drive(size_t(c));
		const int select_mask = 0x08 << c;
		const bool is_selected = !(value & select_mask);

		// Both the motor state and the ID shifter are affected upon
		// changes in drive selection only.
		if(difference & select_mask) {
			// If transitioning to inactive, shift the drive ID value;
			// if transitioning to active, possibly reset the drive
			// ID and definitely latch the new motor state.
			if(!is_selected) {
				drive_ids_[c] <<= 1;
				LOG("Shifted drive ID shift register for drive " << +c << " to " << PADHEX(4) << std::bitset<16>{drive_ids_[c]});
			} else {
				// Motor transition on -> off => reload register.
				if(!motor_on && drive.get_motor_on()) {
					// NB:
					//	0xffff'ffff	= 3.5" drive;
					//	0x5555'5555 = 5.25" drive;
					//	0x0000'0000 = no drive.
					drive_ids_[c] = 0xffff'ffff;
					LOG("Reloaded drive ID shift register for drive " << +c);
				}

				// Also latch the new motor state.
				drive.set_motor_on(motor_on);
			}
		}

		// Set the new side.
		drive.set_head(side);

		// Possibly step.
		if(did_step && is_selected) {
			LOG("Stepped drive " << +c << " by " << std::dec << +direction.as_int());
			drive.step(direction);
		}
	}
}

uint8_t Chipset::DiskController::get_rdy_trk0_wpro_chng() {
	//	b5:	/RDY
	//	b4:	/TRK0
	//	b3:	/WPRO
	//	b2:	/CHNG

	// My interpretation:
	//
	//	RDY isn't RDY, it's a shift value as described above, combined with the motor state.
	//	CHNG is what is normally RDY.

	const uint32_t combined_id =
		((previous_select_ & 0x40) ? 0 : drive_ids_[3]) |
		((previous_select_ & 0x20) ? 0 : drive_ids_[2]) |
		((previous_select_ & 0x10) ? 0 : drive_ids_[1]) |
		((previous_select_ & 0x08) ? 0 : drive_ids_[0]);

	auto &drive = get_drive();
	const uint8_t active_high =
		((combined_id & 0x8000) >> 10) |
		(drive.get_motor_on() ? 0x20 : 0x00) |
		(drive.get_is_ready() ? 0x00 : 0x04) |
		(drive.get_is_track_zero() ? 0x10 : 0x00) |
		(drive.get_is_read_only() ? 0x08 : 0x00);

	return ~active_high;
}

void Chipset::DiskController::set_activity_observer(Activity::Observer *observer) {
	for_all_drives([observer] (Storage::Disk::Drive &drive, size_t index) {
		drive.set_activity_observer(observer, "Drive " + std::to_string(index+1), true);
	});
}

bool Chipset::DiskController::insert(const std::shared_ptr<Storage::Disk::Disk> &disk, size_t drive) {
	if(drive >= 4) return false;
	get_drive(drive).set_disk(disk);
	return true;
}

bool Chipset::insert(const std::vector<std::shared_ptr<Storage::Disk::Disk>> &disks) {
	bool inserted = false;

	size_t target = 0;
	for(const auto &disk: disks) {
		inserted |= disk_controller_.insert(disk, target);
		++target;
	}

	return inserted;
}
