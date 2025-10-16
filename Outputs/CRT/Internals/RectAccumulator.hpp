//
//  RectAccumulator.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/10/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include "Outputs/ScanTarget.hpp"

#include <algorithm>
#include <array>
#include <numeric>
#include <optional>

namespace Outputs::CRT {

struct RectAccumulator {
	std::optional<Display::Rect> posit(const Display::Rect &rect) {
		if(!did_first_read_) {
			first_reading_candidates_.push_back(rect);
		}

		candidates_.push_back(rect);
		if(candidates_.pushes() == CandidateHistorySize) {
			return candidates_.join();
		}
		return std::nullopt;
	}

	std::optional<Display::Rect> first_reading(const float threshold) {
		if(did_first_read_ || !first_reading_candidates_.stable(threshold)) {
			return std::nullopt;
		}
		did_first_read_ = true;
		return first_reading_candidates_.join();
	}

private:
	template <size_t n>
	struct RectHistory {
		void push_back(const Display::Rect &rect) {
			stream_[stream_pointer_] = rect;
			pushes_ = std::min(pushes_ + 1, int(n));
			++stream_pointer_;
			if(stream_pointer_ == n) stream_pointer_ = 0;
		}

		Display::Rect join() const {
			return std::accumulate(
				stream_.begin() + 1,
				stream_.end(),
				*stream_.begin(),
				[](const Display::Rect &lhs, const Display::Rect &rhs) {
					return lhs | rhs;
				}
			);
		}

		bool stable(const float threshold) const {
			return std::all_of(
				stream_.begin() + 1,
				stream_.end(),
				[&](const Display::Rect &rhs) {
					return rhs.equal(stream_[0], threshold);
				}
			);
		}

		const Display::Rect &any() const {
			return stream_[0];
		}

		const int pushes() const {
			return pushes_;
		}

	private:
		std::array<Display::Rect, n> stream_;
		size_t stream_pointer_ = 0;
		int pushes_ = 0;
	};

	// Use the union of "a prolonged period" to figure out what should currently be visible.
	static constexpr int CandidateHistorySize = 500;
	RectHistory<CandidateHistorySize> candidates_;

	// At startup, look for a small number of sequential but consistent frames.
	static constexpr int FirstReadingSize = 4;
	RectHistory<FirstReadingSize> first_reading_candidates_;
	bool did_first_read_ = false;
};

}
