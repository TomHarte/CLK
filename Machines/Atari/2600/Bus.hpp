//
//  Bus.h
//  Clock Signal
//
//  Created by Thomas Harte on 18/03/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef Atari2600_Bus_hpp
#define Atari2600_Bus_hpp

#include "Atari2600.hpp"
#include "PIA.hpp"
#include "TIA.hpp"
#include "TIASound.hpp"

#include "../../../Analyser/Dynamic/ConfidenceCounter.hpp"
#include "../../../ClockReceiver/ClockReceiver.hpp"
#include "../../../Outputs/Speaker/Implementation/LowpassSpeaker.hpp"

namespace Atari2600 {

class Bus {
	public:
		Bus() :
			tia_sound_(audio_queue_),
			speaker_(tia_sound_) {}

		virtual ~Bus() {
			audio_queue_.flush();
		}

		virtual void run_for(const Cycles cycles) = 0;
		virtual void apply_confidence(Analyser::Dynamic::ConfidenceCounter &confidence_counter) = 0;
		virtual void set_reset_line(bool state) = 0;
		virtual void flush() = 0;

		// the RIOT, TIA and speaker
		PIA mos6532_;
		TIA tia_;

		Concurrency::DeferringAsyncTaskQueue audio_queue_;
		TIASound tia_sound_;
		Outputs::Speaker::LowpassSpeaker<TIASound> speaker_;

		// joystick state
		uint8_t tia_input_value_[2] = {0xff, 0xff};

	protected:
		// speaker backlog accumlation counter
		Cycles cycles_since_speaker_update_;
		inline void update_audio() {
			speaker_.run_for(audio_queue_, cycles_since_speaker_update_.divide(Cycles(CPUTicksPerAudioTick * 3)));
		}

		// video backlog accumulation counter
		Cycles cycles_since_video_update_;
		inline void update_video() {
			tia_.run_for(cycles_since_video_update_.flush<Cycles>());
		}

		// RIOT backlog accumulation counter
		Cycles cycles_since_6532_update_;
		inline void update_6532() {
			mos6532_.run_for(cycles_since_6532_update_.flush<Cycles>());
		}
};

}

#endif /* Atari2600_Bus_hpp */
