//
//  CPU6502AllRAM.hpp
//  CLK
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

		int perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value);

		void set_data_at_address(uint16_t startAddress, size_t length, const uint8_t *data);
		uint32_t get_timestamp();

	private:
		uint8_t _memory[65536];
		uint32_t _timestamp;
};

}

#endif /* CPU6502AllRAM_cpp */
