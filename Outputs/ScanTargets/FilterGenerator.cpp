//
//  FilterGenerator.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/01/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "FilterGenerator.hpp"

#include <cmath>
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

FilterGenerator::FilterPair FilterGenerator::separation_filter() const {
	FilterPair result{};

	// Luminance.
	result.luma =
		SignalProcessing::KaiserBessel::filter<SignalProcessing::ScalarType::Float>(
			MaxKernelSize,
			samples_per_line_,
			0.0f,
			subcarrier_frequency_ * 0.55f
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

FilterGenerator::FilterPair FilterGenerator::demouldation_filter() const {
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
			subcarrier_frequency_ * 0.1f,
			subcarrier_frequency_ * 0.25f
		)
		* (decoding_path_ == DecodingPath::SVideo ? 2.0f : 1.0f);

	return result;
}

float FilterGenerator::suggested_sample_multiplier(
	const InputDataType input_type,
	const float per_line_subcarrier_frequency,
	const int samples_per_line,
	const int buffer_width
) {
	// If phase-linked luminance output is in effect, pick a 'high' integral multiple of the
	// subcarrier, ignoring the samples per line. This will allow the shaders to do point sampling
	// with impunity.
	if(input_type == InputDataType::PhaseLinkedLuminance8) {
		const float sample_multiplier = per_line_subcarrier_frequency * 8.0f <= buffer_width ? 8.0f : 4.0f;
		return sample_multiplier * per_line_subcarrier_frequency / float(samples_per_line);
	}

	// Determine the minimum output width that will capture sufficient colour subcarrier information.
	const float minimum = MinColourSubcarrierMultiplier * per_line_subcarrier_frequency;

	// Prefer the minimum integer multiple that is at or above that minimum width.
	const float ideal = std::ceil(minimum / float(samples_per_line));
	if(ideal * float(samples_per_line) <= buffer_width) {
		return ideal;
	}

	// Failing that, pick a smaller integer if one is available; otherwise saturate the available pixels.
	// Integer multiples are preferable for not unduly aliasing the incoming pixel data (even if point-sampled).
	return ideal > 1.0f ? ideal - 1.0f : float(buffer_width) / float(samples_per_line);
}
