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
	const size_t max_kernel_size,
	DecodingPath decoding_path
) :
	samples_per_line_(samples_per_line),
	subcarrier_frequency_(subcarrier_frequency),
	max_kernel_size_(max_kernel_size),
	decoding_path_(decoding_path) {}

float FilterGenerator::radians_per_sample() const {
	return (subcarrier_frequency_ * 3.141592654f * 2.0f) / samples_per_line_;
}

FilterGenerator::FilterPair FilterGenerator::separation_filter() {
	FilterPair result{};

	// Luminance: box.
	result.luma =
		SignalProcessing::Box::filter<SignalProcessing::ScalarType::Float>(
			radians_per_sample(),
			3.141592654f * 2.0f
		);

	// Chrominance: compute as centre sample - luminance.
	// TODO: would be better to apply a separate notch, but I think I've
	// confused the scales and offsets.
	result.chroma = result.luma * -1.0f;
	result.chroma.resize(max_kernel_size_);
	SignalProcessing::KaiserBessel::filter<SignalProcessing::ScalarType::Float>(
		max_kernel_size_,
		samples_per_line_,
		subcarrier_frequency_ * 0.9f,
		samples_per_line_
	).copy_to<SignalProcessing::FIRFilter<SignalProcessing::ScalarType::Float>::iterator>(
		result.chroma.begin(),
		result.chroma.end(),
		[](const auto iterator, const float value) {
			*iterator += value;
		}
	);

	return result;
}

FilterGenerator::FilterPair FilterGenerator::demouldation_filter() {
	FilterPair result{};

	result.chroma =
		SignalProcessing::KaiserBessel::filter<SignalProcessing::ScalarType::Float>(
			max_kernel_size_,
			samples_per_line_,
			40.0f,
			subcarrier_frequency_ * 0.5f
		)
		* (decoding_path_ == DecodingPath::SVideo ? 2.0f : 0.5f);

	// S-Video: don't filter luminance at all.
	if(decoding_path_ == DecodingPath::SVideo) {
		const float identity[] = { 1.0f };
		result.luma =
			SignalProcessing::FIRFilter<SignalProcessing::ScalarType::Float>(
				std::begin(identity),
				std::end(identity)
			);
		return result;
	}

	// Composite: sharpen the luminance a touch.
	result.luma =
		SignalProcessing::KaiserBessel::filter<SignalProcessing::ScalarType::Float>(
			max_kernel_size_, samples_per_line_, 10.0f, subcarrier_frequency_);

	return result;
}
