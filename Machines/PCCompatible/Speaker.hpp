//
//  Speaker.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/03/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include "Components/AudioToggle/AudioToggle.hpp"
#include "Concurrency/AsyncTaskQueue.hpp"
#include "Outputs/Speaker/Implementation/LowpassSpeaker.hpp"

namespace PCCompatible {

struct PCSpeaker {
	PCSpeaker() :
		toggle(queue),
		speaker(toggle) {}

	void update() {
		speaker.run_for(queue, cycles_since_update);
		cycles_since_update = 0;
	}

	void set_pit(const bool pit_input) {
		pit_input_ = pit_input;
		set_level();
	}

	void set_control(const bool pit_mask, const bool level) {
		pit_mask_ = pit_mask;
		level_ = level;
		set_level();
	}

	void set_level() {
		// TODO: I think pit_mask_ actually acts as the gate input to the PIT.
		const bool new_output = (!pit_mask_ | pit_input_) & level_;

		if(new_output != output_) {
			update();
			toggle.set_output(new_output);
			output_ = new_output;
		}
	}

	Concurrency::AsyncTaskQueue<false> queue;
	Audio::Toggle toggle;
	Outputs::Speaker::PullLowpass<Audio::Toggle> speaker;
	Cycles cycles_since_update = 0;

	bool pit_input_ = false;
	bool pit_mask_ = false;
	bool level_ = false;
	bool output_ = false;
};

}
