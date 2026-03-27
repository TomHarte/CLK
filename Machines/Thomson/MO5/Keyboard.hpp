//
//  Keyboard.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/03/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include "Machines/KeyboardMachine.hpp"
#include "Machines/Utility/Typer.hpp"

namespace MO5 {

enum Key: uint16_t {
	Key0	= 0x1e,		Key1	= 0x2f,		Key2	= 0x27,		Key3 	= 0x1f,
	Key4	= 0x17,		Key5	= 0x0f,		Key6	= 0x07,		Key7 	= 0x06,
	Key8	= 0x0e,		Key9	= 0x16,

	KeyA 	= 0x2d,		KeyZ	= 0x25,		KeyE 	= 0x1d,		KeyR	= 0x15,
	KeyT	= 0x0d,		KeyY	= 0x05,		KeyU	= 0x04,		KeyI	= 0x0c,
	KeyO	= 0x14,		KeyP	= 0x1c,		KeyQ	= 0x2b,		KeyS	= 0x23,
	KeyD	= 0x1b,		KeyF	= 0x13,		KeyG	= 0x0b,		KeyH	= 0x03,
	KeyJ	= 0x02,		KeyK	= 0x0a,		KeyL	= 0x12,		KeyW	= 0x30,
	KeyX	= 0x28,		KeyC	= 0x32,		KeyV	= 0x2a,		KeyB	= 0x22,
	KeyN	= 0x00,		KeyM	= 0x1a,

	KeyComma	=	0x08,
	KeyFullStop	=	0x10,
	KeyAt		=	0x18,
	KeyAsterisk =	0x2c,
	KeyMinus	=	0x26,
	KeyPlus		= 	0x2e,

	KeyShift	=	0x38,	// On a real MO: a yellow triangle.
	KeyBASIC	=	0x39,
	KeyControl	=	0x35,
	KeyRAZ		=	0x33,

	KeySpace	= 	0x20,
	KeyEnter	=	0x34,

	KeyUp		=	0x31,
	KeyDown		=	0x21,
	KeyLeft		=	0x29,
	KeyRight	=	0x19,

	KeyINS		=	0x09,
	KeyEFF		=	0x11,

	KeyACC		=	0x36,
	KeyStop		=	0x37,
};

struct KeyboardMapper: public MachineTypes::MappedKeyboardMachine::KeyboardMapper {
	uint16_t mapped_key_for_key(const Inputs::Keyboard::Key key) const {
		switch(key) {
			default:	return MachineTypes::MappedKeyboardMachine::KeyNotMapped;

			using enum Inputs::Keyboard::Key;
			case k0:	return Key0;		case k1:	return Key1;
			case k2:	return Key2;		case k3:	return Key3;
			case k4:	return Key4;		case k5:	return Key5;
			case k6:	return Key6;		case k7:	return Key7;
			case k8:	return Key8;		case k9:	return Key9;

			case Q:		return KeyA;		case W:		return KeyZ;
			case E:		return KeyE;		case R:		return KeyR;
			case T:		return KeyT;		case Y:		return KeyY;
			case U:		return KeyU;		case I:		return KeyI;
			case O:		return KeyO;		case P:		return KeyP;
			case A:		return KeyQ;		case S:		return KeyS;
			case D:		return KeyD;		case F:		return KeyF;
			case G:		return KeyG;		case H:		return KeyH;
			case J:		return KeyJ;		case K:		return KeyK;
			case L:		return KeyL;		case Z:		return KeyW;
			case X:		return KeyX;		case C:		return KeyC;
			case V:		return KeyV;		case B:		return KeyB;
			case N:		return KeyN;		case M:		return KeyM;

			case FullStop:	return KeyFullStop;
			case Comma:		return KeyComma;
			case Hyphen:	return KeyMinus;
			case Equals:	return KeyPlus;

			case OpenSquareBracket:	return KeyAt;
			case Semicolon:			return KeyM;

			case CloseSquareBracket:
			case Quote:		return KeyAsterisk;

			case Space:		return KeySpace;
			case Enter:		return KeyEnter;
			case Backspace:	return KeyACC;
			case Escape:	return KeyStop;

			case Up:		return KeyUp;	case Down:	return KeyDown;
			case Left:		return KeyLeft;	case Right:	return KeyRight;

			case LeftShift:
			case RightShift:	return KeyShift;
			case Tab:
			case LeftControl:
			case RightControl:	return KeyControl;
			case LeftOption:
			case RightOption:	return KeyBASIC;
			case LeftMeta:
			case RightMeta:		return KeyRAZ;

			case Insert:		return KeyINS;
			case Home:			return KeyEFF;
		}
	}
};

}
