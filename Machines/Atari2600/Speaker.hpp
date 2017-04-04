//
//  Speaker.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/12/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Atari2600_Speaker_hpp
#define Atari2600_Speaker_hpp

#include "../../Outputs/Speaker.hpp"

namespace Atari2600 {

const int CPUTicksPerAudioTick = 38;

class Speaker: public ::Outputs::Filter<Speaker> {
	public:
		Speaker();

		void set_volume(int channel, uint8_t volume);
		void set_divider(int channel, uint8_t divider);
		void set_control(int channel, uint8_t control);

		void get_samples(unsigned int number_of_samples, int16_t *target);

	private:
		uint8_t volume_[2];
		uint8_t divider_[2];
		uint8_t control_[2];

		int poly4_counter_[2];
		int poly5_counter_[2];
		int poly9_counter_[2];
		int output_state_[2];

		int divider_counter_[2];
};

}

#endif /* Speaker_hpp */
