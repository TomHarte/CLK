//
//  Rectangle.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/07/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#pragma once

#include "Outputs/OpenGL/API.hpp"
#include "Outputs/OpenGL/OpenGL.hpp"
#include "Outputs/OpenGL/Primitives/Shader.hpp"
#include <memory>

namespace Outputs::Display::OpenGL {

/*!
	Provides a wrapper for drawing a solid, single-colour rectangle.
*/
class Rectangle {
public:
	/*!
		Instantiates an instance of Rectange with the coordinates given.
	*/
	Rectangle(API, float x, float y, float width, float height);
	~Rectangle();

	/*!
		Draws this rectangle in the colour supplied.
	*/
	void draw(float red, float green, float blue);

private:
	Shader pixel_shader_;
	GLuint drawing_vertex_array_ = 0, drawing_array_buffer_ = 0;
	GLint colour_uniform_;
};

}
