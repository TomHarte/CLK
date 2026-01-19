//
//  uniforms.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/12/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include <metal_stdlib>

struct Uniforms {
	// This is used to scale scan positions, i.e. it provides the range
	// for mapping from scan-style integer positions into eye space.
	int2 scale;

	// Applies a multiplication to all cyclesSinceRetrace values.
	float cycleMultiplier;

	// This provides the intended height of a scan, in eye-coordinate terms.
	float lineWidth;

	// Provides zoom and offset to scale the source data.
	metal::float3x3 sourceToDisplay;

	// Provides conversions to and from RGB for the active colour space.
	metal::half3x3 toRGB;
	metal::half3x3 fromRGB;

	// Describes the filter in use for chroma filtering; it'll be
	// 15 coefficients but they're symmetrical around the centre.
	half3 chromaKernel[16];

	// Describes the filter in use for luma filtering; 15 coefficients
	// symmetrical around the centre.
	half2 lumaKernel[16];

	// Sets the opacity at which output strips are drawn.
	half outputAlpha;

	// Sets the gamma power to which output colours are raised.
	half outputGamma;

	// Sets a brightness multiplier for output colours.
	half outputMultiplier;
};

