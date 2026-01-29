//
//  CopyShader.hpp
//  Clock Signal Kiosk
//
//  Created by Thomas Harte on 29/01/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include "Outputs/OpenGL/Primitives/Shader.hpp"

#include <optional>

namespace Outputs::Display::OpenGL {

/*!
	Copies a source texture in its entirety to a destination, optionally applying
	a change in brightness and a gamma adjustment.
*/
Shader copy_shader(
	API,
	const GLenum source_texture_unit,
	std::optional<float> brightness,
	std::optional<float> gamma
);

}
