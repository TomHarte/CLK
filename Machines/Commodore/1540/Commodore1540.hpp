//
//  Commodore1540.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Commodore1540_hpp
#define Commodore1540_hpp

#include "../../../Processors/6502/CPU6502.hpp"
#include "../../../Components/6522/6522.hpp"

namespace Commodore {
namespace C1540 {

class Machine:
	public CPU6502::Processor<Machine> {

	public:
		unsigned int perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value);

		void set_rom(uint8_t *rom);

	private:
		uint8_t _ram[0x800];
		uint8_t _rom[0x4000];
};

}
}

#endif /* Commodore1540_hpp */
