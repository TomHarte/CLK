//
//  ComputeKernels.metal
//  Clock Signal
//
//  Created by Thomas Harte on 22/12/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#include <metal_stdlib>

#include "uniforms.hpp"

// MARK: - Filters for the chrominance portion of an UnfilteredYUVAmplitude texture, to remove high-frequency noise.

/// Given input pixels of the form (luminance, 0.5 + 0.5*chrominance*cos(phase), 0.5 + 0.5*chrominance*sin(phase)), applies a lowpass
/// filter to the two chrominance parts, then uses the toRGB matrix to convert to RGB and stores.
template <bool applyGamma> void filterChromaKernel(
	const metal::texture2d<half, metal::access::read> inTexture [[texture(0)]],
	const metal::texture2d<half, metal::access::write> outTexture [[texture(1)]],
	const uint2 gid [[thread_position_in_grid]],
	const constant Uniforms &uniforms [[buffer(0)]],
	const constant int &offset [[buffer(1)]]
) {
	constexpr half4 moveToZero(0.0f, 0.5f, 0.5f, 0.0f);
	const half4 rawSamples[] = {
		inTexture.read(gid + uint2(0, offset)) - moveToZero,
		inTexture.read(gid + uint2(1, offset)) - moveToZero,
		inTexture.read(gid + uint2(2, offset)) - moveToZero,
		inTexture.read(gid + uint2(3, offset)) - moveToZero,
		inTexture.read(gid + uint2(4, offset)) - moveToZero,
		inTexture.read(gid + uint2(5, offset)) - moveToZero,
		inTexture.read(gid + uint2(6, offset)) - moveToZero,
		inTexture.read(gid + uint2(7, offset)) - moveToZero,
		inTexture.read(gid + uint2(8, offset)) - moveToZero,
		inTexture.read(gid + uint2(9, offset)) - moveToZero,
		inTexture.read(gid + uint2(10, offset)) - moveToZero,
		inTexture.read(gid + uint2(11, offset)) - moveToZero,
		inTexture.read(gid + uint2(12, offset)) - moveToZero,
		inTexture.read(gid + uint2(13, offset)) - moveToZero,
		inTexture.read(gid + uint2(14, offset)) - moveToZero,
	};

#define Sample(x, y) uniforms.chromaKernel[y] * rawSamples[x].rgb
	const half3 colour =
		Sample(0, 0) + Sample(1, 1) + Sample(2, 2) + Sample(3, 3) + Sample(4, 4) + Sample(5, 5) + Sample(6, 6) +
		Sample(7, 7) +
		Sample(8, 6) + Sample(9, 5) + Sample(10, 4) + Sample(11, 3) + Sample(12, 2) + Sample(13, 1) + Sample(14, 0);
#undef Sample

	const half4 output = half4(uniforms.toRGB * colour * uniforms.outputMultiplier, uniforms.outputAlpha);
	if(applyGamma) {
		outTexture.write(metal::pow(output, uniforms.outputGamma), gid + uint2(7, offset));
	} else {
		outTexture.write(output, gid + uint2(7, offset));
	}
}

kernel void filterChromaKernelNoGamma(
	const metal::texture2d<half, metal::access::read> inTexture [[texture(0)]],
	const metal::texture2d<half, metal::access::write> outTexture [[texture(1)]],
	const uint2 gid [[thread_position_in_grid]],
	const constant Uniforms &uniforms [[buffer(0)]],
	const constant int &offset [[buffer(1)]]
) {
	filterChromaKernel<false>(inTexture, outTexture, gid, uniforms, offset);
}

kernel void filterChromaKernelWithGamma(
	const metal::texture2d<half, metal::access::read> inTexture [[texture(0)]],
	const metal::texture2d<half, metal::access::write> outTexture [[texture(1)]],
	const uint2 gid [[thread_position_in_grid]],
	const constant Uniforms &uniforms [[buffer(0)]],
	const constant int &offset [[buffer(1)]]
) {
	filterChromaKernel<true>(inTexture, outTexture, gid, uniforms, offset);
}

// MARK: - Luma/chroma separation filters of various sizes.

/// Stores a separated sample, potentially discarding the chrominance section if there was no colour burst.
void setSeparatedLumaChroma(
	const half luminance,
	const half4 centreSample,
	const metal::texture2d<half, metal::access::write> outTexture,
	const uint2 gid,
	const int offset
) {
	// The mix/steps below ensures that the absence of a colour burst leads the colour subcarrier to be discarded.
	const half isColour = metal::step(half(0.01f), centreSample.a);
	const half chroma = (centreSample.r - luminance) / metal::mix(half(1.0f), centreSample.a, isColour);
	outTexture.write(half4(
			luminance / metal::mix(half(1.0f), (half(1.0f) - centreSample.a), isColour),
			isColour * (centreSample.gb - half2(0.5f)) * chroma + half2(0.5f),
			1.0f
		),
		gid + uint2(7, offset)
	);
}

/// Given input pixels of the form:
///
///	(composite sample, cos(phase), sin(phase), colour amplitude), applies a lowpass
///
/// Filters to separate luminance, subtracts that and scales and maps the remaining chrominance in order to output
/// pixels in the form:
///
///	(luminance, 0.5 + 0.5*chrominance*cos(phase), 0.5 + 0.5*chrominance*sin(phase))
///
/// i.e. the input form for the filterChromaKernel, above].
kernel void separateLumaKernel15(
	const metal::texture2d<half, metal::access::read> inTexture [[texture(0)]],
	const metal::texture2d<half, metal::access::write> outTexture [[texture(1)]],
	const uint2 gid [[thread_position_in_grid]],
	const constant Uniforms &uniforms [[buffer(0)]],
	const constant int &offset [[buffer(1)]]
) {
	const half4 centreSample = inTexture.read(gid + uint2(7, offset));
	const half rawSamples[] = {
		inTexture.read(gid + uint2(0, offset)).r,	inTexture.read(gid + uint2(1, offset)).r,
		inTexture.read(gid + uint2(2, offset)).r,	inTexture.read(gid + uint2(3, offset)).r,
		inTexture.read(gid + uint2(4, offset)).r,	inTexture.read(gid + uint2(5, offset)).r,
		inTexture.read(gid + uint2(6, offset)).r,
		centreSample.r,
		inTexture.read(gid + uint2(8, offset)).r,
		inTexture.read(gid + uint2(9, offset)).r,	inTexture.read(gid + uint2(10, offset)).r,
		inTexture.read(gid + uint2(11, offset)).r,	inTexture.read(gid + uint2(12, offset)).r,
		inTexture.read(gid + uint2(13, offset)).r,	inTexture.read(gid + uint2(14, offset)).r,
	};

#define Sample(x, y) uniforms.lumaKernel[y] * rawSamples[x]
	const half luminance =
		Sample(0, 0) + Sample(1, 1) + Sample(2, 2) + Sample(3, 3) + Sample(4, 4) + Sample(5, 5) + Sample(6, 6) +
		Sample(7, 7) +
		Sample(8, 6) + Sample(9, 5) + Sample(10, 4) + Sample(11, 3) + Sample(12, 2) + Sample(13, 1) + Sample(14, 0);
#undef Sample

	return setSeparatedLumaChroma(luminance, centreSample, outTexture, gid, offset);
}

kernel void separateLumaKernel9(
	const metal::texture2d<half, metal::access::read> inTexture [[texture(0)]],
	const metal::texture2d<half, metal::access::write> outTexture [[texture(1)]],
	const uint2 gid [[thread_position_in_grid]],
	const constant Uniforms &uniforms [[buffer(0)]],
	const constant int &offset [[buffer(1)]]
) {
	const half4 centreSample = inTexture.read(gid + uint2(7, offset));
	const half rawSamples[] = {
		inTexture.read(gid + uint2(3, offset)).r,	inTexture.read(gid + uint2(4, offset)).r,
		inTexture.read(gid + uint2(5, offset)).r,	inTexture.read(gid + uint2(6, offset)).r,
		centreSample.r,
		inTexture.read(gid + uint2(8, offset)).r,	inTexture.read(gid + uint2(9, offset)).r,
		inTexture.read(gid + uint2(10, offset)).r,	inTexture.read(gid + uint2(11, offset)).r
	};

#define Sample(x, y) uniforms.lumaKernel[y] * rawSamples[x]
	const half luminance =
		Sample(0, 3) + Sample(1, 4) + Sample(2, 5) + Sample(3, 6) +
		Sample(4, 7) +
		Sample(5, 6) + Sample(6, 5) + Sample(7, 4) + Sample(8, 3);
#undef Sample

	return setSeparatedLumaChroma(luminance, centreSample, outTexture, gid, offset);
}

kernel void separateLumaKernel7(
	const metal::texture2d<half, metal::access::read> inTexture [[texture(0)]],
	const metal::texture2d<half, metal::access::write> outTexture [[texture(1)]],
	const uint2 gid [[thread_position_in_grid]],
	const constant Uniforms &uniforms [[buffer(0)]],
	const constant int &offset [[buffer(1)]]
) {
	const half4 centreSample = inTexture.read(gid + uint2(7, offset));
	const half rawSamples[] = {
		inTexture.read(gid + uint2(4, offset)).r,
		inTexture.read(gid + uint2(5, offset)).r,	inTexture.read(gid + uint2(6, offset)).r,
		centreSample.r,
		inTexture.read(gid + uint2(8, offset)).r,	inTexture.read(gid + uint2(9, offset)).r,
		inTexture.read(gid + uint2(10, offset)).r
	};

#define Sample(x, y) uniforms.lumaKernel[y] * rawSamples[x]
	const half luminance =
		Sample(0, 4) + Sample(1, 5) + Sample(2, 6) +
		Sample(3, 7) +
		Sample(4, 6) + Sample(5, 5) + Sample(6, 4);
#undef Sample

	return setSeparatedLumaChroma(luminance, centreSample, outTexture, gid, offset);
}

kernel void separateLumaKernel5(
	const metal::texture2d<half, metal::access::read> inTexture [[texture(0)]],
	const metal::texture2d<half, metal::access::write> outTexture [[texture(1)]],
	const uint2 gid [[thread_position_in_grid]],
	const constant Uniforms &uniforms [[buffer(0)]],
	const constant int &offset [[buffer(1)]]
) {
	const half4 centreSample = inTexture.read(gid + uint2(7, offset));
	const half rawSamples[] = {
		inTexture.read(gid + uint2(5, offset)).r,	inTexture.read(gid + uint2(6, offset)).r,
		centreSample.r,
		inTexture.read(gid + uint2(8, offset)).r,	inTexture.read(gid + uint2(9, offset)).r,
	};

#define Sample(x, y) uniforms.lumaKernel[y] * rawSamples[x]
	const half luminance =
		Sample(0, 5) + Sample(1, 6) +
		Sample(2, 7) +
		Sample(3, 6) + Sample(4, 5);
#undef Sample

	return setSeparatedLumaChroma(luminance, centreSample, outTexture, gid, offset);
}

// MARK: - Solid fills.

kernel void clearKernel(
	const metal::texture2d<half, metal::access::write> outTexture [[texture(0)]],
	const uint2 gid [[thread_position_in_grid]]
) {
	outTexture.write(half4(0.0f, 0.0f, 0.0f, 1.0f), gid);
}
