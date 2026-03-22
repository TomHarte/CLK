//
//  SegmentCounter.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/03/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

namespace Numeric {

/*!
	A counter that divides its time into n segments, each of a fixed length, then repeats.

	Lambdas receive:
		(1)	calls to advance through segment and regions within them; and
		(2)	optionally, an announcement each time the counter wraps around.
*/
template <int SegmentLength, int Segments>
struct DividingAccumulator {
	template <typename AdvanceFuncT, typename ResetFuncT>
	void advance(int count, const AdvanceFuncT &&advance, const ResetFuncT &&reset = []{}) {
		while(count > 0) {
			const int segment = position_ / SegmentLength;
			const int begin = position_ % SegmentLength;

			const int length = std::min(SegmentLength - begin, count);
			const int end = std::min(SegmentLength, begin + length);
			count -= length;

			advance(segment, begin, end);

			position_ += length;
			if(position_ == SegmentLength * Segments) {
				position_ = 0;
				reset();
			}
		}
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

}
