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
#include "../../Outputs/Speaker.hpp"
#include <stdint.h>
#include "Atari2600Inputs.h"

namespace Electron {

enum ROMSlot: uint8_t {
	ROMSlot0 = 0,
	ROMSlot1,	ROMSlot2,	ROMSlot3,
	ROMSlot4,	ROMSlot5,	ROMSlot6,	ROMSlot7,

	ROMSlotKeyboard = 8,	ROMSlot9,
	ROMSlotBASIC = 10,		ROMSlot11,

	ROMSlot12,	ROMSlot13,	ROMSlot14,	ROMSlot15,

	ROMSlotOS
};

enum Interrupt: uint8_t {
	InterruptDisplayEnd			= 0x04,
	InterruptRealTimeClock		= 0x08,
	InterruptTransmitDataEmpty	= 0x10,
	InterruptReceiveDataFull	= 0x20,
	InterruptHighToneDetect		= 0x40
};

enum Key: uint16_t {
	KeySpace		= 0x0000 | 0x08,										KeyCopy			= 0x0000 | 0x02,	KeyRight		= 0x0000 | 0x01,
	KeyDelete		= 0x0010 | 0x08,	KeyReturn		= 0x0010 | 0x04,	KeyDown			= 0x0010 | 0x02,	KeyLeft			= 0x0010 | 0x01,
										KeyColon		= 0x0020 | 0x04,	KeyUp			= 0x0020 | 0x02,	KeyMinus		= 0x0020 | 0x01,
	KeySlash		= 0x0030 | 0x08,	KeySemiColon	= 0x0030 | 0x04,	KeyP			= 0x0030 | 0x02,	Key0			= 0x0030 | 0x01,
	KeyFullStop		= 0x0040 | 0x08,	KeyL			= 0x0040 | 0x04,	KeyO			= 0x0040 | 0x02,	Key9			= 0x0040 | 0x01,
	KeyComma		= 0x0050 | 0x08,	KeyK			= 0x0050 | 0x04,	KeyI			= 0x0050 | 0x02,	Key8			= 0x0050 | 0x01,
	KeyM			= 0x0060 | 0x08,	KeyJ			= 0x0060 | 0x04,	KeyU			= 0x0060 | 0x02,	Key7			= 0x0060 | 0x01,
	KeyN			= 0x0070 | 0x08,	KeyH			= 0x0070 | 0x04,	KeyY			= 0x0070 | 0x02,	Key6			= 0x0070 | 0x01,
	KeyB			= 0x0080 | 0x08,	KeyG			= 0x0080 | 0x04,	KeyT			= 0x0080 | 0x02,	Key5			= 0x0080 | 0x01,
	KeyV			= 0x0090 | 0x08,	KeyF			= 0x0090 | 0x04,	KeyR			= 0x0090 | 0x02,	Key4			= 0x0090 | 0x01,
	KeyC			= 0x00a0 | 0x08,	KeyD			= 0x00a0 | 0x04,	KeyE			= 0x00a0 | 0x02,	Key3			= 0x00a0 | 0x01,
	KeyX			= 0x00b0 | 0x08,	KeyS			= 0x00b0 | 0x04,	KeyW			= 0x00b0 | 0x02,	Key2			= 0x00b0 | 0x01,
	KeyZ			= 0x00c0 | 0x08,	KeyA			= 0x00c0 | 0x04,	KeyQ			= 0x00c0 | 0x02,	Key1			= 0x00c0 | 0x01,
	KeyShift		= 0x00d0 | 0x08,	KeyControl		= 0x00d0 | 0x04,	KeyFunc			= 0x00d0 | 0x02,	KeyEscape		= 0x00d0 | 0x01,

	KeyBreak		= 0xffff
};

class Machine: public CPU6502::Processor<Machine> {

	public:

		Machine();
		~Machine();

		unsigned int perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value);

		void set_rom(ROMSlot slot, size_t length, const uint8_t *data);
		void set_key_state(Key key, bool isPressed);

		Outputs::CRT *get_crt() { return _crt; }
		Outputs::Speaker *get_speaker() { return &_speaker; }
		const char *get_signal_decoder();

	private:
		uint8_t _roms[16][16384];
		uint8_t _os[16384], _ram[32768];
		uint8_t _interruptStatus, _interruptControl;
		uint8_t _palette[16];
		uint8_t _keyStates[14];
		ROMSlot _activeRom;
		uint8_t _screenMode;
		uint16_t _screenModeBaseAddress;

		Outputs::CRT *_crt;

		int _frameCycles, _displayOutputPosition, _audioOutputPosition, _audioOutputPositionError;

		uint16_t _startScreenAddress, _startLineAddress, _currentScreenAddress;
		int _currentOutputLine;
		uint8_t *_currentLine;

		inline void update_display();
		inline void update_audio();
		inline void signal_interrupt(Interrupt interrupt);
		inline void evaluate_interrupts();

		class Speaker: public ::Outputs::Filter<Speaker> {
			public:
				void set_divider(uint8_t divider);

				void set_is_enabled(bool is_enabled);
				inline bool get_is_enabled() { return _is_enabled; }

				void get_samples(unsigned int number_of_samples, int16_t *target);
				void skip_samples(unsigned int number_of_samples);

				Speaker() : _counter(0), _divider(0x32), _is_enabled(false), _output_level(0) {}

			private:
				unsigned int _counter;
				uint8_t _divider;
				bool _is_enabled;
				int16_t _output_level;

		} _speaker;
};

}

#endif /* Electron_hpp */
