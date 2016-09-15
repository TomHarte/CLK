//
//  PRG.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 27/08/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Storage_Cartridge_PRG_hpp
#define Storage_Cartridge_PRG_hpp

#include "../Cartridge.hpp"

namespace Storage {
namespace Cartridge {

class PRG : public Cartridge {
	public:
		PRG(const char *file_name);

		enum {
			ErrorNotROM
		};
};

}
}

#endif /* PRG_hpp */
