//
//  LinearFilter.h
//  Clock Signal
//
//  Created by Thomas Harte on 01/10/2011.
//  Copyright 2011 Thomas Harte. All rights reserved.
//

#ifndef FIRFilter_hpp
#define FIRFilter_hpp

#ifdef __APPLE__
#include <Accelerate/Accelerate.h>
#endif

#include <vector>

namespace SignalProcessing {

/*!
	The FIR filter takes a 1d PCM signal with a given sample rate and applies a band-pass filter to it.

	The number of taps (ie, samples considered simultaneously to make an output sample) is configurable;
	smaller numbers permit a filter that operates more quickly and with less lag but less effectively.
*/
class FIRFilter {
	private:
		static constexpr float FixedMultiplier = 32767.0f;
		static constexpr int FixedShift = 15;

	public:
		/*! A suggested default attenuation value. */
		constexpr static float DefaultAttenuation = 60.0f;
		/*!
			Creates an instance of @c FIRFilter.

			@param number_of_taps The size of window for input data.
			@param input_sample_rate The sampling rate of the input signal.
			@param low_frequency The lowest frequency of signal to retain in the output.
			@param high_frequency The highest frequency of signal to retain in the output.
			@param attenuation The attenuation of the discarded frequencies.
		*/
		FIRFilter(std::size_t number_of_taps, float input_sample_rate, float low_frequency, float high_frequency, float attenuation = DefaultAttenuation);
		FIRFilter(const std::vector<float> &coefficients);

		/*!
			Applies the filter to one batch of input samples, returning the net result.

			@param src The source buffer to apply the filter to.
			@returns The result of applying the filter.
		*/
		inline short apply(const short *src, size_t stride = 1) const {
			#ifdef __APPLE__
				short result;
				vDSP_dotpr_s1_15(filter_coefficients_.data(), 1, src, vDSP_Stride(stride), &result, filter_coefficients_.size());
				return result;
			#else
				int outputValue = 0;
				for(std::size_t c = 0; c < filter_coefficients_.size(); ++c) {
					outputValue += filter_coefficients_[c] * src[c * stride];
				}
				return static_cast<short>(outputValue >> FixedShift);
			#endif
		}

		/*! @returns The number of taps used by this filter. */
		inline std::size_t get_number_of_taps() const {
			return filter_coefficients_.size();
		}

		/*! @returns The weighted coefficients that describe this filter. */
		std::vector<float> get_coefficients() const;

		/*!
			@returns A filter that would have the effect of adding (and scaling) the outputs of the two filters.
			Defined only if both have the same number of taps.
		*/
		FIRFilter operator+(const FIRFilter &) const;

		/*!
			@returns A filter that would have the effect of applying the two filters in succession.
			Defined only if both have the same number of taps.
		*/
		FIRFilter operator*(const FIRFilter &) const;

		/*!
			@returns A filter that would have the opposite effect of this filter.
		*/
		FIRFilter operator-() const;

	private:
		std::vector<short> filter_coefficients_;

		static void coefficients_for_idealised_filter_response(short *filterCoefficients, float *A, float attenuation, std::size_t numberOfTaps);
		static float ino(float a);
};

}

#endif
