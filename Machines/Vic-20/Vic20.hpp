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
#include "../../Components/6522/6522.hpp"
#include "../CRTMachine.hpp"

namespace Vic20 {

enum ROMSlot {
	ROMSlotKernel,
	ROMSlotBASIC,
	ROMSlotCharacters,
};


#define key(line, mask) (((mask) << 3) | (line))

enum Key: uint16_t {
	Key2		= key(7, 0x01),		Key4		= key(7, 0x02),		Key6			= key(7, 0x04),		Key8		= key(7, 0x08),
	Key0		= key(7, 0x10),		KeyDash		= key(7, 0x20),		KeyHome			= key(7, 0x40),		KeyF7		= key(7, 0x80),
	KeyQ		= key(6, 0x01),		KeyE		= key(6, 0x02),		KeyT			= key(6, 0x04),		KeyU		= key(6, 0x08),
	KeyO		= key(6, 0x10),		KeyAt		= key(6, 0x20),		KeyUp			= key(6, 0x40),		KeyF5		= key(6, 0x80),
	KeyCBM		= key(5, 0x01),		KeyS		= key(5, 0x02),		KeyF			= key(5, 0x04),		KeyH		= key(5, 0x08),
	KeyK		= key(5, 0x10),		KeyColon	= key(5, 0x20),		KeyEquals		= key(5, 0x40),		KeyF3		= key(5, 0x80),
	KeySpace	= key(4, 0x01),		KeyZ		= key(4, 0x02),		KeyC			= key(4, 0x04),		KeyB		= key(4, 0x08),
	KeyM		= key(4, 0x10),		KeyFullStop	= key(4, 0x20),		KeyRShift		= key(4, 0x40),		KeyF1		= key(4, 0x80),
	KeyRunStop	= key(3, 0x01),		KeyLShift	= key(3, 0x02),		KeyX			= key(3, 0x04),		KeyV		= key(3, 0x08),
	KeyN		= key(3, 0x10),		KeyComma	= key(3, 0x20),		KeySlash		= key(3, 0x40),		KeyDown		= key(3, 0x80),
	KeyControl	= key(2, 0x01),		KeyA		= key(2, 0x02),		KeyD			= key(2, 0x04),		KeyG		= key(2, 0x08),
	KeyJ		= key(2, 0x10),		KeyL		= key(2, 0x20),		KeySemicolon	= key(2, 0x40),		KeyRight	= key(2, 0x80),
	KeyLeft		= key(1, 0x01),		KeyW		= key(1, 0x02),		KeyR			= key(1, 0x04),		KeyY		= key(1, 0x08),
	KeyI		= key(1, 0x10),		KeyP		= key(1, 0x20),		KeyAsterisk		= key(1, 0x40),		KeyReturn	= key(1, 0x80),
	Key1		= key(0, 0x01),		Key3		= key(0, 0x02),		Key5			= key(0, 0x04),		Key7		= key(0, 0x08),
	Key9		= key(0, 0x10),		KeyPlus		= key(0, 0x20),		KeyGBP			= key(0, 0x40),		KeyDel		= key(0, 0x80),
};

class UserPortVIA: public MOS::MOS6522<UserPortVIA> {
};

class KeyboardVIA: public MOS::MOS6522<KeyboardVIA> {
	public:
		void set_key_state(Key key, bool isPressed) {
			if(isPressed)
				_columns[key & 7] &= ~(key >> 3);
			else
				_columns[key & 7] |= (key >> 3);
		}

		// to satisfy MOS::MOS6522
		uint8_t get_port_input(int port) {
			if(!port) {
				uint8_t result = 0xff;
				for(int c = 0; c < 8; c++)
				{
					if(!(_activation_mask&(1 << c)))
						result &= _columns[c];
				}
//				if(_activation_mask)
//					printf("%02x => %02x\n", _activation_mask, result);
				return result;
			}

			return 0xff;
		}

		void set_port_output(int port, uint8_t value) {
			if(port)
				_activation_mask = value;
//			printf("%d <= %02x\n", port, value);
		}

		KeyboardVIA() : _columns{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff} {}
	private:
		uint8_t _columns[8];
		uint8_t _activation_mask;
};

class Machine: public CPU6502::Processor<Machine>, public CRTMachine::Machine, public MOS::MOS6522Delegate {
	public:
		Machine();

		void set_rom(ROMSlot slot, size_t length, const uint8_t *data);
		void add_prg(size_t length, const uint8_t *data);
		void set_key_state(Key key, bool isPressed) { _keyboardVIA->set_key_state(key, isPressed); }

		// to satisfy CPU6502::Processor
		unsigned int perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value);
		void synchronise() {}

		// to satisfy CRTMachine::Machine
		virtual void setup_output(float aspect_ratio);
		virtual void close_output() {}
		virtual Outputs::CRT::CRT *get_crt() { return _mos6560->get_crt(); }
		virtual Outputs::Speaker *get_speaker() { return nullptr; }	// TODO
		virtual void run_for_cycles(int number_of_cycles) { CPU6502::Processor<Machine>::run_for_cycles(number_of_cycles); }

		// to satisfy MOS::MOS6522::Delegate
		virtual void mos6522_did_change_interrupt_status(void *mos6522);

	private:
		uint8_t _characterROM[0x1000];
		uint8_t _basicROM[0x2000];
		uint8_t _kernelROM[0x2000];

		uint8_t _userBASICMemory[0x0400];
		uint8_t _screenMemory[0x1000];
		uint8_t _colorMemory[0x0400];

		inline uint8_t *ram_pointer(uint16_t address) {
			if(address < sizeof(_userBASICMemory)) return &_userBASICMemory[address];
			if(address >= 0x1000 && address < 0x2000) return &_screenMemory[address&0x0fff];
			if(address >= 0x9400 && address < 0x9800) return &_colorMemory[0x03ff];	// TODO: make this 4-bit
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
		std::unique_ptr<UserPortVIA> _userPortVIA;
		std::unique_ptr<KeyboardVIA> _keyboardVIA;
};

}

#endif /* Vic20_hpp */
