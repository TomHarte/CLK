//
//  Keyboard.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/11/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef Machines_MSX_Keyboard_hpp
#define Machines_MSX_Keyboard_hpp

#include "../KeyboardMachine.hpp"

namespace MSX {

enum Key: uint16_t {
#define Line(l, k1, k2, k3, k4, k5, k6, k7, k8)	\
	k1 = (l << 4) | 0x07,	k2 = (l << 4) | 0x06,	k3 = (l << 4) | 0x05,	k4 = (l << 4) | 0x04,\
	k5 = (l << 4) | 0x03,	k6 = (l << 4) | 0x02,	k7 = (l << 4) | 0x01,	k8 = (l << 4) | 0x00,

	Line(0, Key7,				Key6,					Key5,					Key4,				Key3,			Key2,				Key1,			Key0)
	Line(1, KeySemicolon,		KeyRightSquareBracket,	KeyLeftSquareBracket,	KeyBackSlash,		KeyEquals,		KeyMinus,			Key9,			Key8)
	Line(2, KeyB,				KeyA,					KeyNA,					KeyForwardSlash,	KeyFullStop,	KeyComma,			KeyGrave,		KeyQuote)
	Line(3, KeyJ,				KeyI,					KeyH,					KeyG,				KeyF,			KeyE,				KeyD,			KeyC)
	Line(4, KeyR,				KeyQ,					KeyP,					KeyO,				KeyN,			KeyM,				KeyL,			KeyK)
	Line(5, KeyZ,				KeyY,					KeyX,					KeyW,				KeyV,			KeyU,				KeyT,			KeyS)
	Line(6, KeyF3,				KeyF2,					KeyF1,					KeyCode,			KeyCaps,		KeyGraph,			KeyControl,		KeyShift)
	Line(7, KeyEnter,			KeySelect,				KeyBackspace,			KeyStop,			KeyTab,			KeyEscape,			KeyF5,			KeyF4)
	Line(8, KeyRight,			KeyDown,				KeyUp,					KeyLeft,			KeyDelete,		KeyInsert,			KeyHome,		KeySpace)
	Line(9, KeyNumpad4,			KeyNumpad3,				KeyNumpad2,				KeyNumpad1,			KeyNumpad0,		KeyNumpadDivide,	KeyNumpadAdd,	KeyNumpadMultiply)
	Line(10, KeyNumpadDecimal,	KeyNumpadComma,			KeyNumpadSubtract,		KeyNumpad9,			KeyNumpad8,		KeyNumpad7,			KeyNumpad6,		KeyNumpad5)

#undef Line
};

struct KeyboardMapper: public MachineTypes::MappedKeyboardMachine::KeyboardMapper {
	uint16_t mapped_key_for_key(Inputs::Keyboard::Key key);
};

};

#endif /* Machines_MSX_Keyboard_hpp */
