//
//  DMAController.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 26/10/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "DMAController.hpp"

#define LOG_PREFIX "[DMA] "
#include "../../../Outputs/Log.hpp"

#include <cstdio>

using namespace Atari::ST;

namespace {

enum Control: uint16_t {
	Direction = 0x100,
	DRQSource = 0x80,
	SectorCountSelect = 0x10,
	CPUTarget = 0x08
};

}

DMAController::DMAController() {
	fdc_.set_delegate(this);
	fdc_.set_clocking_hint_observer(this);
}

uint16_t DMAController::read(int address) {
	switch(address & 7) {
		// Reserved.
		default: break;

		// Disk controller or sector count.
		case 2:
			if(control_ & Control::SectorCountSelect) {
				return uint16_t((byte_count_ + 511) >> 9);	// Assumed here: the count is of sectors remaining, i.e. it decrements
															// only when a sector is complete.
			} else {
				if(control_ & Control::CPUTarget) {
					return 0xffff;
				} else {
					return 0xff00 | fdc_.read(control_ >> 1);
				}
			}
		break;

		// DMA status.
		case 3:
		// TODO: should DRQ come from the HDC if that mode is selected?
		return 0xfff8 | (error_ ? 0 : 1) | (byte_count_ ? 2 : 0) | (fdc_.get_data_request_line() ? 4 : 0);

		// DMA addressing.
		case 4:	return uint16_t(0xff00 | ((address_ >> 16) & 0xff));
		case 5:	return uint16_t(0xff00 | ((address_ >> 8) & 0xff));
		case 6:	return uint16_t(0xff00 | ((address_ >> 0) & 0xff));
	}
	return 0xffff;
}

void DMAController::write(int address, uint16_t value) {
	switch(address & 7) {
		// Reserved.
		default: break;

		// Disk controller or sector count.
		case 2:
			if(control_ & Control::SectorCountSelect) {
				byte_count_ = (value & 0xff) << 9;	// The computer provides a sector count; that times 512 is a byte count.

				// TODO: if this is a write-mode DMA operation, try to fill both buffers, ASAP.
			} else {
				if(control_ & Control::CPUTarget) {
					// TODO: HDC.
				} else {
					fdc_.write(control_ >> 1, uint8_t(value));
				}
			}
		break;

		// DMA control; meaning is:
		//
		//	b0: unused
		//	b1, b2 = address lines for FDC access.
		//	b3 = 1 => CPU HDC access; 0 => CPU FDC access.
		//	b4 = 1 => sector count access; 0 => [F/H]DC access.
		//	b5: unused.
		//	b6 = officially, 1 => DMA off; 0 => DMA on. Ignored in real hardware.
		//	b7 = 1 => FDC DRQs being observed; 0 => HDC access DRQs being observed.
		//	b8 = 1 => DMA is writing to [F/H]DC; 0 => DMA is reading. Changing value resets DMA state.
		//
		//	All other bits: undefined.
		case 3:
			// Check for a DMA state reset.
			if((control_^value) & Control::Direction) {
				bytes_received_ = active_buffer_ = 0;
				error_ = false;
				byte_count_ = 0;
			}
			control_ = value;
		break;

		// DMA addressing; cf. http://www.atari-forum.com/viewtopic.php?t=30289 on a hardware
		// feature emulated here: 'carry' will ripple upwards if a write resets the top bit
		// of the byte it is adjusting.
		case 4:	address_ = int((address_ & 0x00ffff) | ((value & 0xff) << 16));	break;
		case 5:
			if(((value << 8) ^ address_) & ~(value << 8) & 0x8000) address_ += 0x10000;
			address_ = int((address_ & 0xff00ff) | ((value & 0xff) << 8));
		break;
		case 6:
			if((value ^ address_) & ~value & 0x80) address_ += 0x100;
			address_ = int((address_ & 0xffff00) | ((value & 0xfe) << 0));
		break;	// Lowest bit: discarded.
	}
}

void DMAController::set_floppy_drive_selection(bool drive1, bool drive2, bool side2) {
//	LOG("Selected: " << (drive1 ? "1" : "-") << (drive2 ? "2" : "-") << (side2 ? "s" : "-"));
	fdc_.set_floppy_drive_selection(drive1, drive2, side2);
}

void DMAController::set_floppy_disk(std::shared_ptr<Storage::Disk::Disk> disk, size_t drive) {
	fdc_.set_disk(disk, drive);
}

void DMAController::run_for(HalfCycles duration) {
	running_time_ += duration;
	fdc_.run_for(duration.flush<Cycles>());
}

void DMAController::wd1770_did_change_output(WD::WD1770 *) {
	// Check for a change in interrupt state.
	const bool old_interrupt_line = interrupt_line_;
	interrupt_line_ = fdc_.get_interrupt_request_line();
	if(delegate_ && interrupt_line_ != old_interrupt_line) {
		delegate_->dma_controller_did_change_output(this);
	}

	// Check for a change in DRQ state, if it's the FDC that is currently being watched.
	if(byte_count_ && fdc_.get_data_request_line() && (control_ & Control::DRQSource)) {
		--byte_count_;

		if(control_ & Control::Direction) {
			// TODO: DMA is supposed to be helping with a write.
		} else {
			// DMA is enabling a read.

			// Read from the data register into the active buffer.
			if(bytes_received_ < 16) {
				buffer_[active_buffer_].contents[bytes_received_] = fdc_.read(3);
				++bytes_received_;
			}
			if(bytes_received_ == 16) {
				// Mark buffer as full.
				buffer_[active_buffer_].is_full = true;

				// Move to the next if it is empty; if it isn't, note a DMA error.
				const auto next_buffer = active_buffer_ ^ 1;
				error_ |= buffer_[next_buffer].is_full;
				if(!buffer_[next_buffer].is_full) {
					bytes_received_ = 0;
					active_buffer_ = next_buffer;
				}

				// Set bus request.
				if(!bus_request_line_) {
					bus_request_line_ = true;
					if(delegate_) delegate_->dma_controller_did_change_output(this);
				}
			}
		}
	}
}

int DMAController::bus_grant(uint16_t *ram, size_t size) {
	// Being granted the bus negates the request.
	bus_request_line_ = false;
	if(delegate_) delegate_->dma_controller_did_change_output(this);

	size <<= 1;	// Convert to bytes.
	if(control_ & Control::Direction) {
		// TODO: writes.
		return 0;
	} else {
		// Check that the older buffer is full; stop if not.
		if(!buffer_[active_buffer_ ^ 1].is_full) return 0;

#define b(i, n) " " << PADHEX(2) << int(buffer_[i].contents[n])
#define b2(i, n) b(i, n) << b(i, n+1)
#define b4(i, n) b2(i, n) << b2(i, n+2)
#define b16(i) b4(i, 0) << b4(i, 4) << b4(i, 8) << b4(i, 12)
//		LOG("[1] to " << PADHEX(6) << address_ << b16(active_buffer_ ^ 1));

		for(int c = 0; c < 8; ++c) {
			if(size_t(address_) < size) {
				ram[address_ >> 1] = uint16_t(
					(buffer_[active_buffer_ ^ 1].contents[(c << 1) + 0] << 8) |
					(buffer_[active_buffer_ ^ 1].contents[(c << 1) + 1] << 0)
				);
			}
			address_ += 2;
		}
		buffer_[active_buffer_ ^ 1].is_full = false;

		// Check that the newer buffer is full; stop if not.
		if(!buffer_[active_buffer_ ].is_full) return 8;

//		LOG("[2] to " << PADHEX(6) << address_ << b16(active_buffer_));
#undef b16
#undef b4
#undef b2
#undef b

		for(int c = 0; c < 8; ++c) {
			if(size_t(address_) < size) {
				ram[address_ >> 1] = uint16_t(
					(buffer_[active_buffer_].contents[(c << 1) + 0] << 8) |
					(buffer_[active_buffer_].contents[(c << 1) + 1] << 0)
				);
			}
			address_ += 2;
		}
		buffer_[active_buffer_].is_full = false;

		// Both buffers were full, so unblock reading.
		bytes_received_ = 0;

		return 16;
	}
}

void DMAController::set_delegate(Delegate *delegate) {
	delegate_ = delegate;
}

bool DMAController::get_interrupt_line() {
	return interrupt_line_;
}

bool DMAController::get_bus_request_line() {
	return bus_request_line_;
}

void DMAController::set_component_prefers_clocking(ClockingHint::Source *, ClockingHint::Preference) {
	update_clocking_observer();
}

ClockingHint::Preference DMAController::preferred_clocking() const {
	return (fdc_.preferred_clocking() == ClockingHint::Preference::None) ? ClockingHint::Preference::JustInTime : ClockingHint::Preference::RealTime;
}

void DMAController::set_activity_observer(Activity::Observer *observer) {
	fdc_.set_activity_observer(observer);
}
