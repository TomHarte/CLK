//
//  MultiSpeaker.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/02/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#ifndef MultiSpeaker_hpp
#define MultiSpeaker_hpp

#include "../../../../Machines/DynamicMachine.hpp"
#include "../../../../Outputs/Speaker/Speaker.hpp"

#include <memory>
#include <vector>

namespace Analyser {
namespace Dynamic {

class MultiSpeaker: public Outputs::Speaker::Speaker, Outputs::Speaker::Speaker::Delegate {
	public:
		static MultiSpeaker *create(const std::vector<std::unique_ptr<::Machine::DynamicMachine>> &machines);
		MultiSpeaker(const std::vector<Outputs::Speaker::Speaker *> &speakers);

		void set_new_front_machine(::Machine::DynamicMachine *machine);

		float get_ideal_clock_rate_in_range(float minimum, float maximum);
		void set_output_rate(float cycles_per_second, int buffer_size);
		void set_delegate(Outputs::Speaker::Speaker::Delegate *delegate);
		void speaker_did_complete_samples(Speaker *speaker, const std::vector<int16_t> &buffer);

	private:
		std::vector<Outputs::Speaker::Speaker *> speakers_;
		Outputs::Speaker::Speaker *front_speaker_ = nullptr;
		Outputs::Speaker::Speaker::Delegate *delegate_ = nullptr;
};

}
}

#endif /* MultiSpeaker_hpp */
