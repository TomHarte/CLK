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

bool ArrayBuilder::is_full()
{
	bool was_full;
	buffer_mutex_.lock();
	was_full = is_full_;
	buffer_mutex_.unlock();
	return was_full;
}

uint8_t *ArrayBuilder::get_input_storage(size_t size)
{
	return get_storage(size, input_);
}

uint8_t *ArrayBuilder::reget_input_storage(size_t &size)
{
	return input_.reget_storage(size);
}

uint8_t *ArrayBuilder::get_output_storage(size_t size)
{
	return get_storage(size, output_);
}

uint8_t *ArrayBuilder::reget_output_storage(size_t &size)
{
	return output_.reget_storage(size);
}

void ArrayBuilder::flush()
{
	buffer_mutex_.lock();
	if(!is_full_)
	{
		input_.flush();
		output_.flush();
	}
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
	if(is_full_)
	{
		is_full_ = false;
		input_.reset();
		output_.reset();
	}
	buffer_mutex_.unlock();

	return submission;
}

ArrayBuilder::Buffer::Buffer(size_t size) :
	allocated_data(0), flushed_data(0), submitted_data(0), is_full(false)
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

uint8_t *ArrayBuilder::get_storage(size_t size, Buffer &buffer)
{
	buffer_mutex_.lock();
	uint8_t *pointer = buffer.get_storage(size);
	if(!pointer) is_full_ = true;
	buffer_mutex_.unlock();
	return pointer;
}

uint8_t *ArrayBuilder::Buffer::get_storage(size_t size)
{
	if(is_full || allocated_data + size > data.size())
	{
		is_full = true;
		return nullptr;
	}
	uint8_t *pointer = &data[allocated_data];
	allocated_data += size;
	return pointer;
}

uint8_t *ArrayBuilder::Buffer::reget_storage(size_t &size)
{
	if(is_full)
	{
		return nullptr;
	}
	size = allocated_data - flushed_data;
	return &data[flushed_data];
}

void ArrayBuilder::Buffer::flush()
{
	if(allocated_data > submitted_data)
	{
		flushed_data = allocated_data;
		return;
	}

	if(submitted_data)
	{
		memcpy(data.data(), &data[flushed_data], allocated_data - flushed_data);
		allocated_data -= flushed_data;
		flushed_data = allocated_data;
		submitted_data = 0;
	}
	else
	{
		allocated_data = 0;
		flushed_data = 0;
		submitted_data = 0;
	}
}

size_t ArrayBuilder::Buffer::submit()
{
	size_t length = flushed_data;

	glBindBuffer(GL_ARRAY_BUFFER, buffer);
	uint8_t *destination = (uint8_t *)glMapBufferRange(GL_ARRAY_BUFFER, 0, (GLsizeiptr)length, GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);
	memcpy(destination, data.data(), length);
	glFlushMappedBufferRange(GL_ARRAY_BUFFER, 0, (GLsizeiptr)length);
	glUnmapBuffer(GL_ARRAY_BUFFER);

	submitted_data = flushed_data;
	return length;
}

void ArrayBuilder::Buffer::bind()
{
	glBindBuffer(GL_ARRAY_BUFFER, buffer);
}

void ArrayBuilder::Buffer::reset()
{
	allocated_data = 0;
	flushed_data = 0;
	submitted_data = 0;
	is_full = false;
}
