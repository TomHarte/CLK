//
//  ScanTarget.metal
//  Clock Signal
//
//  Created by Thomas Harte on 04/08/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#include <metal_stdlib>
using namespace metal;

// These two structs are the same, but defined separately as an artefact
// of my learning process, and the fact that they soon won't be.

struct InputVertex {
	float4 position	[[attribute(0)]];
	float4 colour	[[attribute(1)]];
};

struct ColouredVertex {
	float4 position [[position]];
	float4 colour;
};

vertex ColouredVertex vertex_main(	device const InputVertex *vertices [[buffer(0)]],
									uint vid [[vertex_id]]) {
	ColouredVertex output;
	output.position = vertices[vid].position;
	output.colour = vertices[vid].colour;
	return output;
}

fragment float4 fragment_main(ColouredVertex vert [[stage_in]]) {
	return vert.colour;
}
