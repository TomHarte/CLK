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

#include <array>

namespace Outputs::Display::OpenGL {

/*!
	Using `Line`s as input, draws output spans.
*/
class LineOutputShader {
public:
	LineOutputShader(
		API,
		int source_width,
		int source_height,
		float cycle_multiplier,
		int expected_vertical_lines,
		int scale_x,
		int scale_y,
		float alpha,
		const VertexArray &,
		GLenum source_texture_unit
	);
	LineOutputShader() = default;

	void set_aspect_ratio_transformation(const std::array<float, 9> &);
	void bind();
	void reset() {
		shader_.reset();
	}
	bool empty() const {
		return shader_.empty();
	}
private:
	Shader shader_;
};

}
