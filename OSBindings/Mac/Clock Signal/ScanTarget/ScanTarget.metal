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


// This is an intermediate struct, which is TEMPORARY.
struct ColouredVertex {
	float4 position [[position]];
};


// MARK: - Scan shaders; these do final output to the display.

vertex ColouredVertex scanVertexMain(	constant Uniforms &uniforms [[buffer(1)]],
										constant Scan *scans [[buffer(0)]],
										uint instanceID [[instance_id]],
										uint vertexID [[vertex_id]]) {
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

	// Hence determine this quad's real shape, using vertexID to pick a corner.
	ColouredVertex output;
	output.position = float4(
		((start + (float(vertexID&2) * 0.5) * tangent + (float(vertexID&1) - 0.5) * normal * uniforms.lineWidth) * float2(2.0, -2.0) + float2(-1.0, 1.0)) * float2(uniforms.aspectRatioMultiplier, 1.0),
		0.0,
		1.0
	);
	return output;
}

fragment half4 scanFragmentMain(ColouredVertex vert [[stage_in]]) {
	return half4(1.0);
}
