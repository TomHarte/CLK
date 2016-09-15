//
//  BinaryDump.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 28/08/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Storage_Cartridge_BinaryDump_hpp
#define Storage_Cartridge_BinaryDump_hpp

#include "../Cartridge.hpp"

namespace Storage {
namespace Cartridge {

class BinaryDump : public Cartridge {
	public:
		BinaryDump(const char *file_name);

		enum {
			ErrorNotAccessible
		};
};

}
}

#endif /* AcornROM_hpp */
