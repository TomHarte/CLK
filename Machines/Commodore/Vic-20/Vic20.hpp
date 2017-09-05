//
//  Vic20.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Vic20_hpp
#define Vic20_hpp

#include "../../ConfigurationTarget.hpp"
#include "../../CRTMachine.hpp"
#include "../../Typer.hpp"

#include <cstdint>

namespace Commodore {
namespace Vic20 {

enum ROMSlot {
	Kernel,
	BASIC,
	Characters,
	Drive
};

enum MemorySize {
	Default,
	ThreeKB,
	ThirtyTwoKB
};

enum Region {
	NTSC,
	PAL
};

enum Key: uint16_t {
#define key(line, mask) (((mask) << 3) | (line))
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
#undef key
};

enum JoystickInput {
	Up = 0x04,
	Down = 0x08,
	Left = 0x10,
	Right = 0x80,
	Fire = 0x20
};

class Machine:
	public CRTMachine::Machine,
	public ConfigurationTarget::Machine,
	public KeyboardMachine::Machine {
	public:
		virtual ~Machine();

		/// Creates and returns a Vic-20.
		static Machine *Vic20();

		/// Sets the contents of the rom in @c slot to the buffer @c data of length @c length.
		virtual void set_rom(ROMSlot slot, size_t length, const uint8_t *data) = 0;
		// TODO: take a std::vector<uint8_t> to collapse length and data.

		/// Sets the state of joystick input @c input.
		virtual void set_joystick_state(JoystickInput input, bool isPressed) = 0;

		/// Sets the memory size of this Vic-20.
		virtual void set_memory_size(MemorySize size) = 0;

		/// Sets the region of this Vic-20.
		virtual void set_region(Region region) = 0;

		/// Enables or disables turbo-speed tape loading.
		virtual void set_use_fast_tape_hack(bool activate) = 0;
};

}
}

#endif /* Vic20_hpp */
