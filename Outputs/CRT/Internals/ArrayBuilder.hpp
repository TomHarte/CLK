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

#include "OpenGL.hpp"

namespace Outputs {
namespace CRT {

class ArrayBuilder {
	public:
		ArrayBuilder();
		virtual ~ArrayBuilder();

		uint8_t *get_input_storage(size_t size);
		uint8_t *get_output_storage(size_t size);
		void flush_storage();

		void bind_input();
		void bind_output();
		void submit();

	private:
		struct {
			std::vector<uint8_t> data;
			size_t allocated_data, completed_data;
			GLuint buffer;
		} output_, input_;
		std::mutex buffer_mutex_;
};

}
}

#endif /* ArrayBuilder_hpp */
