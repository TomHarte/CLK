//
//  LinearFilter.c
//  Clock Signal
//
//  Created by Thomas Harte on 01/10/2011.
//  Copyright 2011 Thomas Harte. All rights reserved.
//

#include "FIRFilter.hpp"
#include <math.h>

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

		"DIGITAL SIGNAL PROCESSING, II", IEEE Press, pages 123–126.
*/


// our little fixed point scheme
#define kCSKaiserBesselFilterFixedMultiplier	32767.0f
#define kCSKaiserBesselFilterFixedShift			15

/* ino evaluates the 0th order Bessel function at a */
float FIRFilter::ino(float a)
{
	float d = 0.0f;
	float ds = 1.0f;
	float s = 1.0f;

	do
	{
		d += 2.0f;
		ds *= (a * a) / (d * d);
		s += ds;
	}
	while(ds > s*1e-6f);

	return s;
}

//static void csfilter_setIdealisedFilterResponse(short *filterCoefficients, float *A, float attenuation, unsigned int numberOfTaps)
void FIRFilter::coefficients_for_idealised_filter_response(short *filterCoefficients, float *A, float attenuation, unsigned int numberOfTaps)
{
	/* calculate alpha, which is the Kaiser-Bessel window shape factor */
	float a;	// to take the place of alpha in the normal derivation

	if(attenuation < 21.0f)
		a = 0.0f;
	else
	{
		if(attenuation > 50.0f)
			a = 0.1102f * (attenuation - 8.7f);
		else
			a = 0.5842f * powf(attenuation - 21.0f, 0.4f) + 0.7886f * (attenuation - 21.0f);
	}

	float *filterCoefficientsFloat = new float[numberOfTaps];

	/* work out the right hand side of the filter coefficients */
	unsigned int Np = (numberOfTaps - 1) / 2;
	float I0 = ino(a);
	float NpSquared = (float)(Np * Np);
	for(unsigned int i = 0; i <= Np; i++)
	{
		filterCoefficientsFloat[Np + i] = 
				A[i] * 
				ino(a * sqrtf(1.0f - ((float)(i * i) / NpSquared) )) /
				I0;
	}

	/* coefficients are symmetrical, so copy from right hand side to left side */
	for(unsigned int i = 0; i < Np; i++)
	{
		filterCoefficientsFloat[i] = filterCoefficientsFloat[numberOfTaps - 1 - i];
	}
	
	/* scale back up so that we retain 100% of input volume */
	float coefficientTotal = 0.0f;
	for(unsigned int i = 0; i < numberOfTaps; i++)
	{
		coefficientTotal += filterCoefficientsFloat[i];
	}

	/* we'll also need integer versions, potentially */
	float coefficientMultiplier = 1.0f / coefficientTotal;
	for(unsigned int i = 0; i < numberOfTaps; i++)
	{
		filterCoefficients[i] = (short)(filterCoefficientsFloat[i] * kCSKaiserBesselFilterFixedMultiplier * coefficientMultiplier);
	}

	delete[] filterCoefficientsFloat;
}

void FIRFilter::get_coefficients(float *coefficients)
{
	for(unsigned int i = 0; i < number_of_taps_; i++)
	{
		coefficients[i] = (float)filter_coefficients_[i] / kCSKaiserBesselFilterFixedMultiplier;
	}
}

FIRFilter::FIRFilter(unsigned int number_of_taps, unsigned int input_sample_rate, float low_frequency, float high_frequency, float attenuation)
{
	// we must be asked to filter based on an odd number of
	// taps, and at least three
	if(number_of_taps < 3) number_of_taps = 3;
	if(attenuation < 21.0f) attenuation = 21.0f;

	// ensure we have an odd number of taps
	number_of_taps |= 1;

	// store instance variables
	number_of_taps_ = number_of_taps;
	filter_coefficients_ = new short[number_of_taps_];

	/* calculate idealised filter response */
	unsigned int Np = (number_of_taps - 1) / 2;
	float twoOverSampleRate = 2.0f / (float)input_sample_rate;

	float *A = new float[Np+1];
	A[0] = 2.0f * (high_frequency - low_frequency) / (float)input_sample_rate;
	for(unsigned int i = 1; i <= Np; i++)
	{
		float iPi = (float)i * (float)M_PI;
		A[i] = 
			(
				sinf(twoOverSampleRate * iPi * high_frequency) -
				sinf(twoOverSampleRate * iPi * low_frequency)
			) / iPi;
	}

	FIRFilter::coefficients_for_idealised_filter_response(filter_coefficients_, A, attenuation, number_of_taps_);

	/* clean up */
	delete[] A;
}

FIRFilter::~FIRFilter()
{
	delete[] filter_coefficients_;
}
