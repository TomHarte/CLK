//
//  LinearFilter.c
//  Clock Signal
//
//  Created by Thomas Harte on 01/10/2011.
//  Copyright 2011 Thomas Harte. All rights reserved.
//

#include "FIRFilter.hpp"

#include <cmath>

#ifndef M_PI
#define M_PI 3.1415926f
#endif

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

/*! Evaluates the 0th order Bessel function at @c a. */
float FIRFilter::ino(float a) {
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

void FIRFilter::coefficients_for_idealised_filter_response(short *filter_coefficients, float *A, float attenuation, std::size_t number_of_taps) {
	/* calculate alpha, which is the Kaiser-Bessel window shape factor */
	float a;	// to take the place of alpha in the normal derivation

	if(attenuation < 21.0f) {
		a = 0.0f;
	} else {
		if(attenuation > 50.0f)
			a = 0.1102f * (attenuation - 8.7f);
		else
			a = 0.5842f * powf(attenuation - 21.0f, 0.4f) + 0.7886f * (attenuation - 21.0f);
	}

	std::vector<float> filter_coefficients_float(number_of_taps);

	/* work out the right hand side of the filter coefficients */
	std::size_t Np = (number_of_taps - 1) / 2;
	float I0 = ino(a);
	float Np_squared = float(Np * Np);
	for(unsigned int i = 0; i <= Np; ++i) {
		filter_coefficients_float[Np + i] =
				A[i] *
				ino(a * sqrtf(1.0f - (float(i * i) / Np_squared) )) /
				I0;
	}

	/* coefficients are symmetrical, so copy from right hand side to left side */
	for(std::size_t i = 0; i < Np; ++i) {
		filter_coefficients_float[i] = filter_coefficients_float[number_of_taps - 1 - i];
	}

	/* scale back up so that we retain 100% of input volume */
	float coefficientTotal = 0.0f;
	for(std::size_t i = 0; i < number_of_taps; ++i) {
		coefficientTotal += filter_coefficients_float[i];
	}

	/* we'll also need integer versions, potentially */
	float coefficientMultiplier = 1.0f / coefficientTotal;
	for(std::size_t i = 0; i < number_of_taps; ++i) {
		filter_coefficients[i] = short(filter_coefficients_float[i] * FixedMultiplier * coefficientMultiplier);
	}
}

std::vector<float> FIRFilter::get_coefficients() const {
	std::vector<float> coefficients;
	for(const auto short_coefficient: filter_coefficients_) {
		coefficients.push_back(float(short_coefficient) / FixedMultiplier);
	}
	return coefficients;
}

FIRFilter::FIRFilter(std::size_t number_of_taps, float input_sample_rate, float low_frequency, float high_frequency, float attenuation) {
	// we must be asked to filter based on an odd number of
	// taps, and at least three
	if(number_of_taps < 3) number_of_taps = 3;
	if(attenuation < 21.0f) attenuation = 21.0f;

	// ensure we have an odd number of taps
	number_of_taps |= 1;

	// store instance variables
	filter_coefficients_.resize(number_of_taps);

	/* calculate idealised filter response */
	std::size_t Np = (number_of_taps - 1) / 2;
	float two_over_sample_rate = 2.0f / input_sample_rate;

	// Clamp the high cutoff frequency.
	high_frequency = std::min(high_frequency, input_sample_rate * 0.5f);

	std::vector<float> A(Np+1);
	A[0] = 2.0f * (high_frequency - low_frequency) / input_sample_rate;
	for(unsigned int i = 1; i <= Np; ++i) {
		float i_pi = float(i) * float(M_PI);
		A[i] =
			(
				sinf(two_over_sample_rate * i_pi * high_frequency) -
				sinf(two_over_sample_rate * i_pi * low_frequency)
			) / i_pi;
	}

	FIRFilter::coefficients_for_idealised_filter_response(filter_coefficients_.data(), A.data(), attenuation, number_of_taps);
}

FIRFilter::FIRFilter(const std::vector<float> &coefficients) {
	for(const auto coefficient: coefficients) {
		filter_coefficients_.push_back(short(coefficient * FixedMultiplier));
	}
}

FIRFilter FIRFilter::operator+(const FIRFilter &rhs) const {
	std::vector<float> coefficients = get_coefficients();
	std::vector<float> rhs_coefficients = rhs.get_coefficients();

	std::vector<float> sum;
	for(std::size_t i = 0; i < coefficients.size(); ++i) {
		sum.push_back((coefficients[i] + rhs_coefficients[i]) / 2.0f);
	}

	return FIRFilter(sum);
}

FIRFilter FIRFilter::operator-() const {
	std::vector<float> negative_coefficients;

	for(const auto coefficient: get_coefficients()) {
		negative_coefficients.push_back(1.0f - coefficient);
	}

	return FIRFilter(negative_coefficients);
}

FIRFilter FIRFilter::operator*(const FIRFilter &rhs) const {
	std::vector<float> coefficients = get_coefficients();
	std::vector<float> rhs_coefficients = rhs.get_coefficients();

	std::vector<float> sum;
	for(std::size_t i = 0; i < coefficients.size(); ++i) {
		sum.push_back(coefficients[i] * rhs_coefficients[i]);
	}

	return FIRFilter(sum);
}
