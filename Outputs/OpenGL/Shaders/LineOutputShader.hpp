//
//  LineOutputShader.hpp
//  Clock Signal Kiosk
//
//  Created by Thomas Harte on 04/02/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include "Outputs/OpenGL/Primitives/VertexArray.hpp"
#include "Outputs/OpenGL/Primitives/Shader.hpp"

namespace Outputs::Display::OpenGL {

/*!
	Using `Line`s as input, draws output spans.
*/
Shader line_output_shader(
	API,
	int source_width,
	int source_height,
	int expected_vertical_lines,
	int scale_x,
	int scale_y,
	const VertexArray &,
	GLenum source_texture_unit
);

}
