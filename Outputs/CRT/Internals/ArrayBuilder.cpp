//
//  ArrayBuilder.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/11/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "ArrayBuilder.hpp"

using namespace Outputs::CRT;

ArrayBuilder::ArrayBuilder(size_t input_size, size_t output_size) :
		output_(output_size, nullptr),
		input_(input_size, nullptr) {}

ArrayBuilder::ArrayBuilder(size_t input_size, size_t output_size, std::function<void(bool is_input, uint8_t *, size_t)> submission_function) :
		output_(output_size, submission_function),
		input_(input_size, submission_function) {}

bool ArrayBuilder::is_full() {
	bool was_full;
	was_full = is_full_;
	return was_full;
}

uint8_t *ArrayBuilder::get_input_storage(size_t size) {
	return get_storage(size, input_);
}

uint8_t *ArrayBuilder::get_output_storage(size_t size) {
	return get_storage(size, output_);
}

void ArrayBuilder::flush(const std::function<void(uint8_t *input, size_t input_size, uint8_t *output, size_t output_size)> &function) {
	if(!is_full_) {
		size_t input_size = 0, output_size = 0;
		uint8_t *input = input_.get_unflushed(input_size);
		uint8_t *output = output_.get_unflushed(output_size);
		function(input, input_size, output, output_size);

		input_.flush();
		output_.flush();
	}
}

void ArrayBuilder::bind_input() {
	input_.bind();
}

void ArrayBuilder::bind_output() {
	output_.bind();
}

ArrayBuilder::Submission ArrayBuilder::submit() {
	ArrayBuilder::Submission submission;

	submission.input_size = input_.submit(true);
	submission.output_size = output_.submit(false);
	if(is_full_) {
		is_full_ = false;
		input_.reset();
		output_.reset();
	}

	return submission;
}

ArrayBuilder::Buffer::Buffer(size_t size, std::function<void(bool is_input, uint8_t *, size_t)> submission_function) :
		submission_function_(submission_function) {
	if(!submission_function_) {
		glGenBuffers(1, &buffer);
		glBindBuffer(GL_ARRAY_BUFFER, buffer);
		glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)size, NULL, GL_STREAM_DRAW);
	}
	data.resize(size);
}

ArrayBuilder::Buffer::~Buffer() {
	if(!submission_function_)
		glDeleteBuffers(1, &buffer);
}

uint8_t *ArrayBuilder::get_storage(size_t size, Buffer &buffer) {
	uint8_t *pointer = buffer.get_storage(size);
	if(!pointer) is_full_ = true;
	return pointer;
}

uint8_t *ArrayBuilder::Buffer::get_storage(size_t size) {
	if(is_full || allocated_data + size > data.size()) {
		is_full = true;
		return nullptr;
	}
	uint8_t *pointer = &data[allocated_data];
	allocated_data += size;
	return pointer;
}

uint8_t *ArrayBuilder::Buffer::get_unflushed(size_t &size) {
	if(is_full) {
		return nullptr;
	}
	size = allocated_data - flushed_data;
	return &data[flushed_data];
}

void ArrayBuilder::Buffer::flush() {
	if(submitted_data) {
		memmove(data.data(), &data[submitted_data], allocated_data - submitted_data);
		allocated_data -= submitted_data;
		flushed_data -= submitted_data;
		submitted_data = 0;
	}

	flushed_data = allocated_data;
}

size_t ArrayBuilder::Buffer::submit(bool is_input) {
	size_t length = flushed_data;
	if(submission_function_) {
		submission_function_(is_input, data.data(), length);
	} else {
		glBindBuffer(GL_ARRAY_BUFFER, buffer);
		uint8_t *destination = (uint8_t *)glMapBufferRange(GL_ARRAY_BUFFER, 0, (GLsizeiptr)length, GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);
		memcpy(destination, data.data(), length);
		glFlushMappedBufferRange(GL_ARRAY_BUFFER, 0, (GLsizeiptr)length);
		glUnmapBuffer(GL_ARRAY_BUFFER);
	}
	submitted_data = flushed_data;
	return length;
}

void ArrayBuilder::Buffer::bind() {
	glBindBuffer(GL_ARRAY_BUFFER, buffer);
}

void ArrayBuilder::Buffer::reset() {
	is_full = false;
	allocated_data = 0;
	flushed_data = 0;
	submitted_data = 0;
}
