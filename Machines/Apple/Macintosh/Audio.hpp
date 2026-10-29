//
//  Audio.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 31/05/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#pragma once

#include "DriveSpeedAccumulator.hpp"
#include "Video.hpp"

#include "Outputs/Speaker/Implementation/LowpassSpeaker.hpp"

namespace Apple::Macintosh {

/*!
	Implements the Macintosh's audio output hardware.

	Designed to be clocked at half the rate of the real hardware — i.e.
	a shade less than 4Mhz.
*/
class Audio {
public:
	Audio(HalfCycles clock_rate, const Video &, DriveSpeedAccumulator &);
	void set_ram(const uint16_t *ram, uint32_t mask);
	void set_use_alternate_buffer(bool);

	/*!
		Macintosh audio is (partly) sourced by the same scanning
		hardware as the video; each line it collects an additional
		word of memory, half of which is used for audio output.

		Use this method to add a newly-collected sample to the queue.
	*/
	void post_sample(uint8_t);

	/*!
		Macintosh audio also separately receives an output volume
		level, in the range 0 to 7.

		Use this method to set the current output volume.
	*/
	void set_volume(int);

	/*!
		A further factor in audio output is the on-off toggle.
	*/
	void set_enabled(bool);

	Outputs::Speaker::Speaker &speaker() {
		return speaker_.speaker();
	}

	void operator += (const HalfCycles);

	void flush();

private:
	Outputs::Speaker::AsyncPushLowpass<false> speaker_;
	std::vector<int16_t> samples_;
	HalfCycles time_;

	// Fetch state.
	size_t address_ = 0;
	bool use_alternate_buffer_ = false;

	AudioPhase phase_;
	const uint16_t *ram_ = nullptr;
	uint32_t ram_mask_ = 0;
	DriveSpeedAccumulator &drive_speed_accumulator_;

	// Emulator-thread stateful variables, to avoid work posting
	// deferral updates if possible.
	int volume_ = 0;
	int enable_mask_ = 0;

	int volume_multiplier_ = 0;
	uint8_t sample_ = 0;
	void set_volume_multiplier();

	void generate();
};

}
