//
//  VertexArray.cpp
//  Clock Signal Kiosk
//
//  Created by Thomas Harte on 29/01/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "VertexArray.hpp"

#include <algorithm>

using namespace Outputs::Display::OpenGL;

VertexArray::VertexArray(const size_t num_elements, const size_t element_size) {
	const auto buffer_size = num_elements * element_size;

	test_gl([&]{ glGenBuffers(1, &buffer_); });
	test_gl([&]{ glBindBuffer(GL_ARRAY_BUFFER, buffer_); });
	test_gl([&]{ glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(buffer_size), NULL, GL_STREAM_DRAW); });

	test_gl([&]{ glGenVertexArrays(1, &vertex_array_); });
	test_gl([&]{ glBindVertexArray(vertex_array_); });
	test_gl([&]{ glBindBuffer(GL_ARRAY_BUFFER, buffer_); });
}

VertexArray::~VertexArray() {
	glDeleteBuffers(1, &buffer_);
	glDeleteVertexArrays(1, &vertex_array_);
}

VertexArray::VertexArray(VertexArray &&rhs) {
	*this = std::move(rhs);
}

VertexArray &VertexArray::operator =(VertexArray &&rhs) {
	std::swap(buffer_, rhs.buffer_);
	std::swap(vertex_array_, rhs.vertex_array_);
	return *this;
}

void VertexArray::bind() const {
	test_gl([&]{ glBindVertexArray(vertex_array_); });
}

void VertexArray::bind_buffer() const {
	test_gl([&]{ glBindBuffer(GL_ARRAY_BUFFER, buffer_); });
}

void VertexArray::bind_all() const {
	bind();
	bind_buffer();
}
