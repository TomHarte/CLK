//
//  KernelShaders.hpp
//  Clock Signal Kiosk
//
//  Created by Thomas Harte on 03/02/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include "Outputs/ScanTarget.hpp"
#include "Outputs/OpenGL/Primitives/VertexArray.hpp"
#include "Outputs/OpenGL/Primitives/Shader.hpp"

namespace Outputs::Display::OpenGL {

/*!
	Takes input in composite form, i.e.

		(luma, cos(phase), sin(phase), chroma amplitude)

	Applies the relevant filter as provided by an instance of Outputs::Display::Filtergenerator to output in S-Video form, i.e.

		(luma, chroma * cos(phase), luma * sin(phase), 1)

	Works only in terms of whole lines and uses instances of `DirtyZone` as input to indicate the regions that
	need to be translated. Both source and destination buffers are taken to be the same size.
*/
Shader separation_shader(
	API,
	float per_line_subcarrier_frequency,
	int samples_per_line,
	int buffer_width,
	int buffer_height,
	const VertexArray &,
	GLenum source_texture_unit
);

/*!
	Takes input in S-Video form, i.e.

		(luma, chroma * cos(phase), luma * sin(phase), 1)

	Applies the relevant filter as provided by an instance of Outputs::Display::Filtergenerator to output in RGB form.

	Works only in terms of whole lines and uses instances of `DirtyZone` as input to indicate the regions that
	need to be translated. Both source and destination buffers are taken to be the same size.

*/
Shader demodulation_shader(
	API,
	ColourSpace,
	float per_line_subcarrier_frequency,
	int samples_per_line,
	int buffer_width,
	int buffer_height,
	const VertexArray &,
	GLenum source_texture_unit
);

}
