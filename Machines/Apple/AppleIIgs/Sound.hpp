//
//  Sound.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/11/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#pragma once

#include "ClockReceiver/ClockReceiver.hpp"

#include "Outputs/Speaker/Implementation/LowpassSpeaker.hpp"

#include <array>
#include <limits>
#include <cstdint>

namespace Apple::IIgs::Sound {

class GLU {
public:
	GLU(float clock_rate) noexcept;
	auto &speaker() {
		return speaker_.speaker();
	}

	void set_control(uint8_t);
	uint8_t control() const;

	void set_data(uint8_t);
	uint8_t data();

	void set_address_low(uint8_t);
	uint8_t address_low() const;
	void set_address_high(uint8_t);
	uint8_t address_high() const;

	void toggle();

	void run_for(Cycles);
	bool interrupt() const;

	void flush();

private:
	Outputs::Speaker::AsyncPushLowpass<false> speaker_;

	uint16_t address_ = 0;
	uint8_t data_ = 0;
	uint8_t control_ = 0;
	uint8_t ram_[65536];

	int oscillator_count_ = 1;
	int next_oscillator_ = 0;

	static constexpr int16_t ToggleRange = 0x7fff;
	bool toggle_state_ = false;
	int16_t toggle_ = -ToggleRange;

	int16_t output_;

	struct Oscillator {
		uint32_t position = 0;
		bool reset_next = false;

		// Programmer-set values.
		uint16_t velocity = 0;
		uint8_t volume = 0;
		uint8_t address = 0;
		uint8_t control = 1;		// Halt set; interrupt disabled.
		uint8_t table_size = 0;

		// Transient state.
		uint8_t sample = 0;

		int16_t scaled_output(int id, uint8_t contribution) const;
		uint8_t latch_sample(const uint8_t *ram);
	};
	std::array<Oscillator, 32> oscillators_;

	uint32_t interrupts_ = 0;
	void set_interrupt(int);
	void clear_interrupt(int);

	void set_register(uint16_t address, uint8_t value);
};

}
