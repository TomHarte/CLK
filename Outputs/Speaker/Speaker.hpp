//
//  Speaker.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/01/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef Speaker_hpp
#define Speaker_hpp

#include <cstdint>
#include <vector>

namespace Outputs {
namespace Speaker {

/*!
	Provides a communication point for sound; machines that have a speaker provide an
	audio output.
*/
class Speaker {
	public:
		virtual ~Speaker() {}

		virtual float get_ideal_clock_rate_in_range(float minimum, float maximum) = 0;
		virtual void set_output_rate(float cycles_per_second, int buffer_size) = 0;

		int completed_sample_sets() const { return completed_sample_sets_; }

		struct Delegate {
			virtual void speaker_did_complete_samples(Speaker *speaker, const std::vector<int16_t> &buffer) = 0;
			virtual void speaker_did_change_input_clock(Speaker *speaker) {}
		};
		virtual void set_delegate(Delegate *delegate) {
			delegate_ = delegate;
		}

	protected:
		void did_complete_samples(Speaker *speaker, const std::vector<int16_t> &buffer) {
			++completed_sample_sets_;
			delegate_->speaker_did_complete_samples(this, buffer);
		}
		Delegate *delegate_ = nullptr;
		int completed_sample_sets_ = 0;
};

}
}

#endif /* Speaker_hpp */
