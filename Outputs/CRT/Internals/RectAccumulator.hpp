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
		candidates_.push_back(rect);
		if(candidates_.pushes() == CandidateHistorySize) {
			return candidates_.join();
		}
		return std::nullopt;
	}

	std::optional<Display::Rect> first_reading() const {
		if(candidates_.pushes() != 2) return std::nullopt;
		return candidates_.join(2);
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

		Display::Rect join(int limit = 0) const {
			return std::accumulate(
				stream_.begin() + 1,
				limit > 0 ? stream_.begin() + limit : stream_.end(),
				stream_[0],
				[](const Display::Rect &lhs, const Display::Rect &rhs) {
					return lhs | rhs;
				}
			);
		}

		bool stable() const {
			return std::all_of(
				stream_.begin() + 1,
				stream_.end(),
				[&](const Display::Rect &rhs) {
					return rhs == stream_[0];
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

	// Require at least a second in any given state.
	static constexpr int CandidateHistorySize = 250;
	RectHistory<CandidateHistorySize> candidates_;
};

}
