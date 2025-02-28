//
//  KonamiSCC.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/01/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#pragma once

#include "Outputs/Speaker/Implementation/BufferSource.hpp"
#include "Concurrency/AsyncTaskQueue.hpp"

namespace Konami {

/*!
	Provides an emulation of Konami's Sound Creative Chip ('SCC').

	The SCC is a primitive wavetable synthesis chip, offering 32-sample tables,
	and five channels of output. The original SCC uses the same wave for channels
	four and five, the SCC+ supports different waves for the two channels.
*/
class SCC: public ::Outputs::Speaker::BufferSource<SCC, false> {
public:
	/// Creates a new SCC.
	SCC(Concurrency::AsyncTaskQueue<false> &);

	/// As per ::SampleSource; provides a broadphase test for silence.
	bool is_zero_level() const;

	/// As per ::SampleSource; provides audio output.
	template <Outputs::Speaker::Action action>
	void apply_samples(std::size_t number_of_samples, Outputs::Speaker::MonoSample *);
	void set_sample_volume_range(std::int16_t);

	/// Writes to the SCC.
	void write(uint16_t address, uint8_t value);

	/// Reads from the SCC.
	uint8_t read(uint16_t address) const;

private:
	Concurrency::AsyncTaskQueue<false> &task_queue_;

	// State from here on down is accessed ony from the audio thread.
	int master_divider_ = 0;
	std::int16_t master_volume_ = 0;
	Outputs::Speaker::MonoSample transient_output_level_ = 0;

	struct Channel {
		int period = 0;
		int amplitude = 0;

		int tone_counter = 0;
		int offset = 0;
	} channels_[5];

	struct Wavetable {
		std::uint8_t samples[32];
	} waves_[4];

	std::uint8_t channel_enable_ = 0;

	void evaluate_output_volume();

	// This keeps a copy of wave memory that is accessed from the
	// main emulation thread.
	std::uint8_t ram_[128];
};

}
