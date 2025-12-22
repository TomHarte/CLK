//
//  ScanTarget.metal
//  Clock Signal
//
//  Created by Thomas Harte on 04/08/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#include <metal_stdlib>

#include "uniforms.hpp"

using namespace metal;

namespace {

// Although arbitrary, address::clamp_to_edge is used for compatibility all the way down
// to MTLFeatureSet_iOS_GPUFamily1_v1.

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

// MARK: - Structs used for receiving data from the emulation.

// This is intended to match the net effect of `Scan` as defined by the BufferingScanTarget,
// with a couple of added fields.
struct Scan {
	struct EndPoint {
		uint16_t position[2];
		uint16_t dataOffset;
		int16_t compositeAngle;
		uint16_t cyclesSinceRetrace;
	} endPoints[2];

	union {
		uint8_t compositeAmplitude;
		uint32_t padding;			// TODO: reuse some padding as the next two fields, to save two bytes.
	};
	uint16_t dataY;
	uint16_t line;
};

// This matches the BufferingScanTarget's `Line`.
struct Line {
	struct EndPoint {
		uint16_t position[2];
		int16_t compositeAngle;
		uint16_t cyclesSinceRetrace;
	} endPoints[2];

	uint8_t compositeAmplitude;
	uint16_t line;
};

// MARK: - Intermediate structs.

struct SourceInterpolator {
	float4 position [[position]];
	float2 textureCoordinates;
	float unitColourPhase;		// One unit per circle.
	float colourPhase;			// Radians.
	half colourAmplitude [[flat]];
};

struct CopyInterpolator {
	float4 position [[position]];
	float2 textureCoordinates;
};

// MARK: - Vertex shaders.

float2 textureLocation(
	constant Line *line,
	const float offset,
	constant Uniforms &uniforms
) {
	const auto cyclesSinceRetrace =
		mix(line->endPoints[0].cyclesSinceRetrace, line->endPoints[1].cyclesSinceRetrace, offset);
	return float2(
		uniforms.cycleMultiplier * cyclesSinceRetrace,
		line->line + 0.5f
	);
}

float2 textureLocation(
	constant Scan *const scan,
	const float offset,
	constant Uniforms &
) {
	return float2(
		mix(scan->endPoints[0].dataOffset, scan->endPoints[1].dataOffset, offset),
		scan->dataY + 0.5f);
}

template <typename Input> SourceInterpolator toDisplay(
	constant Uniforms &uniforms [[buffer(1)]],
	constant Input *const inputs [[buffer(0)]],
	const uint instanceID [[instance_id]],
	const uint vertexID [[vertex_id]]
) {
	SourceInterpolator output;

	// Get start and end vertices in regular float2 form.
	const float2 start = float2(
		float(inputs[instanceID].endPoints[0].position[0]) / float(uniforms.scale.x),
		float(inputs[instanceID].endPoints[0].position[1]) / float(uniforms.scale.y)
	);
	const float2 end = float2(
		float(inputs[instanceID].endPoints[1].position[0]) / float(uniforms.scale.x),
		float(inputs[instanceID].endPoints[1].position[1]) / float(uniforms.scale.y)
	);

	// Calculate the tangent and normal.
	const float2 tangent = (end - start);
	const float2 normal = float2(tangent.y, -tangent.x) / length(tangent);

	// Load up the colour details.
	output.colourAmplitude = float(inputs[instanceID].compositeAmplitude) / 255.0f;
	output.unitColourPhase = mix(
		float(inputs[instanceID].endPoints[0].compositeAngle),
		float(inputs[instanceID].endPoints[1].compositeAngle),
		float((vertexID&2) >> 1)
	) / 64.0f;
	output.colourPhase = 2.0f * 3.141592654f * output.unitColourPhase;

	// Hence determine this quad's real shape, using vertexID to pick a corner.

	// position2d is now in the range [0, 1].
	const float2 sourcePosition =
		start +
		(float(vertexID&2) * 0.5f) * tangent +
		(float(vertexID&1) - 0.5f) * normal * uniforms.lineWidth;
	const float2 position2d = (uniforms.sourceToDisplay * float3(sourcePosition, 1.0f)).xy;

	output.position = float4(
		position2d,
		0.0f,
		1.0f
	);
	output.textureCoordinates = textureLocation(&inputs[instanceID], float((vertexID&2) >> 1), uniforms);

	return output;
}

// These next two assume the incoming geometry to be a four-vertex triangle strip;
// each instance will therefore produce a quad.

vertex SourceInterpolator scanToDisplay(
	constant Uniforms &uniforms [[buffer(1)]],
	constant Scan *const scans [[buffer(0)]],
	const uint instanceID [[instance_id]],
	const uint vertexID [[vertex_id]]
) {
	return toDisplay(uniforms, scans, instanceID, vertexID);
}

vertex SourceInterpolator lineToDisplay(
	constant Uniforms &uniforms [[buffer(1)]],
	constant Line *const lines [[buffer(0)]],
	const uint instanceID [[instance_id]],
	const uint vertexID [[vertex_id]]
) {
	return toDisplay(uniforms, lines, instanceID, vertexID);
}

// Generates endpoints for a line segment.
vertex SourceInterpolator scanToComposition(
	constant Uniforms &uniforms [[buffer(1)]],
	constant Scan *const scans [[buffer(0)]],
	const uint instanceID [[instance_id]],
	const uint vertexID [[vertex_id]],
	const texture2d<float> texture [[texture(0)]]
) {
	SourceInterpolator result;

	// Populate result as if direct texture access were available.
	result.position.x =
		uniforms.cycleMultiplier *
		mix(
			scans[instanceID].endPoints[0].cyclesSinceRetrace,
			scans[instanceID].endPoints[1].cyclesSinceRetrace,
			float(vertexID)
		);
	result.position.y = scans[instanceID].line;
	result.position.zw = float2(0.0f, 1.0f);

	result.textureCoordinates.x = mix(
		scans[instanceID].endPoints[0].dataOffset,
		scans[instanceID].endPoints[1].dataOffset,
		float(vertexID)
	);
	result.textureCoordinates.y = scans[instanceID].dataY;

	result.unitColourPhase = mix(
		float(scans[instanceID].endPoints[0].compositeAngle),
		float(scans[instanceID].endPoints[1].compositeAngle),
		float(vertexID)
	) / 64.0f;
	result.colourPhase = 2.0f * 3.141592654f * result.unitColourPhase;
	result.colourAmplitude = float(scans[instanceID].compositeAmplitude) / 255.0f;

	// Map position into eye space, allowing for target texture dimensions.
	const float2 textureSize = float2(texture.get_width(), texture.get_height());
	result.position.xy =
		((result.position.xy + float2(0.0f, 0.5f)) / textureSize)
		* float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);

	return result;
}

vertex CopyInterpolator copyVertex(
	const uint vertexID [[vertex_id]],
	const texture2d<float> texture [[texture(0)]]
) {
	const uint x = vertexID & 1;
	const uint y = (vertexID >> 1) & 1;

	return CopyInterpolator{
		.textureCoordinates = float2(
			x * texture.get_width(),
			y * texture.get_height()
		),
		.position = float4(
			float(x) * 2.0 - 1.0,
			1.0 - float(y) * 2.0,
			0.0,
			1.0
		)
	};
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
half2 quadrature(const float phase) {
	return half2(cos(phase), sin(phase));
}

UnfilteredYUVAmplitude composite(const half level, const half2 quadrature, const half amplitude) {
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
	SourceInterpolator vert [[stage_in]],
	texture2d<half> texture [[texture(0)]]
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
	SourceInterpolator vert [[stage_in]],
	texture2d<half> texture [[texture(0)]]
) {
	const half2 luminanceChroma = sample_svideo<InputEncoding::Luminance8Phase8>(vert, texture);
	const half luminance = mix(luminanceChroma.r, luminanceChroma.g, vert.colourAmplitude);
	return composite(luminance, quadrature(vert.colourPhase), vert.colourAmplitude);
}

fragment half4 sampleLuminance8Phase8(SourceInterpolator vert [[stage_in]], texture2d<half> texture [[texture(0)]]) {
	const half2 luminanceChroma = sample_svideo<InputEncoding::Luminance8Phase8>(vert, texture);
	const half2 qam = quadrature(vert.colourPhase) * half(0.5f);
	return half4(luminanceChroma.r,
			half2(0.5f) + luminanceChroma.g*qam,
			half(1.0f));
}

fragment half4 directCompositeSampleLuminance8Phase8(
	SourceInterpolator vert [[stage_in]],
	texture2d<half> texture [[texture(0)]],
	constant Uniforms &uniforms [[buffer(0)]]
) {
	const half2 luminanceChroma = sample_svideo<InputEncoding::Luminance8Phase8>(vert, texture);
	const half luminance = mix(luminanceChroma.r * uniforms.outputMultiplier, luminanceChroma.g, vert.colourAmplitude);
	return half4(half3(luminance), uniforms.outputAlpha);
}

fragment half4 directCompositeSampleLuminance8Phase8WithGamma(
	SourceInterpolator vert [[stage_in]],
	texture2d<half> texture [[texture(0)]],
	constant Uniforms &uniforms [[buffer(0)]]
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
fragment half4 copyFragment(CopyInterpolator vert [[stage_in]], texture2d<half> texture [[texture(0)]]) {
	return texture.sample(standardSampler, vert.textureCoordinates);
}

/// Bilinearly samples @c texture.
fragment half4 interpolateFragment(
	CopyInterpolator vert [[stage_in]],
	texture2d<half> texture [[texture(0)]]
) {
	return texture.sample(linearSampler, vert.textureCoordinates);
}

/// Fills with black.
fragment half4 clearFragment(constant Uniforms &uniforms [[buffer(0)]]) {
	return half4(0.0, 0.0, 0.0, uniforms.outputAlpha);
}
