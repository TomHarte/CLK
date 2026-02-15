//
//  Vertices.metal
//  Clock Signal
//
//  Created by Thomas Harte on 06/01/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "interpolators.hpp"
#include "uniforms.hpp"

#include <metal_stdlib>

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
		uint16_t cyclesSinceRetrace;
	} endPoints[2];

	uint16_t line;
};

// MARK: - Vertex shaders.

float2 textureLocation(
	const constant Line &line,
	const float offset,
	constant Uniforms &uniforms
) {
	const auto cyclesSinceRetrace =
		metal::mix(line.endPoints[0].cyclesSinceRetrace, line.endPoints[1].cyclesSinceRetrace, offset);
	return float2(
		uniforms.cycleMultiplier * cyclesSinceRetrace,
		line.line + 0.5f
	);
}

float2 textureLocation(
	const constant Scan &scan,
	const float offset,
	const constant Uniforms &
) {
	return float2(
		metal::mix(scan.endPoints[0].dataOffset, scan.endPoints[1].dataOffset, offset),
		scan.dataY + 0.5f);
}

template <typename InputT> float unitColourPhase(const constant InputT &, float);
template <typename InputT> half colourAmplitude(const constant InputT &);

template <> float unitColourPhase<Scan>(const constant Scan &scan, const float lateral) {
	return metal::mix(
		float(scan.endPoints[0].compositeAngle),
		float(scan.endPoints[1].compositeAngle),
		lateral
	) / 64.0f;
}
template <> float unitColourPhase<Line>(const constant Line &, const float) {
	return 0.0f;
}

template <> half colourAmplitude<Scan>(const constant Scan &scan) {
	return half(scan.compositeAmplitude) / half(255.0f);
}
template <> half colourAmplitude<Line>(const constant Line &scan) {
	return 0.0f;
}

template <typename Input> SourceInterpolator toDisplay(
	const constant Uniforms &uniforms [[buffer(1)]],
	const constant Input *const inputs [[buffer(0)]],
	const uint instanceID [[instance_id]],
	const uint vertexID [[vertex_id]]
) {
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
	const float2 normal = float2(tangent.y, -tangent.x) / metal::length(tangent);

	// Hence determine this quad's real shape, using vertexID to pick a corner.
	const float2 sourcePosition =
		start +
		(float(vertexID&2) * 0.5f) * tangent +
		(float(vertexID&1) - 0.5f) * normal * uniforms.lineWidth;
	const float2 position2d = (uniforms.sourceToDisplay * float3(sourcePosition, 1.0f)).xy;
	// position2d is now in the range [0, 1].

	const float phase = unitColourPhase<Input>(inputs[instanceID], float((vertexID&2) >> 1));

	return SourceInterpolator{
		.position = float4(
			position2d,
			0.0f,
			1.0f
		),
		.textureCoordinates = textureLocation(inputs[instanceID], float((vertexID&2) >> 1), uniforms),
		.unitColourPhase = phase,
		.colourPhase = 2.0f * 3.141592654f * phase,
		.colourAmplitude = colourAmplitude(inputs[instanceID]),
	};
}

// These next two assume the incoming geometry to be a four-vertex triangle strip;
// each instance will therefore produce a quad.

vertex SourceInterpolator scanToDisplay(
	const constant Uniforms &uniforms [[buffer(1)]],
	const constant Scan *const scans [[buffer(0)]],
	const uint instanceID [[instance_id]],
	const uint vertexID [[vertex_id]]
) {
	return toDisplay(uniforms, scans, instanceID, vertexID);
}

vertex SourceInterpolator lineToDisplay(
	const constant Uniforms &uniforms [[buffer(1)]],
	const constant Line *const lines [[buffer(0)]],
	const uint instanceID [[instance_id]],
	const uint vertexID [[vertex_id]]
) {
	return toDisplay(uniforms, lines, instanceID, vertexID);
}

// Generates endpoints for a line segment.
vertex SourceInterpolator scanToComposition(
	const constant Uniforms &uniforms [[buffer(1)]],
	const constant Scan *const scans [[buffer(0)]],
	const uint instanceID [[instance_id]],
	const uint vertexID [[vertex_id]],
	const metal::texture2d<float> texture [[texture(0)]]
) {
	SourceInterpolator result;

	// Populate result as if direct texture access were available.
	result.position.x =
		uniforms.cycleMultiplier *
		metal::mix(
			scans[instanceID].endPoints[0].cyclesSinceRetrace,
			scans[instanceID].endPoints[1].cyclesSinceRetrace,
			float(vertexID)
		);
	result.position.y = scans[instanceID].line;
	result.position.zw = float2(0.0f, 1.0f);

	result.textureCoordinates.x = metal::mix(
		scans[instanceID].endPoints[0].dataOffset,
		scans[instanceID].endPoints[1].dataOffset,
		float(vertexID)
	);
	result.textureCoordinates.y = scans[instanceID].dataY;

	result.unitColourPhase = metal::mix(
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
	const metal::texture2d<float> texture [[texture(0)]]
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
