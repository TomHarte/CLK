//
//  DeferredAudio.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/06/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#pragma once

#include "Audio.hpp"
#include "../../../Outputs/Speaker/Implementation/LowpassSpeaker.hpp"

namespace Apple::Macintosh {

struct DeferredAudio {
	Concurrency::AsyncTaskQueue<false> queue;
	Audio audio;
	Outputs::Speaker::PullLowpass<Audio> speaker;
	HalfCycles time_since_update;

	DeferredAudio() : audio(queue), speaker(audio) {}

	void flush() {
		speaker.run_for(queue, time_since_update.flush<Cycles>());
	}
};

}
