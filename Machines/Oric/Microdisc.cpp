//
//  Microdisc.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/11/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "Microdisc.hpp"

using namespace Oric;

namespace {
	// The number below, in cycles against an 8Mhz clock, was arrived at fairly unscientifically,
	// by comparing the amount of time this emulator took to show a directory versus a video of
	// a real Oric. It therefore assumes all other timing measurements were correct on the day
	// of the test. More work to do, I think.
	const Cycles::IntType head_load_request_counter_target = 7653333;
}

Microdisc::Microdisc() : DiskController(P1793, 8000000, Storage::Disk::Drive::ReadyType::ShugartRDY) {
	set_control_register(last_control_, 0xff);
}

void Microdisc::set_control_register(uint8_t control) {
	const uint8_t changes = last_control_ ^ control;
	last_control_ = control;
	set_control_register(control, changes);
}

void Microdisc::set_control_register(uint8_t control, uint8_t changes) {
	// b2: data separator clock rate select (1 = double)	[TODO]

	// b65: drive select
	if((changes >> 5)&3) {
		set_drive(1 << (control >> 5)&3);
	}

	// b4: side select
	if(changes & 0x10) {
		const int head = (control & 0x10) >> 4;
		for_all_drives([head] (Storage::Disk::Drive &drive, size_t) {
			drive.set_head(head);
		});
	}

	// b3: double density select (0 = double)
	if(changes & 0x08) {
		set_is_double_density(!(control & 0x08));
	}

	// b0: IRQ enable
	if(changes & 0x01) {
		const bool had_irq = get_interrupt_request_line();
		irq_enable_ = bool(control & 0x01);
		const bool has_irq = get_interrupt_request_line();
		if(has_irq != had_irq && delegate_) {
			delegate_->wd1770_did_change_output(this);
		}
	}

	// b7: EPROM select (0 = select)
	// b1: ROM disable (0 = disable)
	if(changes & 0x82) {
		PagedItem item;
		if(control & 0x02) item = PagedItem::BASIC;
		else if(control & 0x80) {
			item = PagedItem::RAM;
		} else {
			item = PagedItem::DiskROM;
		}
		set_paged_item(item);
	}
}

bool Microdisc::get_interrupt_request_line() {
	return irq_enable_ && WD1770::get_interrupt_request_line();
}

uint8_t Microdisc::get_interrupt_request_register() {
	return 0x7f | (WD1770::get_interrupt_request_line() ? 0x00 : 0x80);
}

uint8_t Microdisc::get_data_request_register() {
	return 0x7f | (get_data_request_line() ? 0x00 : 0x80);
}

void Microdisc::set_head_load_request(bool head_load) {
	head_load_request_ = head_load;

	// The drive motors (at present: I believe **all drive motors** regardless of the selected drive) receive
	// the current head load request state.
	for_all_drives([head_load] (Storage::Disk::Drive &drive, size_t) {
		drive.set_motor_on(head_load);
	});

	// A request to load the head results in a delay until the head is confirmed loaded. This delay is handled
	// in ::run_for. A request to unload the head results in an instant answer that the head is unloaded.
	if(head_load) {
		head_load_request_counter_ = 0;
	} else {
		head_load_request_counter_ = head_load_request_counter_target;
		set_head_loaded(head_load);
	}

	if(observer_) {
		observer_->set_led_status("Microdisc", head_load);
	}
}

void Microdisc::run_for(const Cycles cycles) {
	if(head_load_request_counter_ < head_load_request_counter_target) {
		head_load_request_counter_ += cycles.as_integral();
		if(head_load_request_counter_ >= head_load_request_counter_target) set_head_loaded(true);
	}
	WD::WD1770::run_for(cycles);
}

void Microdisc::set_activity_observer(Activity::Observer *observer) {
	observer_ = observer;
	if(observer) {
		observer->register_led("Microdisc");
		observer_->set_led_status("Microdisc", head_load_request_);
	}
}
