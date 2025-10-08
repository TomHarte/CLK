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
		if(candidates_.stable()) {
			return candidates_.any();
		}
		return std::nullopt;
	}

private:
	template <size_t n>
	struct RectHistory {
		void push_back(const Display::Rect &rect) {
			stream_[stream_pointer_] = rect;
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

	private:
		std::array<Display::Rect, n> stream_;
		size_t stream_pointer_ = 0;
	};

	RectHistory<32> unions_;	// A long record, to try to avoid instability caused by interlaced video, flashing
								// cursors, etc.
	RectHistory<28> candidates_;
};

}
