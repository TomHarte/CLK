//
//  Sound.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/11/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#pragma once

#include <atomic>

#include "ClockReceiver/ClockReceiver.hpp"
#include "Concurrency/AsyncTaskQueue.hpp"
#include "Outputs/Speaker/Implementation/BufferSource.hpp"

namespace Apple::IIgs::Sound {

class GLU {
public:
	void set_control(uint8_t);
	uint8_t control() const;

	void set_data(uint8_t);
	uint8_t data();

	void set_address_low(uint8_t);
	uint8_t address_low() const;
	void set_address_high(uint8_t);
	uint8_t address_high() const;

	void run_for(Cycles);

	Cycles next_sequence_point() const;
	bool interrupt();

private:
	uint16_t address_ = 0;
	uint8_t data_ = 0;
	uint8_t control_ = 0;
	uint8_t ram_[65536];

	int oscillator_count_ = 1;

	struct Oscillator {
		uint32_t position;

		// Programmer-set values.
		uint16_t velocity;
		uint8_t volume;
		uint8_t address;
		uint8_t control;
		uint8_t table_size;

		// Derived state.
		uint32_t overflow_mask;			// If a non-zero bit gets anywhere into the overflow mask, this channel
										// has wrapped around. It's a function of table_size.
		bool interrupt_request = false;	// Will be non-zero if this channel would request an interrupt, were
										// it currently enabled to do so.

		uint8_t sample(const uint8_t *ram);
		int16_t output(const uint8_t *ram);
	} oscillators_[32];

	void set_register(uint16_t address, uint8_t value);
};

}
