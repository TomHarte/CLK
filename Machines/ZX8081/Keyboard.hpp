//
//  Keyboard.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/10/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef Machines_ZX8081_Keyboard_hpp
#define Machines_ZX8081_Keyboard_hpp

#include "../KeyboardMachine.hpp"
#include "../Utility/Typer.hpp"

namespace ZX8081 {

enum Key: uint16_t {
	KeyShift	= 0x0000 | 0x01,	KeyZ	= 0x0000 | 0x02,	KeyX = 0x0000 | 0x04,	KeyC = 0x0000 | 0x08,	KeyV = 0x0000 | 0x10,
	KeyA		= 0x0100 | 0x01,	KeyS	= 0x0100 | 0x02,	KeyD = 0x0100 | 0x04,	KeyF = 0x0100 | 0x08,	KeyG = 0x0100 | 0x10,
	KeyQ		= 0x0200 | 0x01,	KeyW	= 0x0200 | 0x02,	KeyE = 0x0200 | 0x04,	KeyR = 0x0200 | 0x08,	KeyT = 0x0200 | 0x10,
	Key1		= 0x0300 | 0x01,	Key2	= 0x0300 | 0x02,	Key3 = 0x0300 | 0x04,	Key4 = 0x0300 | 0x08,	Key5 = 0x0300 | 0x10,
	Key0		= 0x0400 | 0x01,	Key9	= 0x0400 | 0x02,	Key8 = 0x0400 | 0x04,	Key7 = 0x0400 | 0x08,	Key6 = 0x0400 | 0x10,
	KeyP		= 0x0500 | 0x01,	KeyO	= 0x0500 | 0x02,	KeyI = 0x0500 | 0x04,	KeyU = 0x0500 | 0x08,	KeyY = 0x0500 | 0x10,
	KeyEnter	= 0x0600 | 0x01,	KeyL	= 0x0600 | 0x02,	KeyK = 0x0600 | 0x04,	KeyJ = 0x0600 | 0x08,	KeyH = 0x0600 | 0x10,
	KeySpace	= 0x0700 | 0x01,	KeyDot	= 0x0700 | 0x02,	KeyM = 0x0700 | 0x04,	KeyN = 0x0700 | 0x08,	KeyB = 0x0700 | 0x10,

	// Add some virtual keys; these do not exist on a real ZX80 or ZX81. They're just a convenience.
	KeyDelete	= 0x0801,
	KeyBreak, KeyLeft, KeyRight, KeyUp, KeyDown, KeyEdit
};

struct KeyboardMapper: public MachineTypes::MappedKeyboardMachine::KeyboardMapper {
	uint16_t mapped_key_for_key(Inputs::Keyboard::Key key) const override;
};

class CharacterMapper: public ::Utility::CharacterMapper {
	public:
		CharacterMapper(bool is_zx81);
		const uint16_t *sequence_for_character(char character) const override;

		bool needs_pause_after_key(uint16_t key) const override;

	private:
		bool is_zx81_;
};

};

#endif /* KeyboardMapper_hpp */
