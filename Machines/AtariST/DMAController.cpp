//
//  DMAController.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 26/10/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "DMAController.hpp"

#include <cstdio>

using namespace Atari::ST;

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
			if(control_ & 0x10) {
				return sector_count_;
			} else {
				return fdc_.get_register(control_ >> 1);
			}
		break;

		// DMA status.
		case 3:
		return status_;

		// DMA addressing.
		case 4:
		case 5:
		case 6:
		break;
	}
	return 0xffff;
}

void DMAController::write(int address, uint16_t value) {
	switch(address & 7) {
		// Reserved.
		default: break;

		// Disk controller or sector count.
		case 2:
			if(control_ & 0x10) {
				sector_count_ = value;
			} else {
				fdc_.set_register(control_ >> 1, uint8_t(value));
			}
		break;

		// DMA control; meaning is:
		//
		//	b1, b2 = address lines for FDC access.
		//	b3 = 1 => HDC access; 0 => FDC access.
		//	b4 = 1 => sector count access; 1 => FDC access.
		//	b6 = 1 => DMA off; 0 => DMA on.
		//	b7 = 1 => FDC access; 0 => HDC access.
		//	b8 = 1 => write to [F/H]DC registers; 0 => read.
		//
		//	All other bits: undefined.
		//	TODO: determine how b3 and b7 differ.
		case 3:
			control_ = value;
		break;

		// DMA addressing.
		case 4:
		case 5:
		case 6:
		break;
	}
}

void DMAController::set_floppy_drive_selection(bool drive1, bool drive2, bool side2) {
	fdc_.set_floppy_drive_selection(drive1, drive2, side2);
}

void DMAController::set_floppy_disk(std::shared_ptr<Storage::Disk::Disk> disk, size_t drive) {
	fdc_.drives_[drive]->set_disk(disk);
}

void DMAController::run_for(HalfCycles duration) {
	running_time_ += duration;
	fdc_.run_for(duration.flush<Cycles>());
}

void DMAController::wd1770_did_change_output(WD::WD1770 *) {
	// Check for a change in interrupt state.
	const bool old_interrupt_line = interrupt_line_;
	interrupt_line_ = fdc_.get_interrupt_request_line();
	if(interrupt_delegate_ && interrupt_line_ != old_interrupt_line) {
		interrupt_delegate_->dma_controller_did_change_interrupt_status(this);
	}

	// TODO: check for a data request.
	if(fdc_.get_data_request_line()) {
		// TODO: something?
		printf("DRQ?\n");
	}
}

void DMAController::set_interrupt_delegate(InterruptDelegate *delegate) {
	interrupt_delegate_ = delegate;
}

bool DMAController::get_interrupt_line() {
	return interrupt_line_;
}

void DMAController::set_component_prefers_clocking(ClockingHint::Source *, ClockingHint::Preference) {
	update_clocking_observer();
}

ClockingHint::Preference DMAController::preferred_clocking() {
	return (fdc_.preferred_clocking() == ClockingHint::Preference::None) ? ClockingHint::Preference::None : ClockingHint::Preference::RealTime;
}
