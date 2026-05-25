//
//  SegmentCounter.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/03/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include <algorithm>
#include <cassert>

namespace Numeric {

/*!
	A counter that divides its time into n segments, each of a fixed length, then repeats.

	Lambdas receive:
		(1)	calls to advance through segment and regions within them; and
		(2)	optionally, an announcement each time the counter wraps around.
*/
template <int SegmentLength, int Segments>
struct DividingAccumulator {
	DividingAccumulator(const int initial = 0) : position_(initial % (SegmentLength * Segments)) {}

	template <typename AdvanceFuncT, typename ResetFuncT>
	void advance(int count, const AdvanceFuncT &&advance, const ResetFuncT &&reset = []{}) {
		while(count > 0) {
			const int segment = position_ / SegmentLength;
			const int begin = position_ % SegmentLength;
			const int length = std::min(SegmentLength - begin, count);

			assert(length > 0);
			advance(segment, begin, begin + length);

			count -= length;
			position_ += length;
			if(position_ == SegmentLength * Segments) {
				position_ = 0;
				reset();
			}
		}
	}

	/// @returns Current segment.
	int segment() const {
		return position_ / SegmentLength;
	}

	/// @returns Offset within the current segment.
	int subsegment() const {
		return position_ % SegmentLength;
	}

	/// @returns Number of positions left in the current segment.
	int subsegment_remaining() const {
		return SegmentLength - (position_ % SegmentLength);
	}

	/// @returns Number of positions left in the entire count.
	int total_remaining() const {
		return SegmentLength * Segments - position_;
	}

	/// @returns Position within the range [0, SegmentLength*Segments)
	int absolute() const {
		return position_;
	}

private:
	int position_ = 0;
};

/// Calculates the overlap, if any, between [begin, end] and [RangeBegin, RangeEnd] and calls FuncT with a begin and end if the range is non-zero.
template <int RangeBegin, int RangeEnd, typename FuncT>
void clamp(const int begin, const int end, const FuncT &&function) {
	const int range_begin = std::max(begin, RangeBegin);
	const int range_end = std::min(end, RangeEnd);
	if(range_end > range_begin) {
		function(range_begin, range_end);
	}
}

/// Calls @c function if and only if [begin, end] contains the event point.
template <int EventPoint, typename FuncT>
void if_includes(const int begin, const int end, const FuncT &&function) {
	if(begin <= EventPoint && end > EventPoint) {
		function();
	}
}

// Provides a continuation-esque means of describing events with a fixed sequencing and executing
// a subset of them.
template <int begin = 0>
class ActionRange {
public:
	ActionRange(const int span_begin, const int span_end) : begin_(span_begin), end_(span_end) {}

	template <typename FuncT>
	auto then(const FuncT &&function) {
		if(begin_ <= begin && end_ > begin) {
			function();
		}
		return *this;
	}

	template <typename FuncT>
	auto finally(const FuncT &&function) {
		if(begin == end_) {
			function();
		}
		return *this;
	}

	template <int end, typename FuncT>
	auto until(const FuncT &&function) {
		const int range_begin = std::max(begin_, begin);
		const int range_end = std::min(end_, end);
		if(range_end > range_begin) {
			function(range_begin, range_end);
		}
		return ActionRange<end>(begin_, end_);
	}

private:
	const int begin_, end_;
};

}
