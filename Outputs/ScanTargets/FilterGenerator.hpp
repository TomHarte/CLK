//
//  FilterGenerator.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/01/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include "SignalProcessing/FIRFilter.hpp"

namespace Outputs::Display {

class FilterGenerator {
public:
	enum class DecodingPath {
		Composite,
		SVideo
	};

	FilterGenerator(
		float samples_per_line,
		float subcarrier_frequency,
		int max_kernel_size,
		DecodingPath
	);

	struct FilterPair {
		SignalProcessing::FIRFilter<SignalProcessing::ScalarType::Float> luma;
		SignalProcessing::FIRFilter<SignalProcessing::ScalarType::Float> chroma;
	};

	/// A pair of filters to separate luminance and chrominance from an input of composite scalars.
	/// Chrominance returned remains QAM encoded.
	FilterPair separation_filter();

	/// Pairs a post-demodulation filter to apply to the chrominance channels after the trigonmetric part of
	/// QAM demodulation, to either a passthrough or a sharpen on luminance.
	FilterPair demouldation_filter();

private:
	float samples_per_line_;
	float subcarrier_frequency_;
	int max_kernel_size_;
	DecodingPath decoding_path_;
};

}
