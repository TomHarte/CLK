//
//  CompositionShader.hpp
//  Clock Signal Kiosk
//
//  Created by Thomas Harte on 26/01/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include "Outputs/OpenGL/Primitives/Shader.hpp"

namespace Outputs::Display::OpenGL {

/*!
	A composition shader processes `Scan`s into a line buffer. It can include a downard conversion
	from RGB to S-Video or composite, or from S-Video to composite.
*/
class CompositionShader {
public:
	CompositionShader();
	void bind() const;

private:
//	Shader shader_;
};

}
