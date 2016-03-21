//
//  CRTInputBufferBuilder.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/03/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef CRTInputBufferBuilder_hpp
#define CRTInputBufferBuilder_hpp

#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include "CRTConstants.hpp"
#include "OpenGL.hpp"

namespace Outputs {
namespace CRT {

struct CRTInputBufferBuilder {
	CRTInputBufferBuilder(size_t bytes_per_pixel);
	~CRTInputBufferBuilder();

	void allocate_write_area(size_t required_length);
	void reduce_previous_allocation_to(size_t actual_length, uint8_t *buffer);

	// a pointer to the section of content buffer currently being
	// returned and to where the next section will begin
	uint16_t _next_write_x_position, _next_write_y_position;
	uint16_t _write_x_position, _write_y_position;
	size_t _write_target_pointer;
	size_t _last_allocation_amount;
	size_t bytes_per_pixel;

	// Storage for the amount of buffer uploaded so far; initialised correctly by the buffer
	// builder but otherwise entrusted to the CRT to update.
	unsigned int last_uploaded_line;

	GLsync _wraparound_sync;

	inline void move_to_new_line()
	{
		_next_write_x_position = 0;
		_next_write_y_position = (_next_write_y_position+1)%InputBufferBuilderHeight;
//		if(!_next_write_y_position)
//		{
//			glClientWaitSync(_wraparound_sync, 0, ~(GLuint64)0);
//			glDeleteSync(_wraparound_sync);
//			_wraparound_sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
//		}
	}

	inline uint8_t *get_write_target(uint8_t *buffer)
	{
		return &buffer[_write_target_pointer * bytes_per_pixel];
	}
};

}
}

#endif /* CRTInputBufferBuilder_hpp */
