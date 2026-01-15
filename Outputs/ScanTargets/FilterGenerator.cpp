//
//  FilterGenerator.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/01/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "FilterGenerator.hpp"

using namespace Outputs::Display;

FilterGenerator::FilterGenerator(
	const float samples_per_line,
	const float subcarrier_frequency,
	const int max_kernel_size,
	DecodingPath decoding_path
) :
	samples_per_line_(samples_per_line),
	subcarrier_frequency_(subcarrier_frequency),
	max_kernel_size_(max_kernel_size),
	decoding_path_(decoding_path) {}

FilterGenerator::FilterPair FilterGenerator::separation_filter() {
	const float radians_per_sample =
		(subcarrier_frequency_ * 3.141592654f * 2.0f) / samples_per_line_;

	FilterPair result{};

	// Initial seed: a box filter for the chrominance parts and no filter at all for luminance.
	result.chroma =
		SignalProcessing::Box::filter<SignalProcessing::ScalarType::Float>(
			radians_per_sample,
			3.141592654f * 2.0f
		) * (decoding_path_ == DecodingPath::SVideo ? 2.0f : 1.25f);


//	if(decoding_path_ == DecodingPath::SVideo) {
//		result.luma = FirF
//	}
//
//				// Sharpen the luminance a touch if it was sourced through separation.
//				if(!isSVideoOutput) {
//					SignalProcessing::KaiserBessel::filter<SignalProcessing::ScalarType::Float>(15, 1368, 60.0f, 227.5f)
//						.copy_to<Coefficients3::iterator>(
//							firCoefficients.begin(),
//							firCoefficients.end(),
//							[&](auto destination, float value) {
//								destination->x = value;
//							}
//						);
//					_chromaKernelSize = 15;
//				} else {
//					firCoefficients[7].x = 1.0f;
//				}

	return result;
}

FilterGenerator::FilterPair FilterGenerator::demouldation_filter() {
	return FilterGenerator::FilterPair{};
}
