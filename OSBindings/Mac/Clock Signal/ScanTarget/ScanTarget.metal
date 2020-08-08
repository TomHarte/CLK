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
	int2 scale						[[attribute(0)]];

	// This provides the intended height of a scan, in eye-coordinate terms.
	float lineWidth					[[attribute(1)]];

	// Provides a scaling factor in order to preserve 4:3 central content.
	float aspectRatioMultiplier		[[attribute(2)]];
};

// This is intended to match `Scan` as defined by the BufferingScanTarget.
// Fields have been combined as necessary to make them all at least four
// bytes in size, since that is the required attribute alignment in Swift.
struct Scan {
	uint32_t startPosition			[[attribute(0)]];
	uint32_t startOffsetAndAngle	[[attribute(1)]];
	uint32_t startCycles			[[attribute(2)]];

	uint32_t endPosition			[[attribute(3)]];
	uint32_t endOffsetAndAngle		[[attribute(4)]];
	uint32_t endCycles				[[attribute(5)]];

	uint32_t compositeAmplitude		[[attribute(6)]];
	uint32_t dataYAndLine			[[attribute(7)]];
};

struct ColouredVertex {
	float4 position [[position]];
};


// MARK: - Scan shaders; these do final output to the display.

vertex ColouredVertex scanVertexMain(	constant Uniforms &uniforms [[buffer(1)]],
										constant Scan *scans [[buffer(0)]],
										uint instanceID [[instance_id]],
										uint vertexID [[vertex_id]]) {
	// Unpack start and end vertices; little-endian numbers are assumed here.
	const float2 start = float2(
		float(scans[instanceID].startPosition & 0xffff) / float(uniforms.scale.x),
		float(scans[instanceID].startPosition >> 16) / float(uniforms.scale.y)
	);
	const float2 end = float2(
		float(scans[instanceID].endPosition & 0xffff) / float(uniforms.scale.x),
		float(scans[instanceID].endPosition >> 16) / float(uniforms.scale.y)
	);

	// Calculate the tangent and normal.
	const float2 tangent = (end - start);
	const float2 normal = float2(-tangent.y, tangent.x) / length(tangent);

	// Hence determine this quad's real shape.
	ColouredVertex output;
	output.position = float4(
		((start + (float(vertexID&2) * 0.5) * tangent + (float(vertexID&1) - 0.5) * normal * uniforms.lineWidth) * float2(2.0, -2.0) + float2(-1.0, 1.0)) * float2(uniforms.aspectRatioMultiplier, 1.0),
		0.0,
		1.0
	);
	return output;
}

fragment float4 scanFragmentMain(ColouredVertex vert [[stage_in]]) {
	return float4(1.0);
}
