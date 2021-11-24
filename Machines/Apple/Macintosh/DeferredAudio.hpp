//
//  DeferredAudio.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/06/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef DeferredAudio_h
#define DeferredAudio_h

#include "Audio.hpp"
#include "../../../Outputs/Speaker/Implementation/LowpassSpeaker.hpp"

namespace Apple {
namespace Macintosh {

struct DeferredAudio {
	Concurrency::DeferringAsyncTaskQueue queue;
	Audio audio;
	Outputs::Speaker::PullLowpass<Audio> speaker;
	HalfCycles time_since_update;

	DeferredAudio() : audio(queue), speaker(audio) {}

	void flush() {
		speaker.run_for(queue, time_since_update.flush<Cycles>());
	}
};

}
}

#endif /* DeferredAudio_h */
