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
#include "../../Components/6560/6560.hpp"
#include "../CRTMachine.hpp"

namespace Vic20 {

enum ROMSlot {
	ROMSlotKernel,
	ROMSlotBASIC,
	ROMSlotCharacters,
};

class Machine: public CPU6502::Processor<Machine>, public CRTMachine::Machine {
	public:
		Machine();

		void set_rom(ROMSlot slot, size_t length, const uint8_t *data);
		void add_prg(size_t length, const uint8_t *data);

		// to satisfy CPU6502::Processor
		unsigned int perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value);
		void synchronise() {}

		// to satisfy CRTMachine::Machine
		virtual void setup_output(float aspect_ratio);
		virtual void close_output() {}
		virtual Outputs::CRT::CRT *get_crt() { return _mos6560->get_crt(); }
		virtual Outputs::Speaker *get_speaker() { return nullptr; }	// TODO
		virtual void run_for_cycles(int number_of_cycles) { CPU6502::Processor<Machine>::run_for_cycles(number_of_cycles); }

	private:
		uint8_t _characterROM[0x1000];
		uint8_t _basicROM[0x2000];
		uint8_t _kernelROM[0x2000];

		uint8_t _userBASICMemory[0x0400];
		uint8_t _screenMemory[0x1000];
		uint8_t _colorMemory[0x0200];

		inline uint8_t *ram_pointer(uint16_t address) {
			if(address < sizeof(_userBASICMemory)) return &_userBASICMemory[address];
			if(address >= 0x1000 && address < 0x2000) return &_screenMemory[address&0x0fff];
			if(address >= 0x9400 && address < 0x9600) return &_colorMemory[0x01ff];
			return nullptr;
		}

		inline uint8_t read_memory(uint16_t address) {
			uint8_t *ram = ram_pointer(address);
			if(ram) return *ram;
			else if(address >= 0x8000 && address < 0x9000) return _characterROM[address&0x0fff];
			else if(address >= 0xc000 && address < 0xe000) return _basicROM[address&0x1fff];
			else if(address >= 0xe000) return _kernelROM[address&0x1fff];
			return 0xff;
		}

		std::unique_ptr<MOS::MOS6560> _mos6560;
};

}

#endif /* Vic20_hpp */
