//
//  MultiSpeaker.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/02/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#ifndef MultiSpeaker_hpp
#define MultiSpeaker_hpp

#include "../../../../Machines/DynamicMachine.hpp"
#include "../../../../Outputs/Speaker/Speaker.hpp"

#include <memory>
#include <mutex>
#include <vector>

namespace Analyser {
namespace Dynamic {

/*!
	Provides a class that multiplexes calls to and from Outputs::Speaker::Speaker in order
	transparently to connect a single caller to multiple destinations.

	Makes a static internal copy of the list of machines; expects the owner to keep it
	abreast of the current frontmost machine.
*/
class MultiSpeaker: public Outputs::Speaker::Speaker, Outputs::Speaker::Speaker::Delegate {
	public:
		/*!
			Provides a construction mechanism that may return nullptr, in the case that all included
			machines return nullptr as their speaker.
		*/
		static MultiSpeaker *create(const std::vector<std::unique_ptr<::Machine::DynamicMachine>> &machines);

		/// This class requires the caller to nominate changes in the frontmost machine.
		void set_new_front_machine(::Machine::DynamicMachine *machine);

		// Below is the standard Outputs::Speaker::Speaker interface; see there for documentation.
		float get_ideal_clock_rate_in_range(float minimum, float maximum) override;
		void set_computed_output_rate(float cycles_per_second, int buffer_size, bool stereo) override;
		void set_delegate(Outputs::Speaker::Speaker::Delegate *delegate) override;

	private:
		void speaker_did_complete_samples(Speaker *speaker, const std::vector<int16_t> &buffer) final;
		void speaker_did_change_input_clock(Speaker *speaker) final;
		MultiSpeaker(const std::vector<Outputs::Speaker::Speaker *> &speakers);

		std::vector<Outputs::Speaker::Speaker *> speakers_;
		Outputs::Speaker::Speaker *front_speaker_ = nullptr;
		Outputs::Speaker::Speaker::Delegate *delegate_ = nullptr;
		std::mutex front_speaker_mutex_;

		bool is_stereo_ = false;
};

}
}

#endif /* MultiSpeaker_hpp */
