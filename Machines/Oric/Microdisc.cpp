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
static constexpr std::string_view LED = "Microdisc";
}

Microdisc::Microdisc() : DiskController(P1793, 8000000, Storage::Disk::Drive::ReadyType::ShugartRDY) {
	set_control_register(last_control_, 0xff);
}

void Microdisc::set_control_register(const uint8_t control) {
	const uint8_t changes = last_control_ ^ control;
	last_control_ = control;
	set_control_register(control, changes);
}

void Microdisc::set_control_register(const uint8_t control, const uint8_t changes) {
	//
	// Definition of disc controls.
	//

	// TODO: b2: data separator clock rate select (1 = double)

	// b65: drive select.
	const auto drive = [](const uint8_t value) {
		return (value >> 5) & 3;
	};

	// b4: side select.
	const auto side = [](const uint8_t value) {
		return value & 0x10;
	};

	// b3: double density select (0 = double).
	const auto single_density = [](const uint8_t value) {
		return value & 0x08;
	};

	// b0: IRQ enable.
	const auto irq_enable = [](const uint8_t value) {
		return value & 0x01;
	};

	// b7: EPROM select (0 = select).
	// b1: ROM disable (0 = disable).
	const auto paged_item_selection = [](const uint8_t value) {
		return value & 0x82;
	};
	const auto paged_item = [](const uint8_t value) {
		if(value & 0x02) {
			return PagedItem::BASIC;
		}

		if(value & 0x80) {
			return PagedItem::RAM;
		}

		return PagedItem::DiskROM;
	};

	//
	// Inspect and propagate changes.
	//

	if(drive(changes)) {
		set_drive(1 << drive(control));
	}

	if(side(changes)) {
		const auto head = bool(side(control));
		for_all_drives([head] (Storage::Disk::Drive &drive, size_t) {
			drive.set_head(head);
		});
	}

	if(single_density(changes)) {
		set_is_double_density(!single_density(control));
	}

	if(irq_enable(changes)) {
		const bool had_irq = interrupt_request();
		irq_enable_ = irq_enable(control);
		const bool has_irq = interrupt_request();
		if(has_irq != had_irq && delegate_) {
			delegate_->wd1770_did_change_output(*this);
		}
	}

	if(paged_item_selection(changes)) {
		set_paged_item(paged_item(control));
	}
}

bool Microdisc::interrupt_request() const {
	return irq_enable_ && WD1770::interrupt_request();
}

uint8_t Microdisc::interrupt_request_register() const {
	return 0x7f | (WD1770::interrupt_request() ? 0x00 : 0x80);
}

uint8_t Microdisc::data_request_register() const {
	return 0x7f | (data_request() ? 0x00 : 0x80);
}

void Microdisc::set_head_load_request(const bool head_load) {
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
		observer_->set_led_status(LED, head_load);
	}
}

void Microdisc::run_for(const Cycles cycles) {
	if(head_load_request_counter_ < head_load_request_counter_target) {
		head_load_request_counter_ += cycles.get();
		if(head_load_request_counter_ >= head_load_request_counter_target) set_head_loaded(true);
	}
	WD::WD1770::run_for(cycles);
}

void Microdisc::set_activity_observer(Activity::Observer *const observer) {
	observer_ = observer;
	if(observer) {
		observer->register_led(LED);
		observer_->set_led_status(LED, head_load_request_);
	}
}
