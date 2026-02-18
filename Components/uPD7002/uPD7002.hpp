//
//  uPD7002.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/09/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include "ClockReceiver/ClockReceiver.hpp"

#include <atomic>

namespace NEC {

class uPD7002 {
public:
	/// Constructs a PD7002 that will receive @c run_for updates at the specified clock rate.
	uPD7002(HalfCycles clock_rate);
	void run_for(HalfCycles);

	/// @returns The current state of the interrupt line.
	bool interrupt() const;

	/// Defines a mean for an observer to receive notifications upon updates to the interrupt line.
	struct Delegate {
		virtual void did_change_interrupt_status(uPD7002 &) = 0;
	};
	void set_delegate(Delegate *);

	void write(uint16_t address, uint8_t value);
	uint8_t read(uint16_t address);

	/// Sets the floating point value, which should be in the range [0.0, 1.0], for the signal currently
	/// being supplied to @c channel.
	void set_input(int channel, float value);

private:
	std::atomic<float> inputs_[4]{};
	uint16_t result_ = 0;
	bool interrupt_ = false;

	uint8_t channel_ = 0, spare_ = 0;
	bool high_precision_ = false;

	HalfCycles conversion_time_remaining_{};
	HalfCycles fast_period_, slow_period_;

	uint8_t status() const;
	void set_interrupt(bool);
	Delegate *delegate_ = nullptr;
};

}
