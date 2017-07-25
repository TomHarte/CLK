//
//  Bus.h
//  Clock Signal
//
//  Created by Thomas Harte on 18/03/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef Atari2600_Bus_hpp
#define Atari2600_Bus_hpp

#include "Atari2600.hpp"
#include "PIA.hpp"
#include "Speaker.hpp"
#include "TIA.hpp"
#include "../../Components/ClockReceiver.hpp"

namespace Atari2600 {

class Bus {
	public:
		Bus() :
			tia_input_value_{0xff, 0xff},
			cycles_since_speaker_update_(0),
			cycles_since_video_update_(0),
			cycles_since_6532_update_(0) {}

		virtual void run_for(const Cycles &cycles) = 0;
		virtual void set_reset_line(bool state) = 0;

		// the RIOT, TIA and speaker
		PIA mos6532_;
		std::shared_ptr<TIA> tia_;
		std::shared_ptr<Speaker> speaker_;

		// joystick state
		uint8_t tia_input_value_[2];

	protected:
		// speaker backlog accumlation counter
		unsigned int cycles_since_speaker_update_;
		inline void update_audio() {
			unsigned int audio_cycles = cycles_since_speaker_update_ / (CPUTicksPerAudioTick * 3);
			cycles_since_speaker_update_ %= (CPUTicksPerAudioTick * 3);
			speaker_->run_for(Cycles((int)audio_cycles));
		}

		// video backlog accumulation counter
		unsigned int cycles_since_video_update_;
		inline void update_video() {
			tia_->run_for(Cycles((int)cycles_since_video_update_));
			cycles_since_video_update_ = 0;
		}

		// RIOT backlog accumulation counter
		unsigned int cycles_since_6532_update_;
		inline void update_6532() {
			mos6532_.run_for(Cycles((int)cycles_since_6532_update_));
			cycles_since_6532_update_ = 0;
		}
};

}

#endif /* Atari2600_Bus_hpp */
