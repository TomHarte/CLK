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

	// This provides the intended height of a scan, in eye-coordinate terms.
	float lineWidth;

	// Provides a scaling factor in order to preserve 4:3 central content.
	float aspectRatioMultiplier;
};

// MARK: - Structs used for receiving data from the emulation.

// This is intended to match the net effect of `Scan` as defined by the BufferingScanTarget.
struct Scan {
	struct EndPoint {
		uint16_t position[2];
		uint16_t dataOffset;
		uint16_t compositeAngle;
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
		uint16_t cyclesSinceRetrace;
		uint16_t compositeAngle;
	} endPoints[2];

	uint16_t line;
	uint8_t compositeAmplitude;
};

// MARK: - Intermediate structs.

// This is an intermediate struct, which is TEMPORARY.
struct SourceInterpolator {
	float4 position [[position]];
	float2 textureCoordinates;
	float colourPhase;
	float colourAmplitude;
};


// MARK: - Scan shaders; these do final output to the display.

vertex SourceInterpolator scanToDisplay(	constant Uniforms &uniforms [[buffer(1)]],
											constant Scan *scans [[buffer(0)]],
											uint instanceID [[instance_id]],
											uint vertexID [[vertex_id]]) {
	SourceInterpolator output;

	// Get start and end vertices in regular float2 form.
	const float2 start = float2(
		float(scans[instanceID].endPoints[0].position[0]) / float(uniforms.scale.x),
		float(scans[instanceID].endPoints[0].position[1]) / float(uniforms.scale.y)
	);
	const float2 end = float2(
		float(scans[instanceID].endPoints[1].position[0]) / float(uniforms.scale.x),
		float(scans[instanceID].endPoints[1].position[1]) / float(uniforms.scale.y)
	);

	// Calculate the tangent and normal.
	const float2 tangent = (end - start);
	const float2 normal = float2(-tangent.y, tangent.x) / length(tangent);

	// Load up the colour details.
	output.colourAmplitude = float(scans[instanceID].compositeAmplitude) / 255.0f;
	output.colourPhase = mix(
		float(scans[instanceID].endPoints[0].compositeAngle),
		float(scans[instanceID].endPoints[1].compositeAngle),
		float((vertexID&2) >> 1)
	) / 64.0;

	// Hence determine this quad's real shape, using vertexID to pick a corner.
	output.position = float4(
		((start + (float(vertexID&2) * 0.5) * tangent + (float(vertexID&1) - 0.5) * normal * uniforms.lineWidth) * float2(2.0, -2.0) + float2(-1.0, 1.0)) * float2(uniforms.aspectRatioMultiplier, 1.0),
		0.0,
		1.0
	);
	output.textureCoordinates = float2(
		mix(scans[instanceID].endPoints[0].dataOffset, scans[instanceID].endPoints[1].dataOffset, float((vertexID&2) >> 1)),
		scans[instanceID].dataY);
	return output;
}

namespace {

constexpr sampler standardSampler(	coord::pixel,
									address::clamp_to_edge,	// Although arbitrary, stick with this address mode for compatibility all the way to MTLFeatureSet_iOS_GPUFamily1_v1.
									filter::nearest);

}

// MARK: - Various input format conversion samplers.

/*
	Luminance1,				// 1 byte/pixel; any bit set => white; no bits set => black.
	Luminance8,				// 1 byte/pixel; linear scale.

	PhaseLinkedLuminance8,	// 4 bytes/pixel; each byte is an individual 8-bit luminance
							// value and which value is output is a function of
							// colour subcarrier phase — byte 0 defines the first quarter
							// of each colour cycle, byte 1 the next quarter, etc. This
							// format is intended to permit replay of sampled original data.

	// The luminance plus phase types describe a luminance and the phase offset
	// of a colour subcarrier. So they can be used to generate a luminance signal,
	// or an s-video pipeline.

	Luminance8Phase8,		// 2 bytes/pixel; first is luminance, second is phase.
							// Phase is encoded on a 192-unit circle; anything
							// greater than 192 implies that the colour part of
							// the signal should be omitted.

	// The RGB types can directly feed an RGB pipeline, naturally, or can be mapped
	// to phase+luminance, or just to luminance.

	Red1Green1Blue1,		// 1 byte/pixel; bit 0 is blue on or off, bit 1 is green, bit 2 is red.
	Red2Green2Blue2,		// 1 byte/pixel; bits 0 and 1 are blue, bits 2 and 3 are green, bits 4 and 5 are blue.
	Red4Green4Blue4,		// 2 bytes/pixel; first nibble is red, second is green, third is blue.
	Red8Green8Blue8,		// 4 bytes/pixel; first is red, second is green, third is blue, fourth is vacant.

 */


// There's only one meaningful way to sample the luminance formats.

fragment float4 sampleLuminance1(SourceInterpolator vert [[stage_in]], texture2d<ushort> texture [[texture(0)]]) {
	return float4(float3(texture.sample(standardSampler, vert.textureCoordinates).r), 1.0);
}

fragment float4 sampleLuminance8(SourceInterpolator vert [[stage_in]], texture2d<float> texture [[texture(0)]]) {
	return float4(float3(texture.sample(standardSampler, vert.textureCoordinates).r), 1.0);
}

fragment float4 samplePhaseLinkedLuminance8(SourceInterpolator vert [[stage_in]], texture2d<float> texture [[texture(0)]]) {
	const int offset = int(vert.colourPhase * 4.0);
	auto sample = texture.sample(standardSampler, vert.textureCoordinates);
	return float4(float3(sample[offset]), 1.0);
}

// The luminance/phase format can produce either composite or S-Video.

fragment float4 sampleLuminance8Phase8(SourceInterpolator vert [[stage_in]], texture2d<float> texture [[texture(0)]]) {
	return float4(texture.sample(standardSampler, vert.textureCoordinates).rg, 0.0, 1.0);
}

// All the RGB formats can produce RGB, composite or S-Video.

fragment float4 sampleRed8Green8Blue8(SourceInterpolator vert [[stage_in]], texture2d<float> texture [[texture(0)]]) {
	return float4(texture.sample(standardSampler, vert.textureCoordinates));
}

fragment float4 sampleRed1Green1Blue1(SourceInterpolator vert [[stage_in]], texture2d<ushort> texture [[texture(0)]]) {
	const auto sample = texture.sample(standardSampler, vert.textureCoordinates).r;
	return float4(sample&4, sample&2, sample&1, 1.0);
}

fragment float4 sampleRed2Green2Blue2(SourceInterpolator vert [[stage_in]], texture2d<ushort> texture [[texture(0)]]) {
	const auto sample = texture.sample(standardSampler, vert.textureCoordinates).r;
	return float4((sample >> 4)&3, (sample >> 2)&3, sample&3, 3.0) / 3.0;
}

fragment float4 sampleRed4Green4Blue4(SourceInterpolator vert [[stage_in]], texture2d<ushort> texture [[texture(0)]]) {
	const auto sample = texture.sample(standardSampler, vert.textureCoordinates).rg;
	return float4(sample.r&15, (sample.g >> 4)&15, sample.g&15, 15.0) / 15.0;
}
