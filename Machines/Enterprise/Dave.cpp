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
			case 6:
				noise_.frequency = Noise::Frequency(value&3);
				noise_.polynomial = Noise::Polynomial((value >> 2)&3);
				noise_.swap_polynomial = value & 0x10;
				noise_.low_pass = value & 0x20;
				noise_.high_pass = value & 0x40;
				noise_.ring_modulate = value & 0x80;
			break;

			// TODO:
			//
			//	7:	sync bits, optional D/As.
			//
			//		(will handle interrupt bits elsewhere)

			case 8: case 9: case 10:
				channels_[address - 8].amplitude[0] = value & 0x3f;
			break;
			case 12: case 13: case 14:
				channels_[address - 12].amplitude[1] = value & 0x3f;
			break;
			case 11:	noise_.amplitude[0] = value & 0x3f;		break;
			case 15:	noise_.amplitude[1] = value & 0x3f;		break;
		}
	});
}

void Dave::set_sample_volume_range(int16_t range) {
	audio_queue_.defer([range, this] {
		volume_ = range / (63*4);
	});
}

void Dave::get_samples(std::size_t number_of_samples, int16_t *target) {
	for(size_t c = 0; c < number_of_samples; c++) {
		poly_state_[int(Channel::Distortion::FourBit)] = poly4_.next();
		poly_state_[int(Channel::Distortion::FiveBit)] = poly5_.next();
		poly_state_[int(Channel::Distortion::SevenBit)] = poly7_.next();
		if(noise_.swap_polynomial) {
			poly_state_[int(Channel::Distortion::SevenBit)] = poly_state_[int(Channel::Distortion::None)];
		}

		// Update tone channels.
#define update_channel(x) {									\
		auto output = channels_[x].output & 1;				\
		channels_[x].output <<= 1;							\
		if(!channels_[x].count) {							\
			channels_[x].count = channels_[x].reload;		\
															\
			if(channels_[x].distortion == Channel::Distortion::None)	\
				output ^= 1;								\
			else											\
				output = poly_state_[int(channels_[x].distortion)];	\
															\
			if(channels_[x].high_pass && (channels_[(x+1)%3].output&3) == 2) {	\
				output = 0;									\
			}												\
			if(channels_[x].ring_modulate) {				\
				output = ~(output ^ channels_[(x+2)%3].output) & 1;	\
			}												\
		} else {											\
			--channels_[x].count;							\
		}													\
		channels_[x].output |= output;						\
	}

		update_channel(0);
		update_channel(1);
		update_channel(2);

#undef update_channel

		// TODO: update noise channel.

		// Dumbest ever first attempt: sum channels.
		target[(c << 1) + 0] =
			volume_ * (
				channels_[0].amplitude[0] * (channels_[0].output & 1) +
				channels_[1].amplitude[0] * (channels_[1].output & 1) +
				channels_[2].amplitude[0] * (channels_[2].output & 1) +
				noise_.amplitude[0] * noise_.output
			);

		target[(c << 1) + 1] =
			volume_ * (
				channels_[0].amplitude[1] * (channels_[0].output & 1) +
				channels_[1].amplitude[1] * (channels_[1].output & 1) +
				channels_[2].amplitude[1] * (channels_[2].output & 1) +
				noise_.amplitude[1] * noise_.output
			);
	}
}
