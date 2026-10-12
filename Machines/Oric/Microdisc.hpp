//
//  Microdisc.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/11/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include "Components/1770/1770.hpp"
#include "Activity/Observer.hpp"
#include "DiskController.hpp"

namespace Oric {

class Microdisc: public DiskController {
public:
	Microdisc();

	void set_control_register(uint8_t);

	uint8_t interrupt_request_register() const;
	uint8_t data_request_register() const;
	bool interrupt_request_line() const;

	void run_for(const Cycles);

	void set_activity_observer(Activity::Observer *);

private:
	void set_head_load_request(bool) final;

	void set_control_register(uint8_t control, uint8_t changes);
	uint8_t last_control_ = 0;
	bool irq_enable_ = false;

	Cycles::IntType head_load_request_counter_ = -1;
	bool head_load_request_ = false;

	Activity::Observer *observer_ = nullptr;
};

}
