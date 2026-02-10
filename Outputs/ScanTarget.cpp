//
//  ScanTarget.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/11/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "ScanTarget.hpp"

using namespace Outputs::Display;

NullScanTarget NullScanTarget::singleton;

std::array<float, 9> Outputs::Display::aspect_ratio_transformation(
	const ScanTarget::Modals &modals,
	const float view_aspect_ratio
) {
	using Matrix3x3 = std::array<float, 9>;
	constexpr Matrix3x3 identity = {
		1.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 1.0f
	};
	constexpr auto multiply = [](const Matrix3x3 &lhs, const Matrix3x3 &rhs) {
		return Matrix3x3{
			lhs[0]*rhs[0] + lhs[3]*rhs[1] + lhs[6]*rhs[2],
			lhs[1]*rhs[0] + lhs[4]*rhs[1] + lhs[7]*rhs[2],
			lhs[2]*rhs[0] + lhs[5]*rhs[1] + lhs[8]*rhs[2],

			lhs[0]*rhs[3] + lhs[3]*rhs[4] + lhs[6]*rhs[5],
			lhs[1]*rhs[3] + lhs[4]*rhs[4] + lhs[7]*rhs[5],
			lhs[2]*rhs[3] + lhs[5]*rhs[4] + lhs[8]*rhs[5],

			lhs[0]*rhs[6] + lhs[3]*rhs[7] + lhs[6]*rhs[8],
			lhs[1]*rhs[6] + lhs[4]*rhs[7] + lhs[7]*rhs[8],
			lhs[2]*rhs[6] + lhs[5]*rhs[7] + lhs[8]*rhs[8],
		};
	};

	Matrix3x3 source_to_display = identity;
	// The starting coordinate space is [0, 1].

	// Move the centre of the cropping rectangle to the centre of the display.
	{
		Matrix3x3 recentre = identity;
		recentre[6] = 0.5f - (modals.visible_area.origin.x + modals.visible_area.size.width * 0.5f);
		recentre[7] = 0.5f - (modals.visible_area.origin.y + modals.visible_area.size.height * 0.5f);
		source_to_display = multiply(recentre, source_to_display);
	}

	// Convert from the internal [0, 1] to centred [-1, 1].
	{
		Matrix3x3 to_eye = identity;
		to_eye[0] = 2.0f;
		to_eye[4] = -2.0f;
		to_eye[6] = -1.0f;
		to_eye[7] = 1.0f;
		source_to_display = multiply(to_eye, source_to_display);
	}

	// Determine correct zoom, combining (i) the necessary horizontal stretch for aspect ratio; and
	// (ii) the necessary zoom to fit either the visible area width or height.
	const float aspect_ratio_stretch = float(modals.aspect_ratio / view_aspect_ratio);
	const float zoom = modals.visible_area.appropriate_zoom(aspect_ratio_stretch);

	// Convert from there to the proper aspect ratio by stretching or compressing width.
	// After this the output is exactly centred, filling the vertical space and being as wide or slender as it likes.
	{
		Matrix3x3 apply_aspect_ratio = identity;
		apply_aspect_ratio[0] = aspect_ratio_stretch * zoom;
		apply_aspect_ratio[4] = zoom;
		source_to_display = multiply(apply_aspect_ratio, source_to_display);
	}

	return source_to_display;
}
