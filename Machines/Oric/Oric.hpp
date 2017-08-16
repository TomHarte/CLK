//
//  Oric.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/10/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Oric_hpp
#define Oric_hpp

#include "../ConfigurationTarget.hpp"
#include "../CRTMachine.hpp"
#include "../KeyboardMachine.hpp"

#include <cstdint>
#include <vector>

namespace Oric {

enum ROM {
	BASIC10, BASIC11, Microdisc, Colour
};

enum Key: uint16_t {
	Key3			= 0x0000 | 0x80,	KeyX			= 0x0000 | 0x40,	Key1			= 0x0000 | 0x20,
	KeyV			= 0x0000 | 0x08,	Key5			= 0x0000 | 0x04,	KeyN			= 0x0000 | 0x02,	Key7			= 0x0000 | 0x01,
	KeyD			= 0x0100 | 0x80,	KeyQ			= 0x0100 | 0x40,	KeyEscape		= 0x0100 | 0x20,
	KeyF			= 0x0100 | 0x08,	KeyR			= 0x0100 | 0x04,	KeyT			= 0x0100 | 0x02,	KeyJ			= 0x0100 | 0x01,
	KeyC			= 0x0200 | 0x80,	Key2			= 0x0200 | 0x40,	KeyZ			= 0x0200 | 0x20,	KeyControl		= 0x0200 | 0x10,
	Key4			= 0x0200 | 0x08,	KeyB			= 0x0200 | 0x04,	Key6			= 0x0200 | 0x02,	KeyM			= 0x0200 | 0x01,
	KeyQuote		= 0x0300 | 0x80,	KeyBackSlash	= 0x0300 | 0x40,
	KeyMinus		= 0x0300 | 0x08,	KeySemiColon	= 0x0300 | 0x04,	Key9			= 0x0300 | 0x02,	KeyK			= 0x0300 | 0x01,
	KeyRight		= 0x0400 | 0x80,	KeyDown			= 0x0400 | 0x40,	KeyLeft			= 0x0400 | 0x20,	KeyLeftShift	= 0x0400 | 0x10,
	KeyUp			= 0x0400 | 0x08,	KeyFullStop		= 0x0400 | 0x04,	KeyComma		= 0x0400 | 0x02,	KeySpace		= 0x0400 | 0x01,
	KeyOpenSquare	= 0x0500 | 0x80,	KeyCloseSquare	= 0x0500 | 0x40,	KeyDelete		= 0x0500 | 0x20,	KeyFunction		= 0x0500 | 0x10,
	KeyP			= 0x0500 | 0x08,	KeyO			= 0x0500 | 0x04,	KeyI			= 0x0500 | 0x02,	KeyU			= 0x0500 | 0x01,
	KeyW			= 0x0600 | 0x80,	KeyS			= 0x0600 | 0x40,	KeyA			= 0x0600 | 0x20,
	KeyE			= 0x0600 | 0x08,	KeyG			= 0x0600 | 0x04,	KeyH			= 0x0600 | 0x02,	KeyY			= 0x0600 | 0x01,
	KeyEquals		= 0x0700 | 0x80,										KeyReturn		= 0x0700 | 0x20,	KeyRightShift	= 0x0700 | 0x10,
	KeyForwardSlash	= 0x0700 | 0x08,	Key0			= 0x0700 | 0x04,	KeyL			= 0x0700 | 0x02,	Key8			= 0x0700 | 0x01,

	KeyNMI			= 0xfffd,
};

/*!
	Models an Oric 1/Atmos with or without a Microdisc.
*/
class Machine:
	public CRTMachine::Machine,
	public ConfigurationTarget::Machine,
	public KeyboardMachine::Machine {
	public:
		virtual ~Machine();

		/// Creates an returns an Oric on the heap.
		static Machine *Oric();

		/// Sets the contents of @c rom to @c data. Assumed to be a setup step; has no effect once a machine is running.
		virtual void set_rom(ROM rom, const std::vector<uint8_t> &data) = 0;

		/// Enables or disables turbo-speed tape loading.
		virtual void set_use_fast_tape_hack(bool activate) = 0;

		/// Sets the type of display the Oric is connected to.
		virtual void set_output_device(Outputs::CRT::OutputDevice output_device) = 0;
};

}
#endif /* Oric_hpp */
