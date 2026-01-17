//
//  ComputeKernels.metal
//  Clock Signal
//
//  Created by Thomas Harte on 22/12/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#include "uniforms.hpp"

#include <metal_stdlib>

namespace {
constexpr constant uint KernelCentre = 15;
}

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

#define Sample(x) rawSamples[x].rgb * uniforms.chromaKernel[x > KernelCentre ? KernelCentre - (x - KernelCentre) : x]
	const half3 colour =
		Sample(0) +		Sample(1) +		Sample(2) +		Sample(3) +
		Sample(4) +		Sample(5) +		Sample(6) +		Sample(7) +
		Sample(8) +		Sample(9) +		Sample(10) +	Sample(11) +
		Sample(12) +	Sample(13) +	Sample(14) +
		Sample(15) +
		Sample(16) +	Sample(17) +	Sample(18) +
		Sample(19) +	Sample(20) +	Sample(21) +	Sample(22) +
		Sample(23) +	Sample(24) +	Sample(25) +	Sample(26) +
		Sample(27) +	Sample(28) +	Sample(29) +	Sample(30);
#undef Sample

	const half4 output = half4(uniforms.toRGB * colour * uniforms.outputMultiplier, uniforms.outputAlpha);
	if(applyGamma) {
		outTexture.write(metal::pow(output, uniforms.outputGamma), gid + uint2(KernelCentre, offset));
	} else {
		outTexture.write(output, gid + uint2(KernelCentre, offset));
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
	const auto chromaScaler = metal::mix(half(1.0f), centreSample.a, isColour);
	const auto lumaScaler = metal::mix(half(1.0f), half(1.0f - centreSample.a * 2.0), isColour);

	const half chroma = luminanceChrominance.g / chromaScaler;
	outTexture.write(half4(
			(luminanceChrominance.r - centreSample.a) / lumaScaler,
			isColour * (centreSample.gb - half2(0.5f)) * chroma + half2(0.5f),
			1.0f
		),
		gid + uint2(KernelCentre, offset)
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
	const half4 centreSample = inTexture.read(gid + uint2(KernelCentre, offset));
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

#define Sample(x) rawSamples[x] * uniforms.lumaKernel[x > KernelCentre ? KernelCentre - (x - KernelCentre) : x]
	const half2 luminanceChrominance =
		Sample(0) + 	Sample(1) + 	Sample(2) + 	Sample(3) +
		Sample(4) + 	Sample(5) + 	Sample(6) +		Sample(7) +
		Sample(8) + 	Sample(9) + 	Sample(10) +	Sample(11) +
		Sample(12) + 	Sample(13) + 	Sample(14) +
		Sample(15) +
		Sample(16) + 	Sample(17) + 	Sample(18) +
		Sample(19) + 	Sample(20) + 	Sample(21) + 	Sample(22) +
		Sample(23) + 	Sample(24) + 	Sample(25) +	Sample(26) +
		Sample(27) + 	Sample(28) + 	Sample(29) +	Sample(30);
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
