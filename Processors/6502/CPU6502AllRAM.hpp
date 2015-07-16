//
//  CPU6502AllRAM.hpp
//  ElectrEm
//
//  Created by Thomas Harte on 13/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#ifndef CPU6502AllRAM_cpp
#define CPU6502AllRAM_cpp

#include "CPU6502.hpp"

namespace CPU6502 {

class AllRAMProcessor: public Processor<AllRAMProcessor> {

	public:

		AllRAMProcessor();

		void perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value);

		void set_data_at_address(uint16_t startAddress, size_t length, const uint8_t *data);

	private:
		uint8_t _memory[65536];
};

}

#endif /* CPU6502AllRAM_cpp */
