//
//  VertexArray.hpp
//  Clock Signal Kiosk
//
//  Created by Thomas Harte on 29/01/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include "Outputs/OpenGL/OpenGL.hpp"

namespace Outputs::Display::OpenGL {
/*!
	A vertex array plus its underlying buffer.
*/
class VertexArray {
public:
	template <typename ContainerT>
	VertexArray(const ContainerT &container) :
		VertexArray(container.size(), sizeof(container.front())) {}

	VertexArray(size_t num_elements, size_t element_size);
	~VertexArray();

	VertexArray() = default;
	VertexArray(VertexArray &&);
	VertexArray &operator =(VertexArray &&);

	void bind() const;
	void bind_buffer() const;
	void bind_all() const;

private:
	GLuint buffer_ = 0;
	GLuint vertex_array_ = 0;
};

}
