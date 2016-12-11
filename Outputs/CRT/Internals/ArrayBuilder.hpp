//
//  ArrayBuilder.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/11/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef ArrayBuilder_hpp
#define ArrayBuilder_hpp

#include <functional>
#include <memory>
#include <vector>

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

		/// Attempts to add @c size bytes to the input set.
		/// @returns a pointer to the allocated area if allocation was possible; @c nullptr otherwise.
		uint8_t *get_input_storage(size_t size);

		/// Attempts to add @c size bytes to the output set.
		/// @returns a pointer to the allocated area if allocation was possible; @c nullptr otherwise.
		uint8_t *get_output_storage(size_t size);

		/// @returns @c true if either of the input or output storage areas is currently exhausted; @c false otherwise.
		bool is_full();

		/// If neither input nor output was exhausted since the last flush, atomically commits both input and output
		/// up to the currently allocated size for use upon the next @c submit, giving the supplied function a
		/// chance to perform last-minute processing. Otherwise acts as a no-op.
		void flush(const std::function<void(uint8_t *input, size_t input_size, uint8_t *output, size_t output_size)> &);

		/// Binds the input array to GL_ARRAY_BUFFER.
		void bind_input();

		/// Binds the output array to GL_ARRAY_BUFFER.
		void bind_output();

		struct Submission {
			size_t input_size, output_size;
		};

		/// Submits all flushed input and output data to the corresponding arrays.
		/// @returns A @c Submission record, indicating how much data of each type was submitted.
		Submission submit();

	private:
		class Buffer {
			public:
				Buffer(size_t size, std::function<void(bool is_input, uint8_t *, size_t)> submission_function);
				~Buffer();

				uint8_t *get_storage(size_t size);
				uint8_t *get_unflushed(size_t &size);

				void flush();
				size_t submit(bool is_input);
				void bind();
				void reset();

			private:
				bool is_full;
				GLuint buffer;
				std::function<void(bool is_input, uint8_t *, size_t)> submission_function_;
				std::vector<uint8_t> data;
				size_t allocated_data;
				size_t flushed_data;
				size_t submitted_data;
		} output_, input_;
		uint8_t *get_storage(size_t size, Buffer &buffer);

		bool is_full_;
};

}
}

#endif /* ArrayBuilder_hpp */
