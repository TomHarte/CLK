//
//  Rectangle.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/07/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "Rectangle.hpp"

using namespace Outputs::Display::OpenGL;

Rectangle::Rectangle(const API api, const float x, const float y, const float width, const float height):
	pixel_shader_(
		api,
		R"glsl(
			in vec2 position;
			void main(void) {
				gl_Position = vec4(position, 0.0, 1.0);
			}
		)glsl",
		R"glsl(
			uniform vec4 colour;
			out vec4 fragColour;

			void main(void) {
				fragColour = colour;
			}
		)glsl"
	){
	pixel_shader_.bind();

	test_gl([&]{ glGenVertexArrays(1, &drawing_vertex_array_); });
	test_gl([&]{ glGenBuffers(1, &drawing_array_buffer_); });

	test_gl([&]{ glBindVertexArray(drawing_vertex_array_); });
	test_gl([&]{ glBindBuffer(GL_ARRAY_BUFFER, drawing_array_buffer_); });

	const GLint position_attribute = pixel_shader_.get_attrib_location("position");
	test_gl([&]{ glEnableVertexAttribArray(GLuint(position_attribute)); });

	test_gl([&]{
		glVertexAttribPointer(
			(GLuint)position_attribute,
			2,
			GL_FLOAT,
			GL_FALSE,
			2 * sizeof(GLfloat),
			(void *)0
		);
	});

	colour_uniform_ = pixel_shader_.get_uniform_location("colour");

	// Store positions.
	const float buffer[4*2] = {
		x,			y,
		x,			y + height,
		x + width,	y,
		x + width,	y + height
	};

	// Upload buffer.
	test_gl([&]{ glBindBuffer(GL_ARRAY_BUFFER, drawing_array_buffer_); });
	test_gl([&]{ glBufferData(GL_ARRAY_BUFFER, sizeof(buffer), buffer, GL_STATIC_DRAW); });
}

void Rectangle::draw(const float red, const float green, const float blue) {
	pixel_shader_.bind();
	test_gl([&]{ glUniform4f(colour_uniform_, red, green, blue, 1.0); });

	test_gl([&]{ glBindVertexArray(drawing_vertex_array_); });
	test_gl([&]{ glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); });
}
