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
		void set_output_rate(float cycles_per_second, int buffer_size) {
			output_cycles_per_second_ = cycles_per_second;
			output_buffer_size_ = buffer_size;
			compute_output_rate();
		}
		void set_input_rate_multiplier(float multiplier) {
			input_rate_multiplier_ = multiplier;
			compute_output_rate();
		}

		int completed_sample_sets() const { return completed_sample_sets_; }

		struct Delegate {
			virtual void speaker_did_complete_samples(Speaker *speaker, const std::vector<int16_t> &buffer) = 0;
			virtual void speaker_did_change_input_clock(Speaker *speaker) {}
		};
		virtual void set_delegate(Delegate *delegate) {
			delegate_ = delegate;
		}

		virtual void set_computed_output_rate(float cycles_per_second, int buffer_size) = 0;

	protected:
		void did_complete_samples(Speaker *speaker, const std::vector<int16_t> &buffer) {
			++completed_sample_sets_;
			delegate_->speaker_did_complete_samples(this, buffer);
		}
		Delegate *delegate_ = nullptr;

	private:
		void compute_output_rate() {
			// The input rate multiplier is actually used as an output rate divider,
			// to confirm to the public interface of a generic speaker being output-centric.
			set_computed_output_rate(output_cycles_per_second_ / input_rate_multiplier_, output_buffer_size_);
		}

		int completed_sample_sets_ = 0;
		float input_rate_multiplier_ = 1.0f;
		float output_cycles_per_second_ = 1.0f;
		int output_buffer_size_ = 1;
};

}
}

#endif /* Speaker_hpp */
