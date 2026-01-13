//
//  LinearFilter.c
//  Clock Signal
//
//  Created by Thomas Harte on 01/10/2011.
//  Copyright 2011 Thomas Harte. All rights reserved.
//

#include "FIRFilter.hpp"

#include <cmath>
#include <numbers>
#include <numeric>

using namespace SignalProcessing;

/*

	A Kaiser-Bessel filter is a real time window filter. It looks at the last n samples
	of an incoming data source and computes a filtered value, which is the value you'd
	get after applying the specified filter, at the centre of the sampling window.

	Hence, if you request a 37 tap filter then filtering introduces a latency of 18
	samples. Suppose you're receiving input at 44,100Hz and using 4097 taps, then you'll
	introduce a latency of 2048 samples, which is about 46ms.

	There's a correlation between the number of taps and the quality of the filtering.
	More samples = better filtering, at the cost of greater latency. Internally, applying
	the filter involves calculating a weighted sum of previous values, so increasing the
	number of taps is quite cheap in processing terms.

	Original source for this filter:

		"DIGITAL SIGNAL PROCESSING, II", IEEE Press, pages 123-126.
*/

namespace {

/*! Evaluates the 0th order Bessel function at @c a. */
constexpr float ino(const float a) {
	float d = 0.0f;
	float ds = 1.0f;
	float s = 1.0f;

	do {
		d += 2.0f;
		ds *= (a * a) / (d * d);
		s += ds;
	} while(ds > s*1e-6f);

	return s;
}

std::vector<float> coefficients_for_idealised_filter_response(
	const std::vector<float> &A,
	const float attenuation,
	const std::size_t number_of_taps
) {
	/* Calculate alpha, the Kaiser-Bessel window shape factor */
	const float a =	[&] {
		if(attenuation < 21.0f) {
			return 0.0f;
		} else if(attenuation > 50.0f) {
			return 0.1102f * (attenuation - 8.7f);
		} else {
			return 0.5842f * powf(attenuation - 21.0f, 0.4f) + 0.7886f * (attenuation - 21.0f);
		}
	} ();

	std::vector<float> filter_coefficients_float(number_of_taps);

	/* Work out the right hand side of the filter coefficients. */
	const float I0 = ino(a);
	const std::size_t Np = (number_of_taps - 1) / 2;
	const float Np_squared = float(Np * Np);
	for(std::size_t i = 0; i <= Np; ++i) {
		filter_coefficients_float[Np + i] =
				A[i] *
				ino(a * sqrtf(1.0f - (float(i * i) / Np_squared) )) /
				I0;
	}

	/* Coefficients are symmetrical, so copy from right hand side to left. */
	for(std::size_t i = 0; i < Np; ++i) {
		filter_coefficients_float[i] = filter_coefficients_float[number_of_taps - 1 - i];
	}

	/* Scale back up to retain 100% of input volume. */
	const float coefficientTotal =
		std::accumulate(filter_coefficients_float.begin(), filter_coefficients_float.end(), 0.0f);

	/* Hence produce integer versions. */
	const float coefficientMultiplier = 1.0f / coefficientTotal;
	std::vector<float> filter_coefficients;
	filter_coefficients.reserve(number_of_taps);
	for(std::size_t i = 0; i < number_of_taps; ++i) {
		filter_coefficients.push_back(filter_coefficients_float[i] * coefficientMultiplier);
	}
	return filter_coefficients;
}
}

template <ScalarType type>
FIRFilter<type> KaiserBessel::filter(
	size_t number_of_taps,
	const float input_sample_rate,
	const float low_frequency,
	float high_frequency,
	float attenuation
) {
	// Ensure an odd number of taps greater than or equal to 3, with a minimum attenuation of 21.
	number_of_taps = std::max<size_t>(3, number_of_taps) | 1;
	attenuation = std::max(attenuation, 21.0f);

	/* calculate idealised filter response */
	const std::size_t Np = (number_of_taps - 1) / 2;
	const float two_over_sample_rate = 2.0f / input_sample_rate;

	// Clamp the high cutoff frequency.
	high_frequency = std::min(high_frequency, input_sample_rate * 0.5f);

	std::vector<float> A(Np+1);
	A[0] = 2.0f * (high_frequency - low_frequency) / input_sample_rate;
	for(unsigned int i = 1; i <= Np; ++i) {
		const float i_pi = float(i) * std::numbers::pi_v<float>;
		A[i] =
			(
				sinf(two_over_sample_rate * i_pi * high_frequency) -
				sinf(two_over_sample_rate * i_pi * low_frequency)
			) / i_pi;
	}

	const auto idealised = coefficients_for_idealised_filter_response(A, attenuation, number_of_taps);
	return FIRFilter<type>(
		idealised.begin(),
		idealised.end()
	);
}

template
FIRFilter<ScalarType::Int16> KaiserBessel::filter<ScalarType::Int16>(
	size_t number_of_taps,
	const float input_sample_rate,
	const float low_frequency,
	float high_frequency,
	float attenuation
);
template
FIRFilter<ScalarType::Float> KaiserBessel::filter<ScalarType::Float>(
	size_t number_of_taps,
	const float input_sample_rate,
	const float low_frequency,
	float high_frequency,
	float attenuation
);
