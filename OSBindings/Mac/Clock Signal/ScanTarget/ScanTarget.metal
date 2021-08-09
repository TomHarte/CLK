//
//  ScanTarget.metal
//  Clock Signal
//
//  Created by Thomas Harte on 04/08/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#include <metal_stdlib>

using namespace metal;

struct Uniforms {
	// This is used to scale scan positions, i.e. it provides the range
	// for mapping from scan-style integer positions into eye space.
	int2 scale;

	// Applies a multiplication to all cyclesSinceRetrace values.
	float cycleMultiplier;

	// This provides the intended height of a scan, in eye-coordinate terms.
	float lineWidth;

	// Provides zoom and offset to scale the source data.
	float3x3 sourceToDisplay;

	// Provides conversions to and from RGB for the active colour space.
	half3x3 toRGB;
	half3x3 fromRGB;

	// Describes the filter in use for chroma filtering; it'll be
	// 15 coefficients but they're symmetrical around the centre.
	half3 chromaKernel[8];

	// Describes the filter in use for luma filtering; 15 coefficients
	// symmetrical around the centre.
	half lumaKernel[8];

	// Sets the opacity at which output strips are drawn.
	half outputAlpha;

	// Sets the gamma power to which output colours are raised.
	half outputGamma;

	// Sets a brightness multiplier for output colours.
	half outputMultiplier;
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
	float unitColourPhase;		// i.e. one unit per circle.
	float colourPhase;			// i.e. 2*pi units per circle, just regular radians.
	half colourAmplitude [[flat]];
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
	output.unitColourPhase = mix(
		float(inputs[instanceID].endPoints[0].compositeAngle),
		float(inputs[instanceID].endPoints[1].compositeAngle),
		float((vertexID&2) >> 1)
	) / 64.0f;
	output.colourPhase = 2.0f * 3.141592654f * output.unitColourPhase;

	// Hence determine this quad's real shape, using vertexID to pick a corner.

	// position2d is now in the range [0, 1].
	const float2 sourcePosition = start + (float(vertexID&2) * 0.5f) * tangent + (float(vertexID&1) - 0.5f) * normal * uniforms.lineWidth;
	const float2 position2d = (uniforms.sourceToDisplay * float3(sourcePosition, 1.0f)).xy;

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

half2 quadrature(float phase) {
	return half2(cos(phase), sin(phase));
}

half4 composite(half level, half2 quadrature, half amplitude) {
	return half4(
		level,
		half2(0.5f) + quadrature*half(0.5f),
		amplitude
	);
}

// The luminance formats can be sampled either in their natural format, or to the intermediate
// composite format used for composition. Direct sampling is always for final output, so the two
// 8-bit formats also provide a gamma option.

half convertLuminance1(SourceInterpolator vert [[stage_in]], texture2d<ushort> texture [[texture(0)]]) {
	return clamp(half(texture.sample(standardSampler, vert.textureCoordinates).r), half(0.0f), half(1.0f));
}

half convertLuminance8(SourceInterpolator vert [[stage_in]], texture2d<half> texture [[texture(0)]]) {
	return texture.sample(standardSampler, vert.textureCoordinates).r;
}

half convertPhaseLinkedLuminance8(SourceInterpolator vert [[stage_in]], texture2d<half> texture [[texture(0)]]) {
	const int offset = int(vert.unitColourPhase * 4.0f) & 3;
	auto sample = texture.sample(standardSampler, vert.textureCoordinates);
	return sample[offset];
}


#define CompositeSet(name, type)	\
	fragment half4 sample##name(SourceInterpolator vert [[stage_in]], texture2d<type> texture [[texture(0)]], constant Uniforms &uniforms [[buffer(0)]]) {	\
		const half luminance = convert##name(vert, texture) * uniforms.outputMultiplier;	\
		return half4(half3(luminance), uniforms.outputAlpha);	\
	}	\
	\
	fragment half4 sample##name##WithGamma(SourceInterpolator vert [[stage_in]], texture2d<type> texture [[texture(0)]], constant Uniforms &uniforms [[buffer(0)]]) {	\
		const half luminance = pow(convert##name(vert, texture) * uniforms.outputMultiplier, uniforms.outputGamma);	\
		return half4(half3(luminance), uniforms.outputAlpha);	\
	}	\
	\
	fragment half4 compositeSample##name(SourceInterpolator vert [[stage_in]], texture2d<type> texture [[texture(0)]], constant Uniforms &uniforms [[buffer(0)]]) {	\
		const half luminance = convert##name(vert, texture) * uniforms.outputMultiplier;	\
		return composite(luminance, quadrature(vert.colourPhase), vert.colourAmplitude);	\
	}

CompositeSet(Luminance1, ushort);
CompositeSet(Luminance8, half);
CompositeSet(PhaseLinkedLuminance8, half);

#undef CompositeSet

// The luminance/phase format can produce either composite or S-Video.

/// @returns A 2d vector comprised where .x = luminance; .y = chroma.
half2 convertLuminance8Phase8(SourceInterpolator vert [[stage_in]], texture2d<half> texture [[texture(0)]]) {
	const auto luminancePhase = texture.sample(standardSampler, vert.textureCoordinates).rg;
	const half phaseOffset = 3.141592654 * 4.0 * luminancePhase.g;
	const half rawChroma = step(luminancePhase.g, half(0.75f)) * cos(vert.colourPhase + phaseOffset);
	return half2(luminancePhase.r, rawChroma);
}

fragment half4 compositeSampleLuminance8Phase8(SourceInterpolator vert [[stage_in]], texture2d<half> texture [[texture(0)]]) {
	const half2 luminanceChroma = convertLuminance8Phase8(vert, texture);
	const half luminance = mix(luminanceChroma.r, luminanceChroma.g, vert.colourAmplitude);
	return composite(luminance, quadrature(vert.colourPhase), vert.colourAmplitude);
}

fragment half4 sampleLuminance8Phase8(SourceInterpolator vert [[stage_in]], texture2d<half> texture [[texture(0)]]) {
	const half2 luminanceChroma = convertLuminance8Phase8(vert, texture);
	const half2 qam = quadrature(vert.colourPhase) * half(0.5f);
	return half4(luminanceChroma.r,
			half2(0.5f) + luminanceChroma.g*qam,
			half(1.0f));
}

fragment half4 directCompositeSampleLuminance8Phase8(SourceInterpolator vert [[stage_in]], texture2d<half> texture [[texture(0)]], constant Uniforms &uniforms [[buffer(0)]]) {
	const half2 luminanceChroma = convertLuminance8Phase8(vert, texture);
	const half luminance = mix(luminanceChroma.r * uniforms.outputMultiplier, luminanceChroma.g, vert.colourAmplitude);
	return half4(half3(luminance), uniforms.outputAlpha);
}

fragment half4 directCompositeSampleLuminance8Phase8WithGamma(SourceInterpolator vert [[stage_in]], texture2d<half> texture [[texture(0)]], constant Uniforms &uniforms [[buffer(0)]]) {
	const half2 luminanceChroma = convertLuminance8Phase8(vert, texture);
	const half luminance = mix(pow(luminanceChroma.r * uniforms.outputMultiplier, uniforms.outputGamma), luminanceChroma.g, vert.colourAmplitude);
	return half4(half3(luminance), uniforms.outputAlpha);
}


// All the RGB formats can produce RGB, composite or S-Video.

half3 convertRed8Green8Blue8(SourceInterpolator vert, texture2d<half> texture) {
	return texture.sample(standardSampler, vert.textureCoordinates).rgb;
}

half3 convertRed4Green4Blue4(SourceInterpolator vert, texture2d<ushort> texture) {
	const auto sample = texture.sample(standardSampler, vert.textureCoordinates).rg;
	return half3(sample.r&15, (sample.g >> 4)&15, sample.g&15) / 15.0f;
}

half3 convertRed2Green2Blue2(SourceInterpolator vert, texture2d<ushort> texture) {
	const auto sample = texture.sample(standardSampler, vert.textureCoordinates).r;
	return half3((sample >> 4)&3, (sample >> 2)&3, sample&3) / 3.0f;
}

half3 convertRed1Green1Blue1(SourceInterpolator vert, texture2d<ushort> texture) {
	const auto sample = texture.sample(standardSampler, vert.textureCoordinates).r;
	return clamp(half3(sample&4, sample&2, sample&1), half(0.0f), half(1.0f));
}

#define DeclareShaders(name, pixelType)	\
	fragment half4 sample##name(SourceInterpolator vert [[stage_in]], texture2d<pixelType> texture [[texture(0)]], constant Uniforms &uniforms [[buffer(0)]]) {	\
		return half4(convert##name(vert, texture), uniforms.outputAlpha);	\
	}	\
	\
	fragment half4 sample##name##WithGamma(SourceInterpolator vert [[stage_in]], texture2d<pixelType> texture [[texture(0)]], constant Uniforms &uniforms [[buffer(0)]]) {	\
		return half4(pow(convert##name(vert, texture), uniforms.outputGamma), uniforms.outputAlpha);	\
	}	\
	\
	fragment half4 svideoSample##name(SourceInterpolator vert [[stage_in]], texture2d<pixelType> texture [[texture(0)]], constant Uniforms &uniforms [[buffer(0)]]) {	\
		const auto colour = uniforms.fromRGB * convert##name(vert, texture);	\
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
		const auto colour = uniforms.fromRGB * convert##name(vert, texture);	\
		return mix(colour.r, dot(colour.gb, colourSubcarrier), half(vert.colourAmplitude));	\
	}	\
	\
	fragment half4 compositeSample##name(SourceInterpolator vert [[stage_in]], texture2d<pixelType> texture [[texture(0)]], constant Uniforms &uniforms [[buffer(0)]]) {	\
		const half2 colourSubcarrier = quadrature(vert.colourPhase);	\
		return composite(composite##name(vert, texture, uniforms, colourSubcarrier), colourSubcarrier, vert.colourAmplitude);	\
	}	\
	\
	fragment half4 directCompositeSample##name(SourceInterpolator vert [[stage_in]], texture2d<pixelType> texture [[texture(0)]], constant Uniforms &uniforms [[buffer(0)]]) {	\
		const half level = composite##name(vert, texture, uniforms, quadrature(vert.colourPhase)); 	\
		return half4(half3(level), uniforms.outputAlpha);	\
	}	\
	\
	fragment half4 directCompositeSample##name##WithGamma(SourceInterpolator vert [[stage_in]], texture2d<pixelType> texture [[texture(0)]], constant Uniforms &uniforms [[buffer(0)]]) {	\
		const half level = pow(composite##name(vert, texture, uniforms, quadrature(vert.colourPhase)), uniforms.outputGamma); 	\
		return half4(half3(level), uniforms.outputAlpha);	\
	}

DeclareShaders(Red8Green8Blue8, half)
DeclareShaders(Red4Green4Blue4, ushort)
DeclareShaders(Red2Green2Blue2, ushort)
DeclareShaders(Red1Green1Blue1, ushort)

fragment half4 copyFragment(CopyInterpolator vert [[stage_in]], texture2d<half> texture [[texture(0)]]) {
	return texture.sample(standardSampler, vert.textureCoordinates);
}

fragment half4 interpolateFragment(CopyInterpolator vert [[stage_in]], texture2d<half> texture [[texture(0)]]) {
	return texture.sample(linearSampler, vert.textureCoordinates);
}

fragment half4 clearFragment(constant Uniforms &uniforms [[buffer(0)]]) {
	return half4(0.0, 0.0, 0.0, uniforms.outputAlpha);
}

// MARK: - Compute kernels

/// Given input pixels of the form (luminance, 0.5 + 0.5*chrominance*cos(phase), 0.5 + 0.5*chrominance*sin(phase)), applies a lowpass
/// filter to the two chrominance parts, then uses the toRGB matrix to convert to RGB and stores.
template <bool applyGamma> void filterChromaKernel(	texture2d<half, access::read> inTexture [[texture(0)]],
													texture2d<half, access::write> outTexture [[texture(1)]],
													uint2 gid [[thread_position_in_grid]],
													constant Uniforms &uniforms [[buffer(0)]],
													constant int &offset [[buffer(1)]]) {
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
		outTexture.write(pow(output, uniforms.outputGamma), gid + uint2(7, offset));
	} else {
		outTexture.write(output, gid + uint2(7, offset));
	}
}

kernel void filterChromaKernelNoGamma(	texture2d<half, access::read> inTexture [[texture(0)]],
										texture2d<half, access::write> outTexture [[texture(1)]],
										uint2 gid [[thread_position_in_grid]],
										constant Uniforms &uniforms [[buffer(0)]],
										constant int &offset [[buffer(1)]]) {
	filterChromaKernel<false>(inTexture, outTexture, gid, uniforms, offset);
}

kernel void filterChromaKernelWithGamma(	texture2d<half, access::read> inTexture [[texture(0)]],
											texture2d<half, access::write> outTexture [[texture(1)]],
											uint2 gid [[thread_position_in_grid]],
											constant Uniforms &uniforms [[buffer(0)]],
											constant int &offset [[buffer(1)]]) {
	filterChromaKernel<true>(inTexture, outTexture, gid, uniforms, offset);
}

void setSeparatedLumaChroma(half luminance, half4 centreSample, texture2d<half, access::write> outTexture, uint2 gid, int offset) {
	// The mix/steps below ensures that the absence of a colour burst leads the colour subcarrier to be discarded.
	const half isColour = step(half(0.01f), centreSample.a);
	const half chroma = (centreSample.r - luminance) / mix(half(1.0f), centreSample.a, isColour);
	outTexture.write(half4(
			luminance / mix(half(1.0f), (half(1.0f) - centreSample.a), isColour),
			isColour * (centreSample.gb - half2(0.5f)) * chroma + half2(0.5f),
			1.0f
		),
		gid + uint2(7, offset));
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
kernel void separateLumaKernel15(	texture2d<half, access::read> inTexture [[texture(0)]],
									texture2d<half, access::write> outTexture [[texture(1)]],
									uint2 gid [[thread_position_in_grid]],
									constant Uniforms &uniforms [[buffer(0)]],
									constant int &offset [[buffer(1)]]) {
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

kernel void separateLumaKernel9(	texture2d<half, access::read> inTexture [[texture(0)]],
									texture2d<half, access::write> outTexture [[texture(1)]],
									uint2 gid [[thread_position_in_grid]],
									constant Uniforms &uniforms [[buffer(0)]],
									constant int &offset [[buffer(1)]]) {
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

kernel void separateLumaKernel7(	texture2d<half, access::read> inTexture [[texture(0)]],
									texture2d<half, access::write> outTexture [[texture(1)]],
									uint2 gid [[thread_position_in_grid]],
									constant Uniforms &uniforms [[buffer(0)]],
									constant int &offset [[buffer(1)]]) {
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

kernel void separateLumaKernel5(	texture2d<half, access::read> inTexture [[texture(0)]],
									texture2d<half, access::write> outTexture [[texture(1)]],
									uint2 gid [[thread_position_in_grid]],
									constant Uniforms &uniforms [[buffer(0)]],
									constant int &offset [[buffer(1)]]) {
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

kernel void clearKernel(	texture2d<half, access::write> outTexture [[texture(0)]],
							uint2 gid [[thread_position_in_grid]]) {
	outTexture.write(half4(0.0f, 0.0f, 0.0f, 1.0f), gid);
}
