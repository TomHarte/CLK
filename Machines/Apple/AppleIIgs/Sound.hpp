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

class GLU: public Outputs::Speaker::BufferSource<GLU, false> {	// TODO: isn't this stereo?
public:
	void set_control(uint8_t);
	uint8_t control();

	void set_data(uint8_t);
	uint8_t data();

	void set_address_low(uint8_t);
	uint8_t address_low();
	void set_address_high(uint8_t);
	uint8_t address_high();

	void run_for(Cycles);

	Cycles next_sequence_point() const;
	bool interrupt();

	// SampleSource.
	template <Outputs::Speaker::Action action>
	void apply_samples(std::size_t number_of_samples, Outputs::Speaker::MonoSample *target);
	void set_sample_volume_range(std::int16_t range);
	bool is_zero_level() const { return false; }	// TODO.

private:
	uint16_t address_ = 0;

	struct State {
		uint8_t ram[65536];
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
		} oscillators[32];

		// Some of these aren't actually needed on both threads.
		uint8_t control = 0;
		int oscillator_count = 1;

		void set_register(uint16_t address, uint8_t value);
	} state_;

	// Functions to update an EnsoniqState; these don't belong to the state itself
	// because they also access the pending stores (inter alia).
	template <Outputs::Speaker::Action action>
	void generate_audio(size_t number_of_samples, Outputs::Speaker::MonoSample *target);
//	void skip_audio(EnsoniqState &state, size_t number_of_samples);

	// Audio-thread state.
	int16_t output_range_ = 0;
};

}
