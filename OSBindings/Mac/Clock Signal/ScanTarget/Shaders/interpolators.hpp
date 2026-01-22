//
//  interpolators.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/01/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include <metal_stdlib>

// MARK: - Interpolated values.

struct SourceInterpolator {
	metal::float4 position [[position]];
	metal::float2 textureCoordinates;
	float unitColourPhase;		// One unit per circle.
	float colourPhase;			// Radians.
	half colourAmplitude [[flat]];
};

struct CopyInterpolator {
	metal::float4 position [[position]];
	metal::float2 textureCoordinates;
};
