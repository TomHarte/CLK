//
//  Sound.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/11/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#pragma once

#include "ClockReceiver/ClockReceiver.hpp"
#include "Concurrency/AsyncTaskQueue.hpp"
#include "Outputs/Speaker/Implementation/BufferSource.hpp"

#include <cstdint>

namespace Apple::IIgs::Sound {

class GLU {
public:
	GLU(float clock_rate) noexcept;

	void set_control(uint8_t);
	uint8_t control() const;

	void set_data(uint8_t);
	uint8_t data();

	void set_address_low(uint8_t);
	uint8_t address_low() const;
	void set_address_high(uint8_t);
	uint8_t address_high() const;

	void run_for(Cycles);
	bool interrupt() const;

private:
	uint16_t address_ = 0;
	uint8_t data_ = 0;
	uint8_t control_ = 0;
	uint8_t ram_[65536];

	int oscillator_count_ = 1;
	int next_oscillator_ = 0;

	struct Oscillator {
		uint32_t position;

		// Programmer-set values.
		uint16_t velocity;
		uint8_t volume;
		uint8_t address;
		uint8_t control;
		uint8_t table_size;

		// Derived state.
		bool interrupt = false;	// @c true if this channel would request an interrupt were it enabled.

		uint8_t sample(const uint8_t *ram) const;
		int16_t output(const uint8_t *ram) const;
		bool interrupt_active() const;
	} oscillators_[32];

	void set_register(uint16_t address, uint8_t value);
};

}
