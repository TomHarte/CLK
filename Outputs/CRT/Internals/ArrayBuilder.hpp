//
//  ArrayBuilder.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/11/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef ArrayBuilder_hpp
#define ArrayBuilder_hpp

#include <vector>
#include <mutex>
#include <memory>

#include "OpenGL.hpp"

namespace Outputs {
namespace CRT {

/*!
	Owns two array buffers, an 'input' and an 'output' and vends pointers to allow an owner to write provisional data into those
	plus a flush function to lock provisional data into place. Also supplies a submit method to transfer all currently locked
	data to the GPU and bind_input/output methods to bind the internal buffers.

	It is safe for one thread to communicate via the get_*_storage and flush inputs asynchronously from another that is making
	use of the bind and submit outputs.
*/
class ArrayBuilder {
	public:
		/// Creates an instance of ArrayBuilder with @c output_size bytes of storage for the output buffer and
		/// @c input_size bytes of storage for the input buffer.
		ArrayBuilder(size_t input_size, size_t output_size);

		/// Creates an instance of ArrayBuilder with @c output_size bytes of storage for the output buffer and
		/// @c input_size bytes of storage for the input buffer that, rather than using OpenGL, will submit data
		/// to the @c submission_function. [Teleological: this is provided as a testing hook.]
		ArrayBuilder(size_t input_size, size_t output_size, std::function<void(bool is_input, uint8_t *, size_t)> submission_function);

		/// Attempts to add @c size bytes
		uint8_t *get_input_storage(size_t size);
		uint8_t *reget_input_storage(size_t &size);

		uint8_t *get_output_storage(size_t size);
		uint8_t *reget_output_storage(size_t &size);

		bool is_full();
		void flush();

		void bind_input();
		void bind_output();

		struct Submission {
			size_t input_size, output_size;
		};
		Submission submit();

	private:
		struct Buffer {
			Buffer(size_t size, std::function<void(bool is_input, uint8_t *, size_t)> submission_function);
			~Buffer();

			std::vector<uint8_t> data;
			size_t allocated_data;
			size_t flushed_data;
			size_t submitted_data;
			bool is_full, was_reset;
			GLuint buffer;

			uint8_t *get_storage(size_t size);
			uint8_t *reget_storage(size_t &size);

			void flush();
			size_t submit(bool is_input);
			void bind();
			void reset();
			std::function<void(bool is_input, uint8_t *, size_t)> submission_function_;
		} output_, input_;
		uint8_t *get_storage(size_t size, Buffer &buffer);

		std::mutex buffer_mutex_;
		bool is_full_;
		;
};

}
}

#endif /* ArrayBuilder_hpp */
