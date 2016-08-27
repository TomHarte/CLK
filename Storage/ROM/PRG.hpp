//
//  PRG.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 27/08/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef PRG_hpp
#define PRG_hpp

#include <vector>
#include "ROM.hpp"

namespace Storage {
namespace ROM {

class PRG : public ROM {
	public:
		PRG(const char *file_name);
		~PRG();

		enum {
			ErrorNotROM
		};

	private:
		uint8_t *_contents;
		uint16_t _size;
};

}
}

#endif /* PRG_hpp */
