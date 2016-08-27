//
//  PRG.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 27/08/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Storage_Cartridge_PRG_hpp
#define Storage_Cartridge_PRG_hpp

#include <vector>
#include "../Cartridge.hpp"

namespace Storage {
namespace Cartridge {

class PRG : public Cartridge {
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
