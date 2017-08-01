//
//  AmstradCPC.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 30/07/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef AmstradCPC_hpp
#define AmstradCPC_hpp

#include "../ConfigurationTarget.hpp"
#include "../CRTMachine.hpp"

namespace AmstradCPC {

enum ROMType: uint8_t {
	OS464, OS664, OS6128,
	BASIC464, BASIC664, BASIC6128,
	AMSDOS
};

class Machine:
	public CRTMachine::Machine,
	public ConfigurationTarget::Machine {
	public:
		static Machine *AmstradCPC();
		virtual void set_rom(ROMType type, std::vector<uint8_t> data) = 0;
};

}

#endif /* AmstradCPC_hpp */
