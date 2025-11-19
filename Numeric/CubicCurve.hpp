//
//  CubicCurve.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/10/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include <cassert>

namespace Numeric {

/*!
	Provides a cubic Bezier-based timing function.
*/
struct CubicCurve {
	CubicCurve(const float c1x, const float c1y, const float c2x, const float c2y) :
		c1_(c1x, c1y), c2_(c2x, c2y)
	{
		assert(0.0f <= c1x);	assert(c1x <= 1.0f);
		assert(0.0f <= c1y);	assert(c1y <= 1.0f);
		assert(0.0f <= c2x);	assert(c2x <= 1.0f);
		assert(0.0f <= c2y);	assert(c2y <= 1.0f);
	}

	/// @returns A standard ease-in-out animation curve.
	static CubicCurve easeInOut() {
		return CubicCurve(0.42f, 0.0f, 0.58f, 1.0f);
	}

	/// @returns The value for y given x, in range [0.0, 1.0].
	float value(const float x) const {
		return axis(t(x), 1);
	}

private:
	/// @returns The value for @c t that generates the value @c x.
	float t(const float x) const {
		static constexpr float Precision = 0.01f;
		float bounds[2] = {0.0f, 1.0f};
		const auto midpoint = [&] { return (bounds[0] + bounds[1]) * 0.5f; };

		while(bounds[1] > bounds[0] + Precision) {
			const float mid = midpoint();
			const float value = axis(mid, 0);
			if(value > x) {
				bounds[1] = mid;
			} else {
				bounds[0] = mid;
			}
		}
		return midpoint();
	}

	/// @returns The value for axis @c index at time @c t.
	float axis(const float t, int index) const {
		const float f1 = t * c1_[index];
		const float f2 = t * c2_[index] + (1.0f - t) * c1_[index];
		const float f3 = t + (1.0f - t) * c2_[index];

		const float b1 = t * f2 + (1.0f - t) * f1;
		const float b2 = t * f3 + (1.0f - t) * f2;

		return t * b2 + (1.0f - t) * b1;
	}

	float c1_[2];
	float c2_[2];
};

}
