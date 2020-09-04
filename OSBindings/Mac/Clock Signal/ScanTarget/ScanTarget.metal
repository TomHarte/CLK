//
//  ScanTarget.metal
//  Clock Signal
//
//  Created by Thomas Harte on 04/08/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#include <metal_stdlib>

using namespace metal;

// TODO: I'm being very loose, so far, in use of alpha. Sometimes it's 0.64, somtimes its 1.0.
// Apply some rigour, for crying out loud.

struct Uniforms {
	// This is used to scale scan positions, i.e. it provides the range
	// for mapping from scan-style integer positions into eye space.
	int2 scale;

	// This provides the intended height of a scan, in eye-coordinate terms.
	float lineWidth;

	// Provides a scaling factor in order to preserve 4:3 central content.
	float aspectRatioMultiplier;

	// Provides conversions to and from RGB for the active colour space.
	float3x3 toRGB;
	float3x3 fromRGB;

	// Provides zoom and offset to scale the source data.
	float zoom;
	float2 offset;

	// Describes the FIR filter in use for chroma filtering; it'll be
	// 15 coefficients but they're symmetrical around the centre.
	float3 chromaCoefficients[8];

	// Describes the FIR filter in use for luma filtering; also 15 coefficients
	// symmetrical around the centre.
	float2 lumaCoefficients[8];

	// Maps from pixel offsets into the composition buffer to angular difference.
	float radiansPerPixel;

	// Applies a multiplication to all cyclesSinceRetrace values.
	float cycleMultiplier;

	// Sets the opacity at which output strips are drawn.
	float outputAlpha;

	// Sets the gamma power to which output colours are raised.
	float outputGamma;

	// Sets a brightness multiplier for output colours.
	float outputMultiplier;
};

namespace {

constexpr sampler standardSampler(	coord::pixel,
									address::clamp_to_edge,	// Although arbitrary, stick with this address mode for compatibility all the way to MTLFeatureSet_iOS_GPUFamily1_v1.
									filter::nearest);

constexpr sampler linearSampler(	coord::pixel,
									address::clamp_to_edge,	// Although arbitrary, stick with this address mode for compatibility all the way to MTLFeatureSet_iOS_GPUFamily1_v1.
									filter::linear);

}

// MARK: - Structs used for receiving data from the emulation.

// This is intended to match the net effect of `Scan` as defined by the BufferingScanTarget.
struct Scan {
	struct EndPoint {
		uint16_t position[2];
		uint16_t dataOffset;
		int16_t compositeAngle;
		uint16_t cyclesSinceRetrace;
	} endPoints[2];

	uint8_t compositeAmplitude;
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
	float colourPhase;
	float colourAmplitude [[flat]];
};

struct CopyInterpolator {
	float4 position [[position]];
	float2 textureCoordinates;
};

// MARK: - Vertex shaders.

float2 textureLocation(constant Line *line, float offset, constant Uniforms &uniforms) {
	return float2(
		uniforms.cycleMultiplier * mix(line->endPoints[0].cyclesSinceRetrace, line->endPoints[1].cyclesSinceRetrace, offset),
		line->line + 0.5f);
}

float2 textureLocation(constant Scan *scan, float offset, constant Uniforms &) {
	return float2(
		mix(scan->endPoints[0].dataOffset, scan->endPoints[1].dataOffset, offset),
		scan->dataY + 0.5f);
}

template <typename Input> SourceInterpolator toDisplay(
	constant Uniforms &uniforms [[buffer(1)]],
	constant Input *inputs [[buffer(0)]],
	uint instanceID [[instance_id]],
	uint vertexID [[vertex_id]]) {
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
	output.colourPhase = 3.141592654f * mix(
		float(inputs[instanceID].endPoints[0].compositeAngle),
		float(inputs[instanceID].endPoints[1].compositeAngle),
		float((vertexID&2) >> 1)
	) / 32.0f;

	// Hence determine this quad's real shape, using vertexID to pick a corner.

	// position2d is now in the range [0, 1].
	float2 position2d = start + (float(vertexID&2) * 0.5f) * tangent + (float(vertexID&1) - 0.5f) * normal * uniforms.lineWidth;

	// Apply the requested offset and zoom, to map the desired area to the range [0, 1].
	position2d = (position2d + uniforms.offset) * uniforms.zoom;

	// Remap from [0, 1] to Metal's [-1, 1] and then apply the aspect ratio correction.
	position2d = (position2d * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f)) * float2(uniforms.aspectRatioMultiplier, 1.0f);

	output.position = float4(
		position2d,
		0.0f,
		1.0f
	);
	output.textureCoordinates = textureLocation(&inputs[instanceID], float((vertexID&2) >> 1), uniforms);

	return output;
}

// These next two assume the incoming geometry to be a four-vertex triangle strip; each instance will therefore
// produce a quad.

vertex SourceInterpolator scanToDisplay(	constant Uniforms &uniforms [[buffer(1)]],
											constant Scan *scans [[buffer(0)]],
											uint instanceID [[instance_id]],
											uint vertexID [[vertex_id]]) {
	return toDisplay(uniforms, scans, instanceID, vertexID);
}

vertex SourceInterpolator lineToDisplay(	constant Uniforms &uniforms [[buffer(1)]],
											constant Line *lines [[buffer(0)]],
											uint instanceID [[instance_id]],
											uint vertexID [[vertex_id]]) {
	return toDisplay(uniforms, lines, instanceID, vertexID);
}

// This assumes that it needs to generate endpoints for a line segment.

vertex SourceInterpolator scanToComposition(	constant Uniforms &uniforms [[buffer(1)]],
												constant Scan *scans [[buffer(0)]],
												uint instanceID [[instance_id]],
												uint vertexID [[vertex_id]],
												texture2d<float> texture [[texture(0)]]) {
	SourceInterpolator result;

	// Populate result as if direct texture access were available.
	result.position.x = uniforms.cycleMultiplier * mix(scans[instanceID].endPoints[0].cyclesSinceRetrace, scans[instanceID].endPoints[1].cyclesSinceRetrace, float(vertexID));
	result.position.y = scans[instanceID].line;
	result.position.zw = float2(0.0f, 1.0f);

	result.textureCoordinates.x = mix(scans[instanceID].endPoints[0].dataOffset, scans[instanceID].endPoints[1].dataOffset, float(vertexID));
	result.textureCoordinates.y = scans[instanceID].dataY;

	result.colourPhase = 3.141592654f * mix(
		float(scans[instanceID].endPoints[0].compositeAngle),
		float(scans[instanceID].endPoints[1].compositeAngle),
		float(vertexID)
	) / 32.0f;
	result.colourAmplitude = float(scans[instanceID].compositeAmplitude) / 255.0f;

	// Map position into eye space, allowing for target texture dimensions.
	const float2 textureSize = float2(texture.get_width(), texture.get_height());
	result.position.xy =
		((result.position.xy + float2(0.0f, 0.5f)) / textureSize)
		* float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);

	return result;
}

vertex CopyInterpolator copyVertex(uint vertexID [[vertex_id]], texture2d<float> texture [[texture(0)]]) {
	CopyInterpolator vert;

	const uint x = vertexID & 1;
	const uint y = (vertexID >> 1) & 1;

	vert.textureCoordinates = float2(
		x * texture.get_width(),
		y * texture.get_height()
	);
	vert.position = float4(
		float(x) * 2.0 - 1.0,
		1.0 - float(y) * 2.0,
		0.0,
		1.0
	);

	return vert;
}

// MARK: - Various input format conversion samplers.

float2 quadrature(float phase) {
	return float2(cos(phase), sin(phase));
}

float4 composite(float level, float2 quadrature, float amplitude) {
	return float4(
		level,
		float2(0.5f) + quadrature*0.5f,
		amplitude
	);
}

// There's only one meaningful way to sample the luminance formats.

fragment float4 sampleLuminance1(SourceInterpolator vert [[stage_in]], texture2d<ushort> texture [[texture(0)]]) {
	return composite(texture.sample(standardSampler, vert.textureCoordinates).r, quadrature(vert.colourPhase), vert.colourAmplitude);
}

fragment float4 sampleLuminance8(SourceInterpolator vert [[stage_in]], texture2d<float> texture [[texture(0)]]) {
	return composite(texture.sample(standardSampler, vert.textureCoordinates).r, quadrature(vert.colourPhase), vert.colourAmplitude);
}

fragment float4 samplePhaseLinkedLuminance8(SourceInterpolator vert [[stage_in]], texture2d<float> texture [[texture(0)]]) {
	const int offset = int(vert.colourPhase * 4.0);
	auto sample = texture.sample(standardSampler, vert.textureCoordinates);
	return composite(sample[offset], quadrature(vert.colourPhase), vert.colourAmplitude);
}

// The luminance/phase format can produce either composite or S-Video.

/// @returns A 2d vector comprised where .x = luminance; .y = chroma.
float2 convertLuminance8Phase8(SourceInterpolator vert [[stage_in]], texture2d<float> texture [[texture(0)]]) {
	const auto luminancePhase = texture.sample(standardSampler, vert.textureCoordinates).rg;
	const float phaseOffset = 3.141592654 * 4.0 * luminancePhase.g;
	const float rawChroma = step(luminancePhase.g, 0.75) * cos(vert.colourPhase + phaseOffset);
	return float2(luminancePhase.r, rawChroma);
}

fragment float4 sampleLuminance8Phase8(SourceInterpolator vert [[stage_in]], texture2d<float> texture [[texture(0)]]) {
	const float2 luminanceChroma = convertLuminance8Phase8(vert, texture);
	const float2 qam = quadrature(vert.colourPhase) * 0.5f;
	return float4(luminanceChroma.r,
			float2(0.5f) + luminanceChroma.g*qam,
			1.0);
}

fragment float4 compositeSampleLuminance8Phase8(SourceInterpolator vert [[stage_in]], texture2d<float> texture [[texture(0)]]) {
	const float2 luminanceChroma = convertLuminance8Phase8(vert, texture);
	const float level = mix(luminanceChroma.r, luminanceChroma.g, vert.colourAmplitude);
	return composite(level, quadrature(vert.colourPhase), vert.colourAmplitude);
}

// All the RGB formats can produce RGB, composite or S-Video.
//
// Note on the below: in Metal you may not call a fragment function (so e.g. svideoSampleX can't just cann sampleX).
// Also I can find no functioning way to offer a templated fragment function. So I don't currently know how
// I could avoid the macro mess below.

// TODO: is the calling convention here causing `vert` and `texture` to be copied?
float3 convertRed8Green8Blue8(SourceInterpolator vert, texture2d<float> texture) {
	return float3(texture.sample(standardSampler, vert.textureCoordinates));
}

float3 convertRed4Green4Blue4(SourceInterpolator vert, texture2d<ushort> texture) {
	const auto sample = texture.sample(standardSampler, vert.textureCoordinates).rg;
	return float3(sample.r&15, (sample.g >> 4)&15, sample.g&15);
}

float3 convertRed2Green2Blue2(SourceInterpolator vert, texture2d<ushort> texture) {
	const auto sample = texture.sample(standardSampler, vert.textureCoordinates).r;
	return float3((sample >> 4)&3, (sample >> 2)&3, sample&3);
}

float3 convertRed1Green1Blue1(SourceInterpolator vert, texture2d<ushort> texture) {
	const auto sample = texture.sample(standardSampler, vert.textureCoordinates).r;
	return float3(sample&4, sample&2, sample&1);
}

// TODO: don't hard code the 0.64 in sample##name.
#define DeclareShaders(name, pixelType)	\
	fragment float4 sample##name(SourceInterpolator vert [[stage_in]], texture2d<pixelType> texture [[texture(0)]]) {	\
		return float4(convert##name(vert, texture), 0.64);	\
	}	\
	\
	fragment float4 svideoSample##name(SourceInterpolator vert [[stage_in]], texture2d<pixelType> texture [[texture(0)]], constant Uniforms &uniforms [[buffer(0)]]) {	\
		const auto colour = uniforms.fromRGB * clamp(convert##name(vert, texture), float(0.0f), float(1.0f));	\
		const float2 qam = quadrature(vert.colourPhase);	\
		const float chroma = dot(colour.gb, qam);	\
		return float4(	\
			colour.r,	\
			float2(0.5f) + chroma*qam*0.5f,	\
			1.0f		\
		);	\
	}	\
	\
	fragment float4 compositeSample##name(SourceInterpolator vert [[stage_in]], texture2d<pixelType> texture [[texture(0)]], constant Uniforms &uniforms [[buffer(0)]]) {	\
		const auto colour = uniforms.fromRGB * clamp(convert##name(vert, texture), float3(0.0f), float3(1.0f));	\
		const float2 colourSubcarrier = quadrature(vert.colourPhase);	\
		const float level = mix(colour.r, dot(colour.gb, colourSubcarrier), vert.colourAmplitude);	\
		return composite(level, colourSubcarrier, vert.colourAmplitude);	\
	}

DeclareShaders(Red8Green8Blue8, float)
DeclareShaders(Red4Green4Blue4, ushort)
DeclareShaders(Red2Green2Blue2, ushort)
DeclareShaders(Red1Green1Blue1, ushort)

fragment float4 copyFragment(CopyInterpolator vert [[stage_in]], texture2d<float> texture [[texture(0)]]) {
	return texture.sample(standardSampler, vert.textureCoordinates);
}

fragment float4 interpolateFragment(CopyInterpolator vert [[stage_in]], texture2d<float> texture [[texture(0)]]) {
	return texture.sample(linearSampler, vert.textureCoordinates);
}

fragment float4 clearFragment() {
	return float4(0.0, 0.0, 0.0, 0.64);
}

// MARK: - Compute kernels

/// Given input pixels of the form (luminance, 0.5 + 0.5*chrominance*cos(phase), 0.5 + 0.5*chrominance*sin(phase)), applies a lowpass
/// filter to the two chrominance parts, then uses the toRGB matrix to convert to RGB and stores.
kernel void filterChromaKernel(	texture2d<float, access::read> inTexture [[texture(0)]],
								texture2d<float, access::write> outTexture [[texture(1)]],
								uint2 gid [[thread_position_in_grid]],
								constant Uniforms &uniforms [[buffer(0)]],
								constant int &offset [[buffer(1)]]) {
	constexpr float4 moveToZero = float4(0.0f, 0.5f, 0.5f, 0.0f);
	const float4 rawSamples[] = {
		inTexture.read(gid + uint2(0, offset))  - moveToZero,
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

#define Sample(x, y) uniforms.chromaCoefficients[y] * rawSamples[x].rgb
	const float3 colour =
		Sample(0, 0) + Sample(1, 1) + Sample(2, 2) + Sample(3, 3) + Sample(4, 4) + Sample(5, 5) + Sample(6, 6) +
		Sample(7, 7) +
		Sample(8, 6) + Sample(9, 5) + Sample(10, 4) + Sample(11, 3) + Sample(12, 2) + Sample(13, 1) + Sample(14, 0);
#undef Sample

	outTexture.write(float4(uniforms.toRGB * colour, 1.0f), gid + uint2(7, offset));
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
kernel void separateLumaKernel(	texture2d<float, access::read> inTexture [[texture(0)]],
								texture2d<float, access::write> outTexture [[texture(1)]],
								uint2 gid [[thread_position_in_grid]],
								constant Uniforms &uniforms [[buffer(0)]],
								constant int &offset [[buffer(1)]]) {
	const float4 centreSample = inTexture.read(gid + uint2(7, offset));
	const float2 rawSamples[] = {
		inTexture.read(gid + uint2(0, offset)).rr,
		inTexture.read(gid + uint2(1, offset)).rr,
		inTexture.read(gid + uint2(2, offset)).rr,
		inTexture.read(gid + uint2(3, offset)).rr,
		inTexture.read(gid + uint2(4, offset)).rr,
		inTexture.read(gid + uint2(5, offset)).rr,
		inTexture.read(gid + uint2(6, offset)).rr,
		centreSample.rr,
		inTexture.read(gid + uint2(8, offset)).rr,
		inTexture.read(gid + uint2(9, offset)).rr,
		inTexture.read(gid + uint2(10, offset)).rr,
		inTexture.read(gid + uint2(11, offset)).rr,
		inTexture.read(gid + uint2(12, offset)).rr,
		inTexture.read(gid + uint2(13, offset)).rr,
		inTexture.read(gid + uint2(14, offset)).rr,
	};

#define Sample(x, y) uniforms.lumaCoefficients[y] * rawSamples[x]
	const float2 luminance =
		Sample(0, 0) + Sample(1, 1) + Sample(2, 2) + Sample(3, 3) + Sample(4, 4) + Sample(5, 5) + Sample(6, 6) +
		Sample(7, 7) +
		Sample(8, 6) + Sample(9, 5) + Sample(10, 4) + Sample(11, 3) + Sample(12, 2) + Sample(13, 1) + Sample(14, 0);
#undef Sample

	// The mix/steps below ensures that the absence of a colour burst leads the colour subcarrier to be discarded.
	const float isColour = step(0.01, centreSample.a);
	outTexture.write(float4(
			mix(luminance.g, luminance.r / (1.0f - centreSample.a), isColour),
			isColour * (centreSample.gb - float2(0.5f)) * (centreSample.r - luminance.g) / mix(1.0f, centreSample.a, isColour) + float2(0.5f),
			1.0f
		),
		gid + uint2(7, offset));
}
