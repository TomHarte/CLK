//
//  CopiesAndFills.metal
//  Clock Signal
//
//  Created by Thomas Harte on 06/01/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "interpolators.hpp"
#include "uniforms.hpp"

#include <metal_stdlib>

namespace {
constexpr metal::sampler nearestSampler(
	metal::coord::pixel,
	metal::address::clamp_to_edge,
	metal::filter::nearest
);

constexpr metal::sampler linearSampler(
	metal::coord::pixel,
	metal::address::clamp_to_edge,
	metal::filter::linear
);
}

/// Point-samples @c texture to copy directly from source to target.
fragment half4 copyFragment(
	const CopyInterpolator vert [[stage_in]],
	const metal::texture2d<half> texture [[texture(0)]]
) {
	return texture.sample(nearestSampler, vert.textureCoordinates);
}

/// Bilinearly samples @c texture.
fragment half4 interpolateFragment(
	const CopyInterpolator vert [[stage_in]],
	const metal::texture2d<half> texture [[texture(0)]]
) {
	return texture.sample(linearSampler, vert.textureCoordinates);
}

/// Fills with black.
fragment half4 clearFragment(
	const constant Uniforms &uniforms [[buffer(0)]]
) {
	return half4(0.0, 0.0, 0.0, uniforms.outputAlpha);
}
