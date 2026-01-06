//
//  ScanTarget.metal
//  Clock Signal
//
//  Created by Thomas Harte on 04/08/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#include <metal_stdlib>

#include "interpolators.hpp"
#include "uniforms.hpp"

using namespace metal;

namespace {

// The emulator never attempts to tile data, so the clamping method in use by these samplers is arbitrary; coordinates
// will never be clamped.
//
// That said, address::clamp_to_edge offers compatibility all the way back to MTLFeatureSet_iOS_GPUFamily1_v1.

constexpr sampler standardSampler(
	coord::pixel,
	address::clamp_to_edge,
	filter::nearest
);

constexpr sampler linearSampler(
	coord::pixel,
	address::clamp_to_edge,
	filter::linear
);

}

// MARK: - Input types.

enum class InputEncoding {
	Luminance1,
	Luminance8,
	PhaseLinkedLuminance8,

	Luminance8Phase8,

	Red8Green8Blue8,
	Red4Green4Blue4,
	Red2Green2Blue2,
	Red1Green1Blue1,
};
constexpr bool is_ttl(const InputEncoding encoding) {
	return encoding == InputEncoding::Luminance1 || encoding == InputEncoding::Red1Green1Blue1;
}

// Define the per-pixel type of input textures based on data format.
template <InputEncoding> struct DataFormat { using type = half; };
template<> struct DataFormat<InputEncoding::Luminance1> { using type = ushort; };
template<> struct DataFormat<InputEncoding::Red4Green4Blue4> { using type = ushort; };
template<> struct DataFormat<InputEncoding::Red2Green2Blue2> { using type = ushort; };
template<> struct DataFormat<InputEncoding::Red1Green1Blue1> { using type = ushort; };
template <InputEncoding encoding> using data_t = typename DataFormat<encoding>::type;

// Internal type aliases, correlating to the input data and intermediate buffers.
using Composite = half;					// i.e. a single sample of composite video.
using LuminanceChrominance = half2;		// i.e. a single sample of s-video; .x = luminance; .y = chroma.

using UnfilteredYUVAmplitude = half4;	// .x = pointwise luminance (colour subcarrier not yet removed);
										// .yz = two chrominance channels (with noise at twice the subcarrier frequency);
										// .w = amplitude of the chrominance channels.

using RGB = half3;

namespace {
constexpr half2 quadrature(const float phase) {
	return half2(cos(phase), sin(phase));
}

constexpr UnfilteredYUVAmplitude composite(const half level, const half2 quadrature, const half amplitude) {
	return half4(
		level,
		half2(0.5f) + quadrature*half(0.5f),
		amplitude
	);
}
}

// The luminance formats can be sampled either in their natural format, or to the intermediate
// composite format used for composition. Direct sampling is always for final output, so the two
// 8-bit formats also provide a gamma option.

// MARK: - Prototypical sampling functions.

template <InputEncoding encoding> RGB sample_rgb(SourceInterpolator, texture2d<data_t<encoding>>);
template <InputEncoding encoding> half sample_composite(SourceInterpolator, texture2d<data_t<encoding>>);

// MARK: - Composite sampling.

template <>
Composite sample_composite<InputEncoding::Luminance1>(
	const SourceInterpolator vert [[stage_in]],
	const texture2d<ushort> texture [[texture(0)]]
) {
	return clamp(half(texture.sample(standardSampler, vert.textureCoordinates).r), half(0.0f), half(1.0f));
}

template <>
Composite sample_composite<InputEncoding::Luminance8>(
	const SourceInterpolator vert [[stage_in]],
	const texture2d<half> texture [[texture(0)]]
) {
	return texture.sample(standardSampler, vert.textureCoordinates).r;
}

template <>
Composite sample_composite<InputEncoding::PhaseLinkedLuminance8>(
	const SourceInterpolator vert [[stage_in]],
	const texture2d<half> texture [[texture(0)]]
) {
	const int offset = int(vert.unitColourPhase * 4.0f) & 3;
	const auto sample = texture.sample(standardSampler, vert.textureCoordinates);
	return sample[offset];
}

#define CompositeSet(name, type)	\
	fragment half4 sample##name( \
		SourceInterpolator vert [[stage_in]], \
		texture2d<type> texture [[texture(0)]], \
		constant Uniforms &uniforms [[buffer(0)]] \
	) {	\
		const half luminance = sample_composite<InputEncoding::name>(vert, texture) * uniforms.outputMultiplier;	\
		return half4(half3(luminance), uniforms.outputAlpha);	\
	}	\
	\
	fragment half4 sample##name##WithGamma( \
		SourceInterpolator vert [[stage_in]], \
		texture2d<type> texture [[texture(0)]], \
		constant Uniforms &uniforms [[buffer(0)]] \
	) {	\
		const half luminance = pow( \
			sample_composite<InputEncoding::name>(vert, texture) * uniforms.outputMultiplier, \
			uniforms.outputGamma \
		);	\
		return half4(half3(luminance), uniforms.outputAlpha);	\
	}	\
	\
	fragment half4 compositeSample##name( \
		SourceInterpolator vert [[stage_in]], \
		texture2d<type> texture [[texture(0)]], \
		constant Uniforms &uniforms [[buffer(0)]] \
	) {	\
		const half luminance = sample_composite<InputEncoding::name>(vert, texture) * uniforms.outputMultiplier;	\
		return composite(luminance, quadrature(vert.colourPhase), vert.colourAmplitude);	\
	}

CompositeSet(Luminance1, ushort);
CompositeSet(Luminance8, half);
CompositeSet(PhaseLinkedLuminance8, half);

#undef CompositeSet

// The luminance/phase format can produce either composite or S-Video.

// MARK: - SVideo sampling.

template <InputEncoding encoding>
LuminanceChrominance sample_svideo(
	const SourceInterpolator vert [[stage_in]],
	const texture2d<half> texture [[texture(0)]]
) {
	if(encoding == InputEncoding::Luminance8Phase8) {
		const auto luminancePhase = texture.sample(standardSampler, vert.textureCoordinates).rg;
		const half phaseOffset = 3.141592654 * 4.0 * luminancePhase.g;
		const half rawChroma = step(luminancePhase.g, half(0.75f)) * cos(vert.colourPhase + phaseOffset);
		return half2(luminancePhase.r, rawChroma);
	}

	// TODO: sample_rgb and convert.
	return half2(0.0, 0.0);
}

fragment UnfilteredYUVAmplitude compositeSampleLuminance8Phase8(
	const SourceInterpolator vert [[stage_in]],
	const texture2d<half> texture [[texture(0)]]
) {
	const half2 luminanceChroma = sample_svideo<InputEncoding::Luminance8Phase8>(vert, texture);
	const half luminance = mix(luminanceChroma.r, luminanceChroma.g, vert.colourAmplitude);
	return composite(luminance, quadrature(vert.colourPhase), vert.colourAmplitude);
}

fragment half4 sampleLuminance8Phase8(
	const SourceInterpolator vert [[stage_in]],
	const texture2d<half> texture [[texture(0)]]
) {
	const half2 luminanceChroma = sample_svideo<InputEncoding::Luminance8Phase8>(vert, texture);
	const half2 qam = quadrature(vert.colourPhase) * half(0.5f);
	return half4(luminanceChroma.r,
			half2(0.5f) + luminanceChroma.g*qam,
			half(1.0f));
}

fragment half4 directCompositeSampleLuminance8Phase8(
	const SourceInterpolator vert [[stage_in]],
	const texture2d<half> texture [[texture(0)]],
	const constant Uniforms &uniforms [[buffer(0)]]
) {
	const half2 luminanceChroma = sample_svideo<InputEncoding::Luminance8Phase8>(vert, texture);
	const half luminance = mix(luminanceChroma.r * uniforms.outputMultiplier, luminanceChroma.g, vert.colourAmplitude);
	return half4(half3(luminance), uniforms.outputAlpha);
}

fragment half4 directCompositeSampleLuminance8Phase8WithGamma(
	const SourceInterpolator vert [[stage_in]],
	const texture2d<half> texture [[texture(0)]],
	const constant Uniforms &uniforms [[buffer(0)]]
) {
	const half2 luminanceChroma = sample_svideo<InputEncoding::Luminance8Phase8>(vert, texture);
	const half luminance = mix(
		pow(luminanceChroma.r * uniforms.outputMultiplier, uniforms.outputGamma),
		luminanceChroma.g,
		vert.colourAmplitude
	);
	return half4(half3(luminance), uniforms.outputAlpha);
}


// MARK: - RGB sampling.

template<> RGB sample_rgb<InputEncoding::Red8Green8Blue8>(
	const SourceInterpolator vert,
	const texture2d<half> texture
) {
	return texture.sample(standardSampler, vert.textureCoordinates).rgb;
}

template<> RGB sample_rgb<InputEncoding::Red4Green4Blue4>(
	const SourceInterpolator vert,
	const texture2d<ushort> texture
) {
	const auto sample = texture.sample(standardSampler, vert.textureCoordinates).rg;
	return half3(sample.r&15, (sample.g >> 4)&15, sample.g&15) / 15.0f;
}

template<> RGB sample_rgb<InputEncoding::Red2Green2Blue2>(
	const SourceInterpolator vert,
	const texture2d<ushort> texture
) {
	const auto sample = texture.sample(standardSampler, vert.textureCoordinates).r;
	return half3((sample >> 4)&3, (sample >> 2)&3, sample&3) / 3.0f;
}

template<> RGB sample_rgb<InputEncoding::Red1Green1Blue1>(
	const SourceInterpolator vert,
	const texture2d<ushort> texture
) {
	const auto sample = texture.sample(standardSampler, vert.textureCoordinates).r;
	return clamp(half3(sample&4, sample&2, sample&1), half(0.0f), half(1.0f));
}

#define DeclareShaders(name, pixelType)	\
	fragment half4 sample##name(SourceInterpolator vert [[stage_in]], texture2d<pixelType> texture [[texture(0)]], constant Uniforms &uniforms [[buffer(0)]]) {	\
		return half4(sample_rgb<InputEncoding::name>(vert, texture) * uniforms.outputMultiplier, uniforms.outputAlpha);	\
	}	\
	\
	fragment half4 sample##name##WithGamma(SourceInterpolator vert [[stage_in]], texture2d<pixelType> texture [[texture(0)]], constant Uniforms &uniforms [[buffer(0)]]) {	\
		return half4(pow(sample_rgb<InputEncoding::name>(vert, texture) * uniforms.outputMultiplier, uniforms.outputGamma), uniforms.outputAlpha);	\
	}	\
	\
	fragment half4 svideoSample##name(SourceInterpolator vert [[stage_in]], texture2d<pixelType> texture [[texture(0)]], constant Uniforms &uniforms [[buffer(0)]]) {	\
		const auto colour = uniforms.fromRGB * sample_rgb<InputEncoding::name>(vert, texture);	\
		const half2 qam = quadrature(vert.colourPhase);	\
		const half chroma = dot(colour.gb, qam);	\
		return half4(	\
			colour.r,	\
			half2(0.5f) + chroma*qam*half(0.5f),	\
			half(1.0f)		\
		);	\
	}	\
	\
	half composite##name(SourceInterpolator vert, texture2d<pixelType> texture, constant Uniforms &uniforms, half2 colourSubcarrier) {	\
		const auto colour = uniforms.fromRGB * sample_rgb<InputEncoding::name>(vert, texture);	\
		return mix(colour.r, dot(colour.gb, colourSubcarrier), half(vert.colourAmplitude));	\
	}	\
	\
	fragment half4 compositeSample##name(SourceInterpolator vert [[stage_in]], texture2d<pixelType> texture [[texture(0)]], constant Uniforms &uniforms [[buffer(0)]]) {	\
		const half2 colourSubcarrier = quadrature(vert.colourPhase);	\
		return composite(composite##name(vert, texture, uniforms, colourSubcarrier), colourSubcarrier, vert.colourAmplitude);	\
	}	\
	\
	fragment half4 directCompositeSample##name(SourceInterpolator vert [[stage_in]], texture2d<pixelType> texture [[texture(0)]], constant Uniforms &uniforms [[buffer(0)]]) {	\
		const half level = composite##name(vert, texture, uniforms, quadrature(vert.colourPhase));	\
		return half4(half3(level), uniforms.outputAlpha);	\
	}	\
	\
	fragment half4 directCompositeSample##name##WithGamma(SourceInterpolator vert [[stage_in]], texture2d<pixelType> texture [[texture(0)]], constant Uniforms &uniforms [[buffer(0)]]) {	\
		const half level = pow(composite##name(vert, texture, uniforms, quadrature(vert.colourPhase)), uniforms.outputGamma);	\
		return half4(half3(level), uniforms.outputAlpha);	\
	}

DeclareShaders(Red8Green8Blue8, half)
DeclareShaders(Red4Green4Blue4, ushort)
DeclareShaders(Red2Green2Blue2, ushort)
DeclareShaders(Red1Green1Blue1, ushort)

#undef DeclareShaders

// Annoyance: templated functions, even if explicitly instantiated, don't seem to turn up in MTLLibrarys.
// So I need to do this nonsense:

#define DeclareShaders(name) \

//	fragment half4 internalComposite##name(\
		SourceInterpolator vert [[stage_in]],\
		texture2d<data_t<InputEncoding::name>> texture [[texture(0)]],\
		constant Uniforms &uniforms [[buffer(0)]]\
	) {	\
		const half level = sample_composite<InputEncoding::name>(vert, texture, uniforms, quadrature(vert.colourPhase)); \
		return half4(half3(level), uniforms.outputAlpha);	\
	} \
\
	fragment half4 outputComposite##name(\
		SourceInterpolator vert [[stage_in]],\
		texture2d<data_t<InputEncoding::name>> texture [[texture(0)]],\
		constant Uniforms &uniforms [[buffer(0)]]\
	) {	\
		const half level = sample_composite<InputEncoding::name>(vert, texture, uniforms, quadrature(vert.colourPhase)); \
		return half4(half3(pow(level, uniforms.outputGamma)), uniforms.outputAlpha);	\
	} \
\

//	fragment half4 internalRGB##name(\
		SourceInterpolator vert [[stage_in]],\
		texture2d<data_t<InputEncoding::name>> texture [[texture(0)]],\
		constant Uniforms &uniforms [[buffer(0)]]\
	) {	\
		return half4(sample_rgb<InputEncoding::name>(vert, texture) * uniforms.outputMultiplier, uniforms.outputAlpha);	\
	} \
\
	fragment half4 outputRGB##name(\
		SourceInterpolator vert [[stage_in]],\
		texture2d<data_t<InputEncoding::name>> texture [[texture(0)]],\
		constant Uniforms &uniforms [[buffer(0)]]\
	) {	\
		auto sample = sample_rgb<InputEncoding::name>(vert, texture) * uniforms.outputMultiplier; \
		if(!is_ttl(InputEncoding::name)) { \
			sample = pow(sample, uniforms.outputGamma);	\
		} \
		return half4(sample, uniforms.outputAlpha); \
	}

DeclareShaders(Luminance1);
DeclareShaders(Luminance8);
DeclareShaders(PhaseLinkedLuminance8);
DeclareShaders(Luminance8Phase8);
DeclareShaders(Red1Green1Blue1);
DeclareShaders(Red2Green2Blue2);
DeclareShaders(Red4Green4Blue4);
DeclareShaders(Red8Green8Blue8);

#undef DeclareShaders

// MARK: - Copying and solid fills.

/// Point samples @c texture.
fragment half4 copyFragment(const CopyInterpolator vert [[stage_in]], const texture2d<half> texture [[texture(0)]]) {
	return texture.sample(standardSampler, vert.textureCoordinates);
}

/// Bilinearly samples @c texture.
fragment half4 interpolateFragment(
	const CopyInterpolator vert [[stage_in]],
	const texture2d<half> texture [[texture(0)]]
) {
	return texture.sample(linearSampler, vert.textureCoordinates);
}

/// Fills with black.
fragment half4 clearFragment(const constant Uniforms &uniforms [[buffer(0)]]) {
	return half4(0.0, 0.0, 0.0, uniforms.outputAlpha);
}
