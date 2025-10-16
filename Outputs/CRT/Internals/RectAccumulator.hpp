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
	std::optional<Display::Rect> posit(const Display::Rect &rect, const float stability_threshold) {
		stable_filter_.push_back(rect);

		if(stable_filter_.full() && stable_filter_.stable(stability_threshold)) {
			candidates_.push_back(stable_filter_.join());
			stable_filter_.reset();

			if(candidates_.full()) {
				return candidates_.join();
			}
		}
		return std::nullopt;
	}

	std::optional<Display::Rect> first_reading(const float stability_threshold) {
		if(
			did_first_read_ ||
			!stable_filter_.full() ||
			!stable_filter_.stable(stability_threshold)
		) {
			return std::nullopt;
		}
		did_first_read_ = true;
		return stable_filter_.join();
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
			if(!full()) {
				return false;
			}

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

		bool full() const {
			return pushes_ == int(n);
		}

		void reset() {
			pushes_ = 0;
		}

	private:
		std::array<Display::Rect, n> stream_;
		size_t stream_pointer_ = 0;
		int pushes_ = 0;
	};

	// Use the union of "a prolonged period" to figure out what should currently be visible.
	static constexpr int CandidateHistorySize = 120;
	RectHistory<CandidateHistorySize> candidates_;

	// At startup, look for a small number of sequential but consistent frames.
	static constexpr int StableFilterSize = 4;
	RectHistory<StableFilterSize> stable_filter_;
	bool did_first_read_ = false;
};

}
