//
//  Fragments.metal
//  Clock Signal
//
//  Created by Thomas Harte on 04/08/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#include "interpolators.hpp"
#include "input_encodings.hpp"
#include "uniforms.hpp"

#include <metal_stdlib>
#include <metal_type_traits>

// Internal type aliases, correlating to the input data and intermediate buffers.
using Composite = half;					// i.e. a single sample of composite video.

using UnfilteredYUVAmplitude = half4;	// .x = pointwise luminance (colour subcarrier not yet removed);
										// .yz = two chrominance channels (with noise at twice the subcarrier frequency);
										// .w = amplitude of the chrominance channels.

using RGB = half3;

namespace {
constexpr half2 quadrature(const float phase) {
	return half2(metal::cos(phase), metal::sin(phase));
}
}

// MARK: - Composite samplers.

// Minor implementation note: regular SFINAE enable_if-type selection of implementations didn't
// seem to be supported and I'm still supporting the C++-11-derived version of Metal so if constexpr
// isn't available. Hence the function overload-based solutions below.

template <InputEncoding encoding>
Composite sample_composite(
	const SourceInterpolator vert,
	half2 colourSubcarrier,
	const texture_t<encoding> texture,
	const constant Uniforms &uniforms
) {
	return sample_composite(vert, colourSubcarrier, uniforms, sample<encoding>(vert, texture));
}

Composite sample_composite(
	const SourceInterpolator vert,
	half2,
	const constant Uniforms &,
	const Luminance underlying
) {
	return underlying;
}

Composite sample_composite(
	const SourceInterpolator vert,
	half2,
	const constant Uniforms &,
	const LuminanceChrominance underlying
) {
	return metal::mix(underlying.r, underlying.g, vert.colourAmplitude);
}

Composite sample_composite(
	const SourceInterpolator vert,
	half2 colourSubcarrier,
	const constant Uniforms &uniforms,
	const RGB underlying
) {
	const auto colour = uniforms.fromRGB * underlying;
	return metal::mix(colour.r, metal::dot(colour.gb, colourSubcarrier), vert.colourAmplitude);
}

// MARK: - S-Video samplers.

template <InputEncoding encoding>
LuminanceChrominance sample_svideo(
	const SourceInterpolator vert,
	half2 colourSubcarrier,
	const texture_t<encoding> texture,
	const constant Uniforms &uniforms
) {
	return sample_svideo(vert, colourSubcarrier, uniforms, sample<encoding>(vert, texture));
}

LuminanceChrominance sample_svideo(
	const SourceInterpolator vert,
	half2 colourSubcarrier,
	constant Uniforms &uniforms,
	const Luminance underlying
) {
	return LuminanceChrominance(underlying, 0.0f);
}

LuminanceChrominance sample_svideo(
	const SourceInterpolator vert,
	half2,
	const constant Uniforms &,
	const LuminanceChrominance underlying
) {
	return underlying;
}

LuminanceChrominance sample_svideo(
	const SourceInterpolator vert,
	half2 colourSubcarrier,
	constant Uniforms &uniforms,
	const RGB underlying
) {
	const auto colour = uniforms.fromRGB * underlying;
	return LuminanceChrominance(colour.r, metal::dot(colour.gb, colourSubcarrier));
}

// MARK: - RGB samplers.

template <InputEncoding encoding>
RGB sample_rgb(
	const SourceInterpolator vert,
	const texture_t<encoding> texture
) {
	return sample_rgb(vert, sample<encoding>(vert, texture));
}

RGB sample_rgb(
	const SourceInterpolator vert,
	const Luminance underlying
) {
	return RGB(underlying);
}

RGB sample_rgb(
	const SourceInterpolator vert,
	const LuminanceChrominance underlying
) {
	return RGB(underlying.r);
}

RGB sample_rgb(
	const SourceInterpolator vert,
	const RGB underlying
) {
	return underlying;
}

// MARK: - Fragment shaders.

template <InputEncoding encoding>
half4 internal_composite(
	const SourceInterpolator vert,
	const texture_t<encoding> texture,
	const constant Uniforms &uniforms
) {
	return half4(
		sample_composite<encoding>(vert, quadrature(vert.colourPhase), texture, uniforms),
		quadrature(vert.colourPhase),
		vert.colourAmplitude
	);
}

template <InputEncoding encoding, bool with_gamma>
half4 output_composite(
	const SourceInterpolator vert,
	const texture_t<encoding> texture,
	const constant Uniforms &uniforms
) {
	const half level = sample_composite<encoding>(vert, quadrature(vert.colourPhase), texture, uniforms);
	if(is_ttl(encoding) || !with_gamma) {
		return half4(half3(level), uniforms.outputAlpha);
	}
	return half4(half3(metal::pow(level, uniforms.outputGamma)), uniforms.outputAlpha);
}

template <InputEncoding encoding>
UnfilteredYUVAmplitude internal_svideo(
	const SourceInterpolator vert,
	const texture_t<encoding> texture,
	const constant Uniforms &uniforms
) {
	const auto quad = quadrature(vert.colourPhase);
	const half2 luminanceChroma = sample_svideo<encoding>(vert, quad, texture, uniforms);
	const half2 qam = quad * half(0.5f);
	return half4(
		luminanceChroma.r,
		half2(0.5f) + luminanceChroma.g * qam,
		half(1.0f)
	);
}

template <InputEncoding encoding, bool with_gamma>
half4 output_rgb(
	const SourceInterpolator vert,
	const texture_t<encoding> texture,
	const constant Uniforms &uniforms
) {
	const auto level = sample_rgb<encoding>(vert, texture);
	if(is_ttl(encoding) || !with_gamma) {
		return half4(level, uniforms.outputAlpha);
	}
	return half4(metal::pow(level, uniforms.outputGamma), uniforms.outputAlpha);
}

//
// The templated functions above exactly describe all of the interesting fragment shaders.
// But even with explicit instantiation they don't seem to appear in the MTLLibrary.
//
// So the macros below bind appropriate instantiations to non-templated names for all
// available input formats.
//

#define DeclareShaders(name) \
	fragment half4 outputComposite##name(\
		SourceInterpolator vert [[stage_in]],\
		texture_t<InputEncoding::name> texture [[texture(0)]],\
		const constant Uniforms &uniforms [[buffer(0)]]\
	) {	\
		return output_composite<InputEncoding::name, false>(vert, texture, uniforms);	\
	}	\
	\
	fragment half4 outputCompositeWithGamma##name(\
		SourceInterpolator vert [[stage_in]],\
		texture_t<InputEncoding::name> texture [[texture(0)]],\
		const constant Uniforms &uniforms [[buffer(0)]]\
	) {	\
		return output_composite<InputEncoding::name, true>(vert, texture, uniforms);	\
	}	\
	\
	fragment half4 internalComposite##name(\
		SourceInterpolator vert [[stage_in]],\
		texture_t<InputEncoding::name> texture [[texture(0)]],\
		const constant Uniforms &uniforms [[buffer(0)]]\
	) {	\
		return internal_composite<InputEncoding::name>(vert, texture, uniforms);	\
	} \
	\
	fragment half4 internalSVideo##name(\
		SourceInterpolator vert [[stage_in]],\
		texture_t<InputEncoding::name> texture [[texture(0)]],\
		const constant Uniforms &uniforms [[buffer(0)]]\
	) {	\
		return internal_svideo<InputEncoding::name>(vert, texture, uniforms);	\
	} \
	\
	fragment half4 outputRGB##name(\
		SourceInterpolator vert [[stage_in]],\
		texture_t<InputEncoding::name> texture [[texture(0)]],\
		const constant Uniforms &uniforms [[buffer(0)]]\
	) {	\
		return output_rgb<InputEncoding::name, false>(vert, texture, uniforms);	\
	}	\
	\
	fragment half4 outputRGBWithGamma##name(\
		SourceInterpolator vert [[stage_in]],\
		texture_t<InputEncoding::name> texture [[texture(0)]],\
		const constant Uniforms &uniforms [[buffer(0)]]\
	) {	\
		return output_rgb<InputEncoding::name, true>(vert, texture, uniforms);	\
	}

AllEncodings(DeclareShaders);
