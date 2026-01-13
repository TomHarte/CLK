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

using CompositeQAMAmplitude = half4;	// .x = pointwise composite (luminance + chrominance);
										// .yz = two quadrature subcarriers, i.e. cos(p), sin(p);
										// .w = amplitude of chrominance within the composite signal.

using UnfilteredYUVAmplitude = half4;	// .x = pointwise luminance (colour subcarrier not yet removed);
										// .yz = two chrominance channels (with noise at twice the subcarrier frequency);
										// .w = amplitude of the chrominance channels.


namespace {
constexpr half2 quadrature(const float phase) {
	return half2(metal::cos(phase), metal::sin(phase));
}
}

// MARK: - Composite samplers.

// Minor implementation note: regular SFINAE enable_if-type selection of implementations didn't
// seem to be supported and I'm still supporting the C++-11-derived version of Metal so if constexpr
// isn't available. Hence the function overload-based solution below and in the other sampler sections.

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
	// Passthrough.
	return underlying;
}

Composite sample_composite(
	const SourceInterpolator vert,
	half2,
	const constant Uniforms &,
	const LuminanceChrominance underlying
) {
	// Composite is a linear interpolation of the two S-Video channels with an above-zero offset.
	return
		metal::dot(
			underlying,
			half2(1.0 - 2 * vert.colourAmplitude, vert.colourAmplitude)
		) + vert.colourAmplitude;
}

Composite sample_composite(
	const SourceInterpolator vert,
	half2 colourSubcarrier,
	const constant Uniforms &uniforms,
	const RGB underlying
) {
	// Convert RGB to composite by switching colour space, applying the colour subcarrier to combine the two
	// chrominance channels into one, then mixing that on top of the luminance.
	const auto colour = uniforms.fromRGB * underlying;
	return
		metal::dot(
			half2(colour.r, metal::dot(colour.gb, colourSubcarrier)),
			half2(1.0 - 2 * vert.colourAmplitude, vert.colourAmplitude)
		) + vert.colourAmplitude;
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
	// Unused in practice; this isn't a composite decoding. Just present with no chrominance.
	return LuminanceChrominance(underlying, 0.0f);
}

LuminanceChrominance sample_svideo(
	const SourceInterpolator vert,
	half2,
	const constant Uniforms &,
	const LuminanceChrominance underlying
) {
	// Passthrough.
	return underlying;
}

LuminanceChrominance sample_svideo(
	const SourceInterpolator vert,
	half2 colourSubcarrier,
	constant Uniforms &uniforms,
	const RGB underlying
) {
	// Map RGB to S-Video by switching colour space and then applying the colour subcarrier.
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
	// Unused in practice; present composite video as pure black and white.
	return RGB(underlying);
}

RGB sample_rgb(
	const SourceInterpolator vert,
	const LuminanceChrominance underlying
) {
	// Also unused in practice; present only the luminance part of S-Video, rendering it black and white.
	return RGB(underlying.r);
}

RGB sample_rgb(
	const SourceInterpolator vert,
	const RGB underlying
) {
	// Passthrough.
	return underlying;
}

// MARK: - Fragment shaders.

// General discussion:
//
//	(i) 	gamma is optionally applied for the two 'output'-format samplers. Gamma is optional because sometimes
//			the user's computer will already have the same gamma curve as the guest platform;
//
//	(ii)	gamma is also not applied for any 'TTL' input encoding; those are the ones that are effectively binary,
//			so gamma will have no effect on output;
//
//	(iii)	'output' fragment shaders are available for RGB and composite sources, which more or less reflect the
//			reality that an RGB monitor does no processing on an RGB input and a monochrome composite monitor does
//			no processing on a composite input. Even though there are S-Video monitors (such as Commodore's), they
//			still have to transform the input signal to RGB for display; and
//
//	(iv)	the two 'input' shaders are coupled to the total pipeline used elsewhere. Check out the compute
//			kernels for relevant transforms.
//

/// "Internal" composite = a composite sample plus capture of the pure colour subcarrier plus a measure of colour amplitude within the signal.
/// It's an intermediate format used internally as the first step in composite decoding.
template <InputEncoding encoding>
CompositeQAMAmplitude internal_composite(
	const SourceInterpolator vert,
	const texture_t<encoding> texture,
	const constant Uniforms &uniforms
) {
	return CompositeQAMAmplitude(
		sample_composite<encoding>(vert, quadrature(vert.colourPhase), texture, uniforms),
		quadrature(vert.colourPhase),
		vert.colourAmplitude
	);
}

/// Output composite = the composite level, mapped to black and white, subject to the uniforms-defined output alpha.
/// So it's composite as presented for a human viewer.
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

/// "Internal" S-Video = luminance plus subcarrier-multiplied chrominance, scaled back up to the range [0, 1].
/// So it would be YUV (or the relevant colour space) except that there's noise at twice the colour subcarrier frequence within the chrominance channels.
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

/// Output RGB = direct RGB, subject to the uniforms-defined output alpha.
/// So as with output composite, it's for direct presentation to a user.
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
