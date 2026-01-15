//
//  ComputeKernels.metal
//  Clock Signal
//
//  Created by Thomas Harte on 22/12/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#include "uniforms.hpp"

#include <metal_stdlib>

// MARK: - Filters for the chrominance portion of an UnfilteredYUVAmplitude texture, to remove high-frequency noise.

/// Applies a filter and a colour space conversion, and optionally gamma.
///
/// In practice, this takes pixels in UnfilteredYUVAmplitude form, lowpasses the chrominance parts and converts to RGB,
/// though it's dependent on an appropriate filter being generated externally to do that.
template <bool applyGamma> void filterChromaKernel(
	const metal::texture2d<half, metal::access::read> inTexture,
	const metal::texture2d<half, metal::access::write> outTexture,
	const uint2 gid,
	const constant Uniforms &uniforms,
	const constant int &offset
) {
	constexpr half4 moveToZero(0.0f, 0.5f, 0.5f, 0.0f);
	const half4 rawSamples[] = {
		inTexture.read(gid + uint2(0, offset)) - moveToZero,	inTexture.read(gid + uint2(1, offset)) - moveToZero,
		inTexture.read(gid + uint2(2, offset)) - moveToZero,	inTexture.read(gid + uint2(3, offset)) - moveToZero,
		inTexture.read(gid + uint2(4, offset)) - moveToZero,	inTexture.read(gid + uint2(5, offset)) - moveToZero,
		inTexture.read(gid + uint2(6, offset)) - moveToZero,	inTexture.read(gid + uint2(7, offset)) - moveToZero,
		inTexture.read(gid + uint2(8, offset)) - moveToZero,	inTexture.read(gid + uint2(9, offset)) - moveToZero,
		inTexture.read(gid + uint2(10, offset)) - moveToZero,	inTexture.read(gid + uint2(11, offset)) - moveToZero,
		inTexture.read(gid + uint2(12, offset)) - moveToZero,	inTexture.read(gid + uint2(13, offset)) - moveToZero,
		inTexture.read(gid + uint2(14, offset)) - moveToZero,
		inTexture.read(gid + uint2(15, offset)) - moveToZero,
		inTexture.read(gid + uint2(16, offset)) - moveToZero,
		inTexture.read(gid + uint2(17, offset)) - moveToZero,	inTexture.read(gid + uint2(18, offset)) - moveToZero,
		inTexture.read(gid + uint2(19, offset)) - moveToZero,	inTexture.read(gid + uint2(20, offset)) - moveToZero,
		inTexture.read(gid + uint2(21, offset)) - moveToZero,	inTexture.read(gid + uint2(22, offset)) - moveToZero,
		inTexture.read(gid + uint2(23, offset)) - moveToZero,	inTexture.read(gid + uint2(24, offset)) - moveToZero,
		inTexture.read(gid + uint2(25, offset)) - moveToZero,	inTexture.read(gid + uint2(26, offset)) - moveToZero,
		inTexture.read(gid + uint2(27, offset)) - moveToZero,	inTexture.read(gid + uint2(28, offset)) - moveToZero,
		inTexture.read(gid + uint2(29, offset)) - moveToZero,	inTexture.read(gid + uint2(30, offset)) - moveToZero,
	};

#define Sample(x, y) uniforms.chromaKernel[y] * rawSamples[x].rgb
	const half3 colour =
		Sample(0, 0) +		Sample(1, 1) +		Sample(2, 2) +		Sample(3, 3) +
		Sample(4, 4) +		Sample(5, 5) +		Sample(6, 6) +		Sample(7, 7) +
		Sample(8, 8) +		Sample(9, 9) +		Sample(10, 10) +	Sample(11, 11) +
		Sample(12, 12) +	Sample(13, 13) +	Sample(14, 14) +
		Sample(15, 15) +
		Sample(16, 14) +	Sample(17, 13) +	Sample(18, 12) +
		Sample(19, 11) +	Sample(20, 10) +	Sample(21, 9) +		Sample(22, 8) +
		Sample(23, 7) +		Sample(24, 6) +		Sample(25, 5) +		Sample(26, 4) +
		Sample(27, 3) +		Sample(28, 2) +		Sample(29, 1) +		Sample(30, 0);
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
	const half2 luminanceChrominance,
	const half4 centreSample,
	const metal::texture2d<half, metal::access::write> outTexture,
	const uint2 gid,
	const int offset
) {
	// The mix/steps below ensures that the absence of a colour burst leads the colour subcarrier to be discarded.
	const half isColour = metal::step(half(0.01f), centreSample.a);
	const half chroma = luminanceChrominance.g / metal::mix(half(1.0f), centreSample.a, isColour);
	outTexture.write(half4(
			(luminanceChrominance.r - centreSample.a) / metal::mix(half(1.0f), half(1.0f - centreSample.a * 2.0), isColour),
			isColour * (centreSample.gb - half2(0.5f)) * chroma + half2(0.5f),
			1.0f
		),
		gid + uint2(7, offset)
	);
}

/// Given input pixels of the form:
///
///	{composite sample, cos(phase), sin(phase), colour amplitude}
///
/// Filters to separate luminance, subtracts also to obtain chrominance.
/// Outputs pixels in the form:
///
///	{luminance, 0.5 + 0.5*chrominance*cos(phase), 0.5 + 0.5*chrominance*sin(phase)}
///
/// i.e. in the correct form for consumption by filterChromaKernel, above.
///
/// Various forms are defined: separateLumaKernelX means by applying a filter kernel of size X.
/// Regardless of kernel size, weights are always taken to be centred on index 7 of the `lumaKernel`
/// member of `Uniforms`.

kernel void separateLumaKernel15(
	const metal::texture2d<half, metal::access::read> inTexture [[texture(0)]],
	const metal::texture2d<half, metal::access::write> outTexture [[texture(1)]],
	const uint2 gid [[thread_position_in_grid]],
	const constant Uniforms &uniforms [[buffer(0)]],
	const constant int &offset [[buffer(1)]]
) {
	const half4 centreSample = inTexture.read(gid + uint2(15, offset));
	const half rawSamples[] = {
		inTexture.read(gid + uint2(0, offset)).r,	inTexture.read(gid + uint2(1, offset)).r,
		inTexture.read(gid + uint2(2, offset)).r,	inTexture.read(gid + uint2(3, offset)).r,
		inTexture.read(gid + uint2(4, offset)).r,	inTexture.read(gid + uint2(5, offset)).r,
		inTexture.read(gid + uint2(6, offset)).r,	inTexture.read(gid + uint2(7, offset)).r,
		inTexture.read(gid + uint2(8, offset)).r,	inTexture.read(gid + uint2(9, offset)).r,
		inTexture.read(gid + uint2(10, offset)).r,	inTexture.read(gid + uint2(11, offset)).r,
		inTexture.read(gid + uint2(12, offset)).r,	inTexture.read(gid + uint2(13, offset)).r,
		inTexture.read(gid + uint2(14, offset)).r,
		centreSample.r,
		inTexture.read(gid + uint2(16, offset)).r,
		inTexture.read(gid + uint2(17, offset)).r,	inTexture.read(gid + uint2(18, offset)).r,
		inTexture.read(gid + uint2(19, offset)).r,	inTexture.read(gid + uint2(20, offset)).r,
		inTexture.read(gid + uint2(21, offset)).r,	inTexture.read(gid + uint2(22, offset)).r,
		inTexture.read(gid + uint2(23, offset)).r,	inTexture.read(gid + uint2(24, offset)).r,
		inTexture.read(gid + uint2(25, offset)).r,	inTexture.read(gid + uint2(26, offset)).r,
		inTexture.read(gid + uint2(27, offset)).r,	inTexture.read(gid + uint2(28, offset)).r,
		inTexture.read(gid + uint2(29, offset)).r,	inTexture.read(gid + uint2(30, offset)).r,
	};

#define Sample(x, y) uniforms.lumaKernel[y] * rawSamples[x]
	const half2 luminanceChrominance =
		Sample(0, 0) + 		Sample(1, 1) + 		Sample(2, 2) + 		Sample(3, 3) +
		Sample(4, 4) + 		Sample(5, 5) + 		Sample(6, 6) +		Sample(7, 7) +
		Sample(8, 8) + 		Sample(9, 9) + 		Sample(10, 10) +	Sample(11, 11) +
		Sample(12, 12) + 	Sample(13, 13) + 	Sample(14, 14) +
		Sample(15, 15) +
		Sample(16, 14) + 	Sample(17, 13) + 	Sample(18, 12) +
		Sample(19, 11) + 	Sample(20, 10) + 	Sample(21, 9) + 	Sample(22, 8) +
		Sample(23, 7) + 	Sample(24, 6) + 	Sample(25, 5) +		Sample(26, 4) +
		Sample(27, 3) + 	Sample(28, 2) + 	Sample(29, 1) +		Sample(30, 0);
#undef Sample

	return setSeparatedLumaChroma(luminanceChrominance, centreSample, outTexture, gid, offset);
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
	const half2 luminanceChrominance =
		Sample(0, 3) + Sample(1, 4) + Sample(2, 5) + Sample(3, 6) +
		Sample(4, 7) +
		Sample(5, 6) + Sample(6, 5) + Sample(7, 4) + Sample(8, 3);
#undef Sample

	return setSeparatedLumaChroma(luminanceChrominance, centreSample, outTexture, gid, offset);
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
	const half2 luminanceChrominance =
		Sample(0, 4) + Sample(1, 5) + Sample(2, 6) +
		Sample(3, 7) +
		Sample(4, 6) + Sample(5, 5) + Sample(6, 4);
#undef Sample

	return setSeparatedLumaChroma(luminanceChrominance, centreSample, outTexture, gid, offset);
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
	const half2 luminanceChrominance =
		Sample(0, 5) + Sample(1, 6) +
		Sample(2, 7) +
		Sample(3, 6) + Sample(4, 5);
#undef Sample

	return setSeparatedLumaChroma(luminanceChrominance, centreSample, outTexture, gid, offset);
}

// MARK: - Solid fills.

kernel void clearKernel(
	const metal::texture2d<half, metal::access::write> outTexture [[texture(0)]],
	const uint2 gid [[thread_position_in_grid]]
) {
	outTexture.write(half4(0.0f, 0.0f, 0.0f, 1.0f), gid);
}
