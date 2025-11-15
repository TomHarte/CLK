//
//  SoundGenerator.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/12/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include "Outputs/Speaker/Implementation/BufferSource.hpp"
#include "Outputs/Speaker/SpeakerQueue.hpp"

namespace Electron {

class SoundGenerator: public ::Outputs::Speaker::BufferSource<SoundGenerator, false> {
public:
	SoundGenerator(Outputs::Speaker::TaskQueue &);

	void set_divider(uint8_t);
	void set_is_enabled(bool);

	static constexpr unsigned int clock_rate_divider = 8;

	// For BufferSource.
	template <Outputs::Speaker::Action action>
	void apply_samples(std::size_t number_of_samples, Outputs::Speaker::MonoSample *);
	void set_sample_volume_range(std::int16_t range);

private:
	Outputs::Speaker::TaskQueue &audio_queue_;
	unsigned int counter_ = 0;
	unsigned int divider_ = 0;
	bool is_enabled_ = false;
	unsigned int volume_ = 0;
};

}
