//
//  FilterGenerator.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/01/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include "SignalProcessing/FIRFilter.hpp"

#include <algorithm>

namespace Outputs::Display {

class FilterGenerator {
private:
	static constexpr float MinColourSubcarrierMultiplier = 8.0f;

public:
	/// The largest size of filter this generator might produce.
	static constexpr size_t MaxKernelSize = 31;

	/// A suggested size, in pixels, for a buffer large enough to contain at least an entire line of composite or s-video samples, in PAL or NTSC,
	/// at a suitable precision for the filters this generator will produce to work acceptably.
	static constexpr int SuggestedBufferWidth = (MinColourSubcarrierMultiplier > 5.0f) ? 3072 : 1536;

	/// Provides a suggested multiplier to map from input locations measured in terms of `samples_per_line` to locations within a buffer that is
	/// at most `buffer_width` in size in order to capture sufficient detail to do a good job of decoding composite video with a subcarrier
	/// at `per_line_subcarrier_frequency`.
	static float suggested_sample_multiplier(
		float per_line_subcarrier_frequency,
		int samples_per_line,
		int buffer_width = SuggestedBufferWidth
	);

	enum class DecodingPath {
		Composite,
		SVideo
	};

	FilterGenerator(
		float samples_per_line,
		float subcarrier_frequency,
		DecodingPath
	);

	struct FilterPair {
		SignalProcessing::FIRFilter<SignalProcessing::ScalarType::Float> luma;
		SignalProcessing::FIRFilter<SignalProcessing::ScalarType::Float> chroma;

		size_t size() const {
			return std::max(luma.size(), chroma.size());
		}
	};

	/// A pair of filters to separate luminance and chrominance from an input of composite scalars.
	/// Chrominance returned remains QAM encoded.
	FilterPair separation_filter() const;

	/// Pairs a post-demodulation filter to apply to the chrominance channels after the trigonmetric part of
	/// QAM demodulation, to either a passthrough or a sharpen on luminance.
	FilterPair demouldation_filter() const;

private:
	float samples_per_line_;
	float subcarrier_frequency_;
	DecodingPath decoding_path_;

	float radians_per_sample() const;
};

}
