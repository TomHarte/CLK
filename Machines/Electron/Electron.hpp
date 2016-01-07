//
//  Electron.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Electron_hpp
#define Electron_hpp

#include "../../Processors/6502/CPU6502.hpp"
#include "../../Outputs/CRT.hpp"
#include <stdint.h>
#include "Atari2600Inputs.h"

namespace Electron {

enum ROMSlot: int {
	ROMTypeBASIC = 12,
	ROMTypeOS = 16,
};

class Machine: public CPU6502::Processor<Machine> {

	public:

		Machine();
		~Machine();

		unsigned int perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value);

		void set_rom(ROMSlot slot, size_t length, const uint8_t *data);

	private:
		uint8_t os[16384], basic[16384], ram[32768];
};

}

#endif /* Electron_hpp */
