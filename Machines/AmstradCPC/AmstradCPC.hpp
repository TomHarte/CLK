//
//  AmstradCPC.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 30/07/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef AmstradCPC_hpp
#define AmstradCPC_hpp

#include "../ConfigurationTarget.hpp"
#include "../CRTMachine.hpp"
#include "../KeyboardMachine.hpp"

#include <cstdint>
#include <vector>

namespace AmstradCPC {

enum ROMType: int {
	OS464 = 0,	BASIC464,
	OS664,		BASIC664,
	OS6128,		BASIC6128,
	AMSDOS
};

enum Key: uint16_t {
#define Line(l, k1, k2, k3, k4, k5, k6, k7, k8)	\
	k1 = (l << 4) | 0x07,	k2 = (l << 4) | 0x06,	k3 = (l << 4) | 0x05,	k4 = (l << 4) | 0x04,\
	k5 = (l << 4) | 0x03,	k6 = (l << 4) | 0x02,	k7 = (l << 4) | 0x01,	k8 = (l << 4) | 0x00,

	Line(0, KeyFDot,		KeyEnter,			KeyF3,			KeyF6,			KeyF9,					KeyDown,		KeyRight,				KeyUp)
	Line(1, KeyF0,			KeyF2,				KeyF1,			KeyF5,			KeyF8,					KeyF7,			KeyCopy,				KeyLeft)
	Line(2, KeyControl,		KeyBackSlash,		KeyShift,		KeyF4,			KeyRightSquareBracket,	KeyReturn,		KeyLeftSquareBracket,	KeyClear)
	Line(3, KeyFullStop,	KeyForwardSlash,	KeyColon,		KeySemicolon,	KeyP,					KeyAt,			KeyMinus,				KeyCaret)
	Line(4, KeyComma,		KeyM,				KeyK,			KeyL,			KeyI,					KeyO,			Key9,					Key0)
	Line(5, KeySpace,		KeyN,				KeyJ,			KeyH,			KeyY,					KeyU,			Key7,					Key8)
	Line(6, KeyV,			KeyB,				KeyF,			KeyG,			KeyT,					KeyR,			Key5,					Key6)
	Line(7, KeyX,			KeyC,				KeyD,			KeyS,			KeyW,					KeyE,			Key3,					Key4)
	Line(8, KeyZ,			KeyCapsLock,		KeyA,			KeyTab,			KeyQ,					KeyEscape,		Key2,					Key1)
	Line(9, KeyDelete,		KeyJoy1Fire3,		KeyJoy2Fire2,	KeyJoy1Fire1,	KeyJoy1Right,			KeyJoy1Left,	KeyJoy1Down,			KeyJoy1Up)

#undef Line
};

/*!
	Models an Amstrad CPC, a CRT-outputting machine with a keyboard that can accept configuration targets.
*/
class Machine:
	public CRTMachine::Machine,
	public ConfigurationTarget::Machine,
	public KeyboardMachine::Machine {
	public:
		virtual ~Machine();

		/// Creates an returns an Amstrad CPC on the heap.
		static Machine *AmstradCPC();

		/// Sets the contents of rom @c type to @c data. Assumed to be a setup step; has no effect once a machine is running.
		virtual void set_rom(ROMType type, std::vector<uint8_t> data) = 0;
};

}

#endif /* AmstradCPC_hpp */
