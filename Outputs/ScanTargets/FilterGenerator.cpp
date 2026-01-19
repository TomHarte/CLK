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
	DecodingPath decoding_path
) :
	samples_per_line_(samples_per_line),
	subcarrier_frequency_(subcarrier_frequency),
	decoding_path_(decoding_path) {}

float FilterGenerator::radians_per_sample() const {
	return std::numbers::pi_v<float> * 2.0f * subcarrier_frequency_ / samples_per_line_;
}

FilterGenerator::FilterPair FilterGenerator::separation_filter() {
	FilterPair result{};

	// Luminance.
	result.luma =
		SignalProcessing::KaiserBessel::filter<SignalProcessing::ScalarType::Float>(
			MaxKernelSize,
			samples_per_line_,
			subcarrier_frequency_ / 6.0f,	// Based on the broad logic that artefact colour 'sort of' assumes  that
											// subcarrier_frequency_/4 pixels won't be discernable, and /6 is a bit
											// smaller than that. Hands are suitably waved.
			subcarrier_frequency_ * 0.5f
		);

	// Chrominance; attempt to pick the smallest kernel that covers at least one
	// complete cycle of the colour subcarrier.
	const auto chroma_size = size_t(ceil(samples_per_line_ / subcarrier_frequency_)) | 1;
	result.chroma = SignalProcessing::KaiserBessel::filter<SignalProcessing::ScalarType::Float>(
		chroma_size,
		samples_per_line_,
		subcarrier_frequency_,
		samples_per_line_
	);
	SignalProcessing::KaiserBessel::filter<SignalProcessing::ScalarType::Float>(
		chroma_size,
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

	// Don't filter luminance at all.
	const float identity[] = { 1.0f };
	result.luma =
		SignalProcessing::FIRFilter<SignalProcessing::ScalarType::Float>(
			std::begin(identity),
			std::end(identity)
		);

	result.chroma =
		SignalProcessing::KaiserBessel::filter<SignalProcessing::ScalarType::Float>(
			MaxKernelSize,
			samples_per_line_,
			0.0f,
			subcarrier_frequency_ * 0.5f
		)
		* (decoding_path_ == DecodingPath::SVideo ? 2.0f : 0.5f);

	return result;
}
