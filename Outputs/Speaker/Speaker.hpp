//
//  Speaker.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/01/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef Speaker_hpp
#define Speaker_hpp

#include <atomic>
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

		/*!
			@returns The best output clock rate for the audio being supplied to this speaker, from the range given.
		*/
		virtual float get_ideal_clock_rate_in_range(float minimum, float maximum) = 0;

		/*!
			@returns @c true if the device would most ideally output stereo sound; @c false otherwise.
		*/
		virtual bool get_is_stereo() = 0;

		/*!
			Sets the actual output rate; packets provided to the delegate will conform to these
			specifications regardless of the input.
		*/
		void set_output_rate(float cycles_per_second, int buffer_size, bool stereo) {
			output_cycles_per_second_ = cycles_per_second;
			output_buffer_size_ = buffer_size;
			stereo_output_ = stereo;
			compute_output_rate();
		}

		/// Sets the output volume, in the range [0, 1].
		virtual void set_output_volume(float) = 0;

		/*!
			Speeds a speed multiplier for this machine, e.g. that it is currently being run at 2.0x its normal rate.
			This will affect the number of input samples that are combined to produce one output sample.
		*/
		void set_input_rate_multiplier(float multiplier) {
			input_rate_multiplier_ = multiplier;
			compute_output_rate();
		}

		/*!
			@returns The number of sample sets so far delivered to the delegate.
		*/
		int completed_sample_sets() const { return completed_sample_sets_; }

		/*!
			Defines a receiver for audio packets.
		*/
		struct Delegate {
			/*!
				Indicates that a new audio packet is ready. If the output is stereo, samples will be interleaved with the first
				being left, the second being right, etc.
			*/
			virtual void speaker_did_complete_samples(Speaker *speaker, const std::vector<int16_t> &buffer) = 0;

			/*!
				Provides the delegate with a hint that the input clock rate has changed, which provides an opportunity to
				renegotiate the ideal clock rate, if desired.
			*/
			virtual void speaker_did_change_input_clock(Speaker *speaker) {}
		};
		virtual void set_delegate(Delegate *delegate) {
			delegate_ = delegate;
		}


		// This is primarily exposed for MultiSpeaker et al; it's not for general callers.
		virtual void set_computed_output_rate(float cycles_per_second, int buffer_size, bool stereo) = 0;

	protected:
		void did_complete_samples(Speaker *speaker, const std::vector<int16_t> &buffer, bool is_stereo) {
			// Test the delegate for existence again, as it may have changed.
			const auto delegate = delegate_.load();
			if(!delegate) return;

			++completed_sample_sets_;

			// Hope for the fast path first: producer and consumer agree about
			// number of channels.
			if(is_stereo == stereo_output_) {
				delegate->speaker_did_complete_samples(this, buffer);
				return;
			}

			// Producer and consumer don't agree, so mix two channels to one, or double out one to two.
			if(is_stereo) {
				// Mix down.
				mix_buffer_.resize(buffer.size() / 2);
				for(size_t c = 0; c < mix_buffer_.size(); ++c) {
					mix_buffer_[c] = (buffer[(c << 1) + 0] + buffer[(c << 1) + 1]) >> 1;
					// TODO: is there an Accelerate framework solution to this?
				}
			} else {
				// Double up.
				mix_buffer_.resize(buffer.size() * 2);
				for(size_t c = 0; c < buffer.size(); ++c) {
					mix_buffer_[(c << 1) + 0] = mix_buffer_[(c << 1) + 1] = buffer[c];
				}
			}
			delegate->speaker_did_complete_samples(this, mix_buffer_);
		}
		std::atomic<Delegate *> delegate_ = nullptr;

	private:
		void compute_output_rate() {
			// The input rate multiplier is actually used as an output rate divider,
			// to confirm to the public interface of a generic speaker being output-centric.
			set_computed_output_rate(output_cycles_per_second_ / input_rate_multiplier_, output_buffer_size_, stereo_output_);
		}

		int completed_sample_sets_ = 0;
		float input_rate_multiplier_ = 1.0f;
		float output_cycles_per_second_ = 1.0f;
		int output_buffer_size_ = 1;
		std::atomic<bool> stereo_output_ = false;
		std::vector<int16_t> mix_buffer_;
};

}
}

#endif /* Speaker_hpp */
