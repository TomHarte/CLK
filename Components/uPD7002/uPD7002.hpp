//
//  uPD7002.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/09/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#include "ClockReceiver/ClockReceiver.hpp"

namespace NEC {

class uPD7002 {
public:
	uPD7002(HalfCycles clock_rate);
	void run_for(HalfCycles);

	bool interrupt() const;

	struct Delegate {
	virtual void did_change_interrupt_status(uPD7002 &) = 0;
	};
	void set_delegate(Delegate *);

	void write(uint16_t address, uint8_t value);
	uint8_t read(uint16_t address);

	void set_input(int channel, float value);

private:
	float inputs_[4]{};
	uint16_t result_ = 0;
	bool interrupt_ = false;

	uint8_t channel_ = 0;
	bool high_precision_ = false;

	HalfCycles conversion_time_remaining_;
	HalfCycles fast_period_, slow_period_;

	uint8_t status() const;
	Delegate *delegate_ = nullptr;
};

}
