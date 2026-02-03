//
//  CopyShader.hpp
//  Clock Signal Kiosk
//
//  Created by Thomas Harte on 29/01/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include "Outputs/OpenGL/Primitives/VertexArray.hpp"
#include "Outputs/OpenGL/Primitives/Shader.hpp"

#include <optional>

namespace Outputs::Display::OpenGL {

/*!
	Copies a source texture in its entirety to a destination, optionally applying
	a change in brightness and a gamma adjustment.

	This always copies the entirety of the source texture to the entirety of the
	target surface; hence no inputs are required to the vertex program. Simply
	issue a four-vertex triangle strip.

	TODO: consider colour adaptations beyond mere brightness.
	I want at least a 'tint' and am considering a full-on matrix application for any
	combination of tint, brightness and channel remapping — e.g. imagine a
	handheld console in which the native red pixels are some colour other than
	pure red.

	(would need support in the ScanTarget modals and therefore also a correlated
	change in the other scan targets)
*/
class CopyShader {
public:
	CopyShader(
		API,
		std::optional<float> brightness,	// Optionally: multiply all input
		std::optional<float> gamma
	);

	CopyShader() = default;
	CopyShader(CopyShader &&) = default;
	CopyShader &operator =(CopyShader &&) = default;

	void perform(const GLenum source);
	bool empty() const {
		return shader_.empty();
	}

private:
	Shader shader_;
	VertexArray vertices_;
	GLenum source_ = 0;
};

}
