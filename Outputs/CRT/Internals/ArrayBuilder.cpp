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
	output_(output_size),
	input_(input_size)
{}

ArrayBuilder::Buffer::Buffer(size_t size) :
	allocated_data(0), completed_data(0)
{
	glGenBuffers(1, &buffer);
	glBindBuffer(GL_ARRAY_BUFFER, buffer);
	glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)size, NULL, GL_STREAM_DRAW);
	data.resize(size);
}

ArrayBuilder::Buffer::~Buffer()
{
	glDeleteBuffers(1, &buffer);
}

uint8_t *ArrayBuilder::get_input_storage(size_t size)
{
	return input_.get_storage(size);
}

uint8_t *ArrayBuilder::get_output_storage(size_t size)
{
	return output_.get_storage(size);
}

void ArrayBuilder::flush()
{
	// TODO: don't flush anything if either buffer was exhausted
	buffer_mutex_.lock();
	input_.flush();
	output_.flush();
	buffer_mutex_.unlock();
}

void ArrayBuilder::bind_input()
{
	input_.bind();
}

void ArrayBuilder::bind_output()
{
	output_.bind();
}

ArrayBuilder::Submission ArrayBuilder::submit()
{
	ArrayBuilder::Submission submission;

	buffer_mutex_.lock();
	submission.input_size = input_.submit();
	submission.output_size = output_.submit();
	buffer_mutex_.unlock();
	// TODO: if either buffer was exhausted, reset both

	return submission;
}

uint8_t *ArrayBuilder::Buffer::get_storage(size_t size)
{
	if(allocated_data + size > data.size()) return nullptr;
	uint8_t *pointer = &data[allocated_data];
	vended_pointer = allocated_data;
	allocated_data += size;
	return pointer;
}

void ArrayBuilder::Buffer::flush()
{
	// Ordinarily this just requires the completed data count to be bumped up
	// to the current allocated data value. However if the amount of allocated
	// data is now less than the completed data then that implies a submission
	// occurred while the pointer previously vended by get_storage was in use.
	// So copy whatever is danging back to the start and make amends.
	if(completed_data < allocated_data)
		completed_data = allocated_data;
	else
	{
		completed_data = allocated_data - vended_pointer;
		allocated_data = completed_data;
		memcpy(data.data(), &data[vended_pointer], completed_data);
	}
}

size_t ArrayBuilder::Buffer::submit()
{
	size_t length = completed_data;

	glBindBuffer(GL_ARRAY_BUFFER, buffer);
	uint8_t *destination = (uint8_t *)glMapBufferRange(GL_ARRAY_BUFFER, 0, (GLsizeiptr)length, GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);
	memcpy(destination, data.data(), length);
	glFlushMappedBufferRange(GL_ARRAY_BUFFER, 0, (GLsizeiptr)length);
	glUnmapBuffer(GL_ARRAY_BUFFER);

	completed_data = 0;
	return length;
}

void ArrayBuilder::Buffer::bind()
{
	glBindBuffer(GL_ARRAY_BUFFER, buffer);
}

void ArrayBuilder::Buffer::reset()
{
	completed_data = allocated_data = 0;
}
