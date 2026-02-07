//
//  CompositionShader.hpp
//  Clock Signal Kiosk
//
//  Created by Thomas Harte on 26/01/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include "Outputs/ScanTarget.hpp"
#include "Outputs/OpenGL/Primitives/VertexArray.hpp"
#include "Outputs/OpenGL/Primitives/Shader.hpp"

namespace Outputs::Display::OpenGL {

/*!
	A composition shader assembles scans into a line buffer. It can include a downard conversion
	from RGB to S-Video or composite, or from S-Video to composite.

	The shader is configured to accept a buffer of ScanTarget::Scan as vertex attributes.

	Output formats are:

		RGB: 		(r, g, b, 1)
		S-Video:	(luma, chroma * cos(phase), luma * sin(phase), 1)
		Composite:	(luma, cos(phase), sin(phase), chroma amplitude)

	Data flow:

	*	the separation shader takes input in 'composite' form and produce output in 's-video' form;
	*	the demodulation shader takes input in 's-video' form and produces output in 'RGB' form;
	*	chroma amplitude = 0 from composite output means that no chrominance is known to be
		present; in practice it means that no colour burst was detected.

	Implementation notes:

	*	phase carries forward in cos and sin form because those values will definitely be needed
		later in the pipeline and might be needed earlier.

	Aside: the demodulation shader only _finishes_ demodulation — the earlier multiply
	by cos and sin started it.
*/
Shader composition_shader(
	API,
	InputDataType,
	DisplayType,
	ColourSpace,
	float cyclesMultiplier,
	int source_width,
	int source_height,
	int target_width,
	int target_height,
	const VertexArray &,
	GLenum source_texture_unit
);

class ScanOutputShader {
public:
	ScanOutputShader(
		API,
		InputDataType,
		int expected_vertical_lines,
		int scale_x,
		int scale_y,
		int source_width,
		int source_height,
		const VertexArray &,
		GLenum source_texture_unit
	);
	ScanOutputShader() = default;

	void set_aspect_ratio_transformation(const std::array<float, 9> &);
	void bind();

	void reset() { shader_.reset(); }
	bool empty() const { return shader_.empty(); }

private:
	Shader shader_;
};

}
