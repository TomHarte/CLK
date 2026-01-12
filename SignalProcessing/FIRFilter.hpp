//
//  LinearFilter.h
//  Clock Signal
//
//  Created by Thomas Harte on 01/10/2011.
//  Copyright 2011 Thomas Harte. All rights reserved.
//

#pragma once

// Use the Accelerate framework to vectorise, unless this is a Qt build.
// Primarily that avoids gymnastics in the QMake file; it also eliminates
// a difference in the Qt build across platforms.
#if defined(__APPLE__) && !defined(TARGET_QT)
#include <Accelerate/Accelerate.h>
#define USE_ACCELERATE
#endif

#include <cstddef>
#include <vector>

namespace SignalProcessing {
constexpr float FixedMultiplier = 32767.0f;
constexpr int FixedShift = 15;

/*!
	The FIR filter takes a 1d PCM signal with a given sample rate and applies a band-pass filter to it.

	The number of taps (ie, samples considered simultaneously to make an output sample) is configurable;
	smaller numbers permit a filter that operates more quickly and with less lag but less effectively.
*/
class FIRFilter {
public:
	/*! A suggested default attenuation value. */
	static constexpr float DefaultAttenuation = 60.0f;

	/*!
		Creates an instance of @c FIRFilter.

		@param number_of_taps The size of window for input data.
		@param input_sample_rate The sampling rate of the input signal.
		@param low_frequency The lowest frequency of signal to retain in the output.
		@param high_frequency The highest frequency of signal to retain in the output.
		@param attenuation The attenuation of the discarded frequencies.
	*/
	FIRFilter(
		std::size_t number_of_taps,
		float input_sample_rate,
		float low_frequency,
		float high_frequency,
		float attenuation = DefaultAttenuation
	);
	FIRFilter(const std::vector<float> &coefficients);

	/*!
		Applies the filter to one batch of input samples, returning the net result.

		@param src The source buffer to apply the filter to.
		@returns The result of applying the filter.
	*/
	inline short apply(const short *const src, const size_t stride = 1) const {
		#ifdef USE_ACCELERATE
			short result;
			vDSP_dotpr_s1_15(
				filter_coefficients_.data(),
				1,
				src,
				vDSP_Stride(stride), &result, filter_coefficients_.size()
			);
			return result;
		#else
			int outputValue = 0;
			for(std::size_t c = 0; c < filter_coefficients_.size(); ++c) {
				outputValue += filter_coefficients_[c] * src[c * stride];
			}
			return short(outputValue >> FixedShift);
		#endif
	}

	/*! @returns The number of taps used by this filter. */
	inline std::size_t size() const {
		return filter_coefficients_.size();
	}

	/*! @returns The weighted coefficients that describe this filter. */
	std::vector<float> coefficients() const;

private:
	std::vector<short> filter_coefficients_;
};

}
