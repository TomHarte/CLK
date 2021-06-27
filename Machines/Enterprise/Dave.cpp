//
//  Dave.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/06/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Dave.hpp"

using namespace Enterprise;

Dave::Dave(Concurrency::DeferringAsyncTaskQueue &audio_queue) :
	audio_queue_(audio_queue) {}

void Dave::write(uint16_t address, uint8_t value) {
	address &= 0xf;
	audio_queue_.defer([address, value, this] {
		switch(address) {
			case 0:	case 2:	case 4:
				channels_[address >> 1].reload = (channels_[address >> 1].reload & 0xff00) | value;
			break;
			case 1:	case 3:	case 5:
				channels_[address >> 1].reload = uint16_t((channels_[address >> 1].reload & 0x00ff) | ((value & 0xf) << 4));
				channels_[address >> 1].distortion = Channel::Distortion((value >> 4)&3);
				channels_[address >> 1].high_pass = value & 0x40;
				channels_[address >> 1].ring_modulate = value & 0x80;
			break;

			// TODO:
			//
			//	6:	noise selection.
			//	7:	sync bits, optional D/As, interrupt rate (albeit some of that shouldn't be on this thread, or possibly even _here_).
			//	11, 14:	noise amplitude.

			case 8: case 9: case 10:
				channels_[address - 8].amplitude[0] = value & 0x3f;
			break;
			case 11: case 12: case 13:
				channels_[address - 11].amplitude[1] = value & 0x3f;
			break;
		}
	});
}

void Dave::set_sample_volume_range(int16_t range) {
	audio_queue_.defer([range, this] {
		volume_ = range / (63*3);
	});
}

void Dave::get_samples(std::size_t number_of_samples, int16_t *target) {
	// Step 1: divide input clock to 125,000 Hz (?)
	for(size_t c = 0; c < number_of_samples; c++) {
#define update_channel(x)								\
		if(!channels_[x].count) {						\
			channels_[x].output ^= true;				\
			channels_[x].count = channels_[x].reload;	\
		} else {										\
			--channels_[x].count;						\
		}

		update_channel(0);
		update_channel(1);
		update_channel(2);

#undef update_channel

		// Dumbest ever first attempt: sum channels.
		target[(c << 1) + 0] =
			volume_ * (
				channels_[0].amplitude[0] * channels_[0].output +
				channels_[1].amplitude[0] * channels_[1].output +
				channels_[2].amplitude[0] * channels_[2].output
			);

		target[(c << 1) + 1] =
			volume_ * (
				channels_[0].amplitude[1] * channels_[0].output +
				channels_[1].amplitude[1] * channels_[1].output +
				channels_[2].amplitude[1] * channels_[2].output
			);
	}
}
