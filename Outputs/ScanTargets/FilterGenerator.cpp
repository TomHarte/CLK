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
	return FilterGenerator::FilterPair{};
}

FilterGenerator::FilterPair FilterGenerator::demouldation_filter() {
	return FilterGenerator::FilterPair{};
}
