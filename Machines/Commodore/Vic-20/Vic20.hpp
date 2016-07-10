//
//  Vic20.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Vic20_hpp
#define Vic20_hpp

#include "../../../Processors/6502/CPU6502.hpp"
#include "../../../Storage/Tape/Tape.hpp"
#include "../../../Components/6560/6560.hpp"
#include "../../../Components/6522/6522.hpp"
#include "../1540/C1540.hpp"
#include "../SerialBus.hpp"

#include "../../CRTMachine.hpp"
#include "../../Typer.hpp"

namespace Commodore {
namespace Vic20 {

enum ROMSlot {
	Kernel,
	BASIC,
	Characters,
	Drive
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
	Key9		= key(0, 0x10),		KeyPlus		= key(0, 0x20),		KeyGBP			= key(0, 0x40),		KeyDelete	= key(0, 0x80),

	TerminateSequence = 0,	NotMapped = 0xffff
};

enum JoystickInput {
	Up = 0x04,
	Down = 0x08,
	Left = 0x10,
	Right = 0x80,
	Fire = 0x20
};

class UserPortVIA: public MOS::MOS6522<UserPortVIA>, public MOS::MOS6522IRQDelegate {
	public:
		uint8_t get_port_input(Port port) {
			if(!port) {
				return _portA;	// TODO: bit 6 should be high if there is no tape, low otherwise
			}
			return 0xff;
		}

		void set_control_line_output(Port port, Line line, bool value) {
//			if(port == Port::A && line == Line::Two) {
//				printf("Tape motor %s\n", value ? "on" : "off");
//			}
		}

		void set_serial_line_state(::Commodore::Serial::Line line, bool value) {
//			printf("VIC Serial port line %d: %s\n", line, value ? "on" : "off");
			switch(line) {
				default: break;
				case ::Commodore::Serial::Line::Data: _portA = (_portA & ~0x02) | (value ? 0x02 : 0x00);	break;
				case ::Commodore::Serial::Line::Clock: _portA = (_portA & ~0x01) | (value ? 0x01 : 0x00);	break;
			}
		}

		void set_joystick_state(JoystickInput input, bool value) {
			if(input != JoystickInput::Right)
			{
				_portA = (_portA & ~input) | (value ? 0 : input);
			}
		}

		void set_port_output(Port port, uint8_t value, uint8_t mask) {
			// Line 7 of port A is inverted and output as serial ATN
			if(!port) {
				std::shared_ptr<::Commodore::Serial::Port> serialPort = _serialPort.lock();
				if(serialPort)
					serialPort->set_output(::Commodore::Serial::Line::Attention, (::Commodore::Serial::LineLevel)!(value&0x80));
			}
		}

		using MOS6522IRQDelegate::set_interrupt_status;

		UserPortVIA() : _portA(0xbf) {}

		void set_serial_port(std::shared_ptr<::Commodore::Serial::Port> serialPort) {
			_serialPort = serialPort;
		}

	private:
		uint8_t _portA;
		std::weak_ptr<::Commodore::Serial::Port> _serialPort;
};

class KeyboardVIA: public MOS::MOS6522<KeyboardVIA>, public MOS::MOS6522IRQDelegate {
	public:
		KeyboardVIA() : _portB(0xff) {
			clear_all_keys();
		}

		void set_key_state(Key key, bool isPressed) {
			if(isPressed)
				_columns[key & 7] &= ~(key >> 3);
			else
				_columns[key & 7] |= (key >> 3);
		}

		void clear_all_keys() {
			memset(_columns, 0xff, sizeof(_columns));
		}

		// to satisfy MOS::MOS6522
		uint8_t get_port_input(Port port) {
			if(!port) {
				uint8_t result = 0xff;
				for(int c = 0; c < 8; c++)
				{
					if(!(_activation_mask&(1 << c)))
						result &= _columns[c];
				}
				return result;
			}

			return _portB;
		}

		void set_port_output(Port port, uint8_t value, uint8_t mask) {
			if(port)
				_activation_mask = (value & mask) | (~mask);
		}

		void set_control_line_output(Port port, Line line, bool value) {
			if(line == Line::Two) {
				std::shared_ptr<::Commodore::Serial::Port> serialPort = _serialPort.lock();
				if(serialPort) {
					// CB2 is inverted to become serial data; CA2 is inverted to become serial clock
					if(port == Port::A) {
						serialPort->set_output(::Commodore::Serial::Line::Clock, (::Commodore::Serial::LineLevel)!value);
					} else {
						serialPort->set_output(::Commodore::Serial::Line::Data, (::Commodore::Serial::LineLevel)!value);
					}
				}
			}
		}

		void set_joystick_state(JoystickInput input, bool value) {
			if(input == JoystickInput::Right)
			{
				_portB = (_portB & ~input) | (value ? 0 : input);
			}
		}

		using MOS6522IRQDelegate::set_interrupt_status;

		void set_serial_port(std::shared_ptr<::Commodore::Serial::Port> serialPort) {
			_serialPort = serialPort;
		}

	private:
		uint8_t _portB;
		uint8_t _columns[8];
		uint8_t _activation_mask;
		std::weak_ptr<::Commodore::Serial::Port> _serialPort;
};

class SerialPort : public ::Commodore::Serial::Port {
	public:
		void set_input(::Commodore::Serial::Line line, ::Commodore::Serial::LineLevel level) {
			std::shared_ptr<UserPortVIA> userPortVIA = _userPortVIA.lock();
			if(userPortVIA) userPortVIA->set_serial_line_state(line, (bool)level);
		}

		void set_user_port_via(std::shared_ptr<UserPortVIA> userPortVIA) {
			_userPortVIA = userPortVIA;
		}

	private:
		std::weak_ptr<UserPortVIA> _userPortVIA;
};

class Tape: public Storage::TapePlayer {
	public:
		Tape();

		void set_motor_control(bool enabled);
		void set_tape_output(bool set);
		inline bool get_input() { return _input_level; }

		class Delegate {
			public:
				virtual void tape_did_change_input(Tape *tape) = 0;
		};
		void set_delegate(Delegate *delegate)
		{
			_delegate = delegate;
		}

	private:
		Delegate *_delegate;
		virtual void process_input_pulse(Storage::Tape::Pulse pulse);
		bool _input_level;
};


class Machine:
	public CPU6502::Processor<Machine>,
	public CRTMachine::Machine,
	public MOS::MOS6522IRQDelegate::Delegate,
	public Utility::TypeRecipient,
	public Tape::Delegate {

	public:
		Machine();
		~Machine();

		void set_rom(ROMSlot slot, size_t length, const uint8_t *data);
		void add_prg(size_t length, const uint8_t *data);
		void set_tape(std::shared_ptr<Storage::Tape> tape);
		void set_disc();

		void set_key_state(Key key, bool isPressed) { _keyboardVIA->set_key_state(key, isPressed); }
		void clear_all_keys() { _keyboardVIA->clear_all_keys(); }
		void set_joystick_state(JoystickInput input, bool isPressed) {
			_userPortVIA->set_joystick_state(input, isPressed);
			_keyboardVIA->set_joystick_state(input, isPressed);
		}

		inline void set_use_fast_tape_hack(bool activate) { _use_fast_tape_hack = activate; }

		// to satisfy CPU6502::Processor
		unsigned int perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value);
		void synchronise() { _mos6560->synchronise(); }

		// to satisfy CRTMachine::Machine
		virtual void setup_output(float aspect_ratio);
		virtual void close_output();
		virtual std::shared_ptr<Outputs::CRT::CRT> get_crt() { return _mos6560->get_crt(); }
		virtual std::shared_ptr<Outputs::Speaker> get_speaker() { return _mos6560->get_speaker(); }
		virtual void run_for_cycles(int number_of_cycles) { CPU6502::Processor<Machine>::run_for_cycles(number_of_cycles); }
		virtual double get_clock_rate() { return 1022727; }
		// TODO: or 1108405 for PAL; see http://www.antimon.org/dl/c64/code/stable.txt

		// to satisfy MOS::MOS6522::Delegate
		virtual void mos6522_did_change_interrupt_status(void *mos6522);

		// for Utility::TypeRecipient
		virtual int get_typer_delay();
		virtual int get_typer_frequency();
		virtual bool typer_set_next_character(Utility::Typer *typer, char character, int phase);

		// for Tape::Delegate
		virtual void tape_did_change_input(Tape *tape);

	private:
		uint8_t _characterROM[0x1000];
		uint8_t _basicROM[0x2000];
		uint8_t _kernelROM[0x2000];

		uint8_t *_rom;
		uint16_t _rom_address, _rom_length;

		uint8_t _userBASICMemory[0x0400];
		uint8_t _screenMemory[0x1000];
		uint8_t _colorMemory[0x0400];
		uint8_t _junkMemory[0x0400];

		uint8_t *_videoMemoryMap[16];
		uint8_t *_processorReadMemoryMap[64];
		uint8_t *_processorWriteMemoryMap[64];
		void write_to_map(uint8_t **map, uint8_t *area, uint16_t address, uint16_t length);

		std::unique_ptr<MOS::MOS6560> _mos6560;
		std::shared_ptr<UserPortVIA> _userPortVIA;
		std::shared_ptr<KeyboardVIA> _keyboardVIA;
		std::shared_ptr<SerialPort> _serialPort;
		std::shared_ptr<::Commodore::Serial::Bus> _serialBus;
//		std::shared_ptr<::Commodore::Serial::DebugPort> _debugPort;

		// Tape
		Tape _tape;
		bool _use_fast_tape_hack;

		// Disc
		std::shared_ptr<::Commodore::C1540::Machine> _c1540;
};

}
}

#endif /* Vic20_hpp */
