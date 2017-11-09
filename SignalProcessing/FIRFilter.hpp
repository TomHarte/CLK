//
//  LinearFilter.h
//  Clock Signal
//
//  Created by Thomas Harte on 01/10/2011.
//  Copyright 2011 Thomas Harte. All rights reserved.
//

#ifndef FIRFilter_hpp
#define FIRFilter_hpp

/*

	The FIR filter takes a 1d PCM signal with
	a given sample rate and filters it according
	to a specified filter (band pass only at
	present, more to come if required). The number
	of taps (ie, samples considered simultaneously
	to make an output sample) is configurable;
	smaller numbers permit a filter that operates
	more quickly and with less lag but less
	effectively.

	FIR filters are window functions; expected use is
	to point sample an input that has been subject to
	a filter.

*/

#ifdef __APPLE__
#include <Accelerate/Accelerate.h>
#else
#define kCSKaiserBesselFilterFixedMultiplier	32767.0f
#define kCSKaiserBesselFilterFixedShift			15
#endif

namespace SignalProcessing {

class FIRFilter {
	public:
		/*!
			Creates an instance of @c FIRFilter.

			@param number_of_taps The size of window for input data.
			@param input_sample_rate The sampling rate of the input signal.
			@param low_frequency The lowest frequency of signal to retain in the output.
			@param high_frequency The highest frequency of signal to retain in the output.
			@param attenuation The attenuation of the discarded frequencies.
		*/
		FIRFilter(unsigned int number_of_taps, float input_sample_rate, float low_frequency, float high_frequency, float attenuation);

		~FIRFilter();

		/*! A suggested default attenuation value. */
		constexpr static float DefaultAttenuation = 60.0f;

		/*!
			Applies the filter to one batch of input samples, returning the net result.

			@param src The source buffer to apply the filter to.
			@returns The result of applying the filter.
		*/
		inline short apply(const short *src) {
			#ifdef __APPLE__
				short result;
				vDSP_dotpr_s1_15(filter_coefficients_, 1, src, 1, &result, number_of_taps_);
				return result;
			#else
				int outputValue = 0;
				for(unsigned int c = 0; c < number_of_taps_; c++) {
					outputValue += filter_coefficients_[c] * src[c];
				}
				return (short)(outputValue >> kCSKaiserBesselFilterFixedShift);
			#endif
		}

		inline unsigned int get_number_of_taps() {
			return number_of_taps_;
		}

		void get_coefficients(float *coefficients);

	private:
		short *filter_coefficients_;
		unsigned int number_of_taps_;

		static void coefficients_for_idealised_filter_response(short *filterCoefficients, float *A, float attenuation, unsigned int numberOfTaps);
		static float ino(float a);
};

}

#endif
