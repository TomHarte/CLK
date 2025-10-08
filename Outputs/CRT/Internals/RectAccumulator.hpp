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
		unions_.push_back(rect);

		candidates_.push_back(unions_.join());
		if(candidates_.pushes() == CandidateHistorySize && candidates_.stable()) {
			return candidates_.any();
		}
		return std::nullopt;
	}

	std::optional<Display::Rect> any_union() const {
		if(unions_.pushes() == UnionHistorySize) {
			return unions_.join();
		}
		return std::nullopt;
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

	// A long record, to try to avoid instability caused by interlaced video, flashing cursors, etc.
	static constexpr int UnionHistorySize = 17;
	RectHistory<UnionHistorySize> unions_;

	// Require at least a second in any given state.
	static constexpr int CandidateHistorySize = 60;
	RectHistory<CandidateHistorySize> candidates_;
};

}
