//
//  Shader.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/02/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Shader_hpp
#define Shader_hpp

namespace OpenGL {
  class Shader {
	public:
		Shader(const char *vertex_shader, const char *fragment_shader);
		~Shader();

		void bind();
  };
}

#endif /* Shader_hpp */
