//
//  A26.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 28/08/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Storage_Cartridge_A26_hpp
#define Storage_Cartridge_A26_hpp

#include "../Cartridge.hpp"

namespace Storage {
namespace Cartridge {

class A26 : public Cartridge {
	public:
		A26(const char *file_name);

		enum {
			ErrorNotAcornROM
		};
};

}
}


#endif /* A26_hpp */
