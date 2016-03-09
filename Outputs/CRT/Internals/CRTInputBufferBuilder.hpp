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

namespace Outputs {
namespace CRT {

struct CRTInputBufferBuilder {
	CRTInputBufferBuilder(unsigned int number_of_buffers, va_list buffer_sizes);
	~CRTInputBufferBuilder();

	void allocate_write_area(size_t required_length);
	void reduce_previous_allocation_to(size_t actual_length);
	uint8_t *get_write_target_for_buffer(int buffer);

	// a pointer to the section of content buffer currently being
	// returned and to where the next section will begin
	uint16_t _next_write_x_position, _next_write_y_position;
	uint16_t _write_x_position, _write_y_position;
	size_t _write_target_pointer;
	size_t _last_allocation_amount;

	struct Buffer {
		uint8_t *data;
		size_t bytes_per_pixel;
	} *buffers;
	unsigned int number_of_buffers;

	// Storage for the amount of buffer uploaded so far; initialised correctly by the buffer
	// builder but otherwise entrusted to the CRT to update.
	unsigned int last_uploaded_line;
};

}
}

#endif /* CRTInputBufferBuilder_hpp */
