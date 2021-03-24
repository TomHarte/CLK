//
//  Keyboard.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/10/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef Machines_Electron_Keyboard_hpp
#define Machines_Electron_Keyboard_hpp

#include "../KeyboardMachine.hpp"
#include "../Utility/Typer.hpp"

namespace Electron {

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

	// Virtual keys.
	KeyF1			= 0xfff0, KeyF2, KeyF3, KeyF4, KeyF5, KeyF6, KeyF7, KeyF8, KeyF9, KeyF0,

	KeyBreak		= 0xfffd,
};

constexpr bool is_modifier(Key key) {
	return (key == KeyShift) || (key == KeyControl) || (key == KeyFunc);
}

struct KeyboardMapper: public MachineTypes::MappedKeyboardMachine::KeyboardMapper {
	uint16_t mapped_key_for_key(Inputs::Keyboard::Key key) const final;
};

struct CharacterMapper: public ::Utility::CharacterMapper {
	const uint16_t *sequence_for_character(char character) const override;

	bool needs_pause_after_reset_all_keys() const override	{ return false; }
	bool needs_pause_after_key(uint16_t key) const override;
};

};

#endif /* KeyboardMapper_hpp */
