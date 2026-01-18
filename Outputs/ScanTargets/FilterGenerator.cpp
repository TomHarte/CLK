//
//  FilterGenerator.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/01/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "FilterGenerator.hpp"

#include <numbers>

using namespace Outputs::Display;

FilterGenerator::FilterGenerator(
	const float samples_per_line,
	const float subcarrier_frequency,
	const size_t max_kernel_size,
	DecodingPath decoding_path
) :
	samples_per_line_(samples_per_line),
	subcarrier_frequency_(subcarrier_frequency),
	max_kernel_size_(max_kernel_size),
	decoding_path_(decoding_path) {}

float FilterGenerator::radians_per_sample() const {
	return std::numbers::pi_v<float> * 2.0f * subcarrier_frequency_ / samples_per_line_;
}

FilterGenerator::FilterPair FilterGenerator::separation_filter() {
	FilterPair result{};

	// Luminance.
	result.luma =
		SignalProcessing::Box::filter<SignalProcessing::ScalarType::Float>(
			radians_per_sample(),
			std::numbers::pi_v<float> * 2.0f
		);

//	Usually provides a better luminance filter, but has issues with in-phase NTSC colour:
//
//		SignalProcessing::KaiserBessel::filter<SignalProcessing::ScalarType::Float>(
//			max_kernel_size_,
//			samples_per_line_,
//			0.0f,
//			subcarrier_frequency_ * 0.5f
//		);

	// Chrominance.
	result.chroma = SignalProcessing::KaiserBessel::filter<SignalProcessing::ScalarType::Float>(
		max_kernel_size_,
		samples_per_line_,
		subcarrier_frequency_,
		samples_per_line_
	);
	SignalProcessing::KaiserBessel::filter<SignalProcessing::ScalarType::Float>(
		max_kernel_size_,
		samples_per_line_,
		0.0f,
		subcarrier_frequency_
	).copy_to<SignalProcessing::FIRFilter<SignalProcessing::ScalarType::Float>::iterator>(
		result.chroma.begin(),
		result.chroma.end(),
		[](const auto iterator, const float value) {
			*iterator -= value;
		}
	);

	return result;
}

FilterGenerator::FilterPair FilterGenerator::demouldation_filter() {
	FilterPair result{};

	if(decoding_path_ == DecodingPath::SVideo) {
		// S-Video: don't filter luminance at all.
		const float identity[] = { 1.0f };
		result.luma =
			SignalProcessing::FIRFilter<SignalProcessing::ScalarType::Float>(
				std::begin(identity),
				std::end(identity)
			);
	} else {
		// Composite: sharpen the luminance a touch.
		result.luma =
			SignalProcessing::KaiserBessel::filter<SignalProcessing::ScalarType::Float>(
				max_kernel_size_, samples_per_line_, 100.0f, subcarrier_frequency_);
	}

	result.chroma =
		SignalProcessing::Box::filter<SignalProcessing::ScalarType::Float>(
			radians_per_sample(),
			std::numbers::pi_v<float> * 2.0f
		)
		* (decoding_path_ == DecodingPath::SVideo ? 2.0f : 0.5f);

//	Usually provides a better chroma filter, but has issues with in-phase NTSC colour:
//
//		SignalProcessing::KaiserBessel::filter<SignalProcessing::ScalarType::Float>(
//			max_kernel_size_,
//			samples_per_line_,
//			0.0f,
//			subcarrier_frequency_ * 0.5f
//		)

	return result;
}
