//
//  CRTOpenGL.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/02/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "CRT.hpp"

// TODO: figure out correct include paths for other platforms.
#include <OpenGL/OpenGL.h>

using namespace Outputs;

namespace  {
	struct OpenGLState {
		GLuint _vertexShader, _fragmentShader;
		GLuint _shaderProgram;
		GLuint _arrayBuffer, _vertexArray;

		GLint _positionAttribute;
		GLint _textureCoordinatesAttribute;
		GLint _lateralAttribute;

		GLint _textureSizeUniform, _windowSizeUniform;
		GLint _boundsOriginUniform, _boundsSizeUniform;
		GLint _alphaUniform;

		GLuint _textureName, _shadowMaskTextureName;
	};
}

void CRT::draw_frame(int output_width, int output_height)
{
	printf("%d %d\n", output_width, output_height);
}

void CRT::set_openGL_context_will_change(bool should_delete_resources)
{
}

void CRT::set_composite_sampling_function(const char *shader)
{
}

void CRT::set_rgb_sampling_function(const char *shader)
{
	printf("%s\n", shader);
}
