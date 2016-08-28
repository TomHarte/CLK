//
//  AcornROM.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 28/08/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Storage_Cartridge_AcornROM_hpp
#define Storage_Cartridge_AcornROM_hpp

#include "../Cartridge.hpp"

namespace Storage {
namespace Cartridge {

class AcornROM : public Cartridge {
	public:
		AcornROM(const char *file_name);

		enum {
			ErrorNotAcornROM
		};
};

}
}

#endif /* AcornROM_hpp */
