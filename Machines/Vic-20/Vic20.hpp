//
//  Vic20.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Vic20_hpp
#define Vic20_hpp

#include "../../Processors/6502/CPU6502.hpp"
#include "../CRTMachine.hpp"

namespace Vic20 {

enum ROMSlot {
	ROMSlotKernel,
	ROMSlotBASIC,
	ROMSlotCharacters,
};

class Machine: public CPU6502::Processor<Machine>, public CRTMachine::Machine {
	public:

		void set_rom(ROMSlot slot, size_t length, const uint8_t *data);
		void add_prg(size_t length, const uint8_t *data);

		// to satisfy CPU6502::Processor
		unsigned int perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value);
		void synchronise() {}

		// to satisfy CRTMachine::Machine
		virtual void setup_output(float aspect_ratio) {}
		virtual void close_output() {}
		virtual Outputs::CRT::CRT *get_crt() { return nullptr; }	// TODO
		virtual Outputs::Speaker *get_speaker() { return nullptr; }	// TODO
		virtual void run_for_cycles(int number_of_cycles) { CPU6502::Processor<Machine>::run_for_cycles(number_of_cycles); }
};

}

#endif /* Vic20_hpp */
