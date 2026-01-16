//
//  LinearFilter.h
//  Clock Signal
//
//  Created by Thomas Harte on 01/10/2011.
//  Copyright 2011 Thomas Harte. All rights reserved.
//

#pragma once

// Use the Accelerate framework to vectorise, unless this is a Qt build.
// Primarily that avoids gymnastics in the QMake file; it also eliminates
// a difference in the Qt build across platforms.
#if defined(__APPLE__) && !defined(TARGET_QT)
#include <Accelerate/Accelerate.h>
#define USE_ACCELERATE
#endif

#include <algorithm>
#include <cassert>
#include <cmath>
#include <functional>
#include <iterator>
#include <numeric>
#include <vector>

namespace SignalProcessing {
constexpr float FixedMultiplier = 32767.0f;
constexpr int FixedShift = 15;

enum class ScalarType {
	Int16,
	Float,
};

template <ScalarType type>
class FIRFilter {
public:
	using CoefficientType = std::conditional_t<
		type == ScalarType::Int16,
		int16_t,
		float
	>;

	FIRFilter() = default;

	template <typename IteratorT>
	FIRFilter(const IteratorT begin, const IteratorT end) {
		// Copy into place, possibly converting to fixed point.
		if constexpr (type == ScalarType::Float) {
			std::copy(begin, end, std::back_inserter(coefficients_));
		} else {
			IteratorT it = begin;
			while(it != end) {
				coefficients_.push_back(int16_t(*it * FixedMultiplier));
				++it;
			}
		}

		// Trim.
		static constexpr CoefficientType threshold = type == ScalarType::Int16 ? 2 : 0.0001f;
		while(!coefficients_.empty()) {
			if(
				std::abs(coefficients_.front()) > threshold ||
				std::abs(coefficients_.back()) > threshold
			) break;
			coefficients_.erase(coefficients_.begin());
			if(!coefficients_.empty()) coefficients_.erase(coefficients_.end() - 1);
		}
	}

	/*!
		Applies the filter to one batch of input samples, returning the net result.

		@param src The source buffer to apply the filter to.
		@returns The result of applying the filter.
	*/
	CoefficientType apply(const CoefficientType *const src, const size_t stride = 1) const {
		#ifdef USE_ACCELERATE
		if constexpr (type == ScalarType::Int16) {
			int16_t result;
			vDSP_dotpr_s1_15(
				coefficients_.data(),
				1,
				src,
				vDSP_Stride(stride),
				&result,
				coefficients_.size()
			);
			return result;
		} else {
			float result;
			vDSP_dotpr(
				coefficients_.data(),
				1,
				src,
				vDSP_Stride(stride),
				&result,
				coefficients_.size()
			);
			return result;
		}
		#endif

		using AccumulatorType = std::conditional_t<
			type == ScalarType::Int16,
			int,
			float
		>;
		AccumulatorType result = 0;
		for(size_t c = 0; c < coefficients_.size(); ++c) {
			result += coefficients_[c] * src[c * stride];
		}

		if constexpr (type == ScalarType::Int16) {
			return CoefficientType(result >> FixedShift);
		} else {
			return result;
		}
	}

	CoefficientType operator[](const size_t index) const {
		return coefficients_[index];
	}

	size_t size() const {
		return coefficients_.size();
	}

	template <typename IteratorT>
	void copy_to(
		IteratorT begin,
		IteratorT end,
		const std::function<void(IteratorT, CoefficientType)> &applier
	) const {
		auto dest = begin;
		auto src = coefficients_.begin();

		const auto destination_size = size_t(std::distance(begin, end));
		if(destination_size <= coefficients_.size()) {
			std::advance(src, (coefficients_.size() - destination_size) / 2);
		} else {
			std::advance(dest, (destination_size - coefficients_.size()) / 2);
		}

		auto steps = std::min(destination_size, coefficients_.size());
		while(steps--) {
			applier(dest, *src);
			++dest;
			++src;
		}
	}

	template <typename IteratorT>
	void copy_to(
		const IteratorT begin,
		const IteratorT end
	) const {
		copy_to(begin, end, [](const IteratorT target, const CoefficientType coefficient) {
			*target = coefficient;
		});
	}

	FIRFilter &operator *(const CoefficientType rhs) {
		for(auto &coefficient: coefficients_) {
			if constexpr (type == ScalarType::Float) {
				coefficient *= rhs;
			} else {
				coefficient = (coefficient * rhs) >> FixedShift;
			}
		}
		return *this;
	}

private:
	std::vector<CoefficientType> coefficients_;
};

/*!
	The FIR filter takes a 1d PCM signal with a given sample rate and applies a band-pass filter to it.

	The number of taps (ie, samples considered simultaneously to make an output sample) is configurable;
	smaller numbers permit a filter that operates more quickly and with less lag but less effectively.
*/


namespace KaiserBessel {
static constexpr float DefaultAttenuation = 60.0f;

/*!
	@param number_of_taps The size of window for input data.
	@param input_sample_rate The sampling rate of the input signal.
	@param low_frequency The lowest frequency of signal to retain in the output.
	@param high_frequency The highest frequency of signal to retain in the output.
	@param attenuation The attenuation of the discarded frequencies.
*/
template <ScalarType type>
FIRFilter<type> filter(
	size_t number_of_taps,
	float input_sample_rate,
	float low_frequency,
	float high_frequency,
	float attenuation = DefaultAttenuation
);
}

namespace Box {
template <ScalarType type>
FIRFilter<type> filter(
	float units_per_sample,
	float total_range
);
}

}
