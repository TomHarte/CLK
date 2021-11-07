//
//  Keyboard.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/07/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef Machines_Amiga_Keyboard_hpp
#define Machines_Amiga_Keyboard_hpp

#include <cstdint>
#include "../KeyboardMachine.hpp"
#include "../../Components/Serial/Line.hpp"
#include "../../ClockReceiver/ClockReceiver.hpp"

namespace Amiga {

enum class Key: uint16_t {
	Escape	= 0x45,
	Delete	= 0x46,

	F1	= 0x50,	F2	= 0x51,	F3	= 0x52,	F4	= 0x53,	F5	= 0x54,
	F6	= 0x55,	F7	= 0x56,	F8	= 0x57,	F9	= 0x58,	F10	= 0x59,

	Tilde	= 0x00,
	k1	= 0x01,	k2	= 0x02,	k3	= 0x03,	k4	= 0x04,	k5	= 0x05,
	k6	= 0x06,	k7	= 0x07,	k8	= 0x08,	k9	= 0x09,	k0	= 0x0a,

	Hyphen	= 0x0b,
	Equals = 0x0c,
	Backslash = 0x0d,
	Backspace = 0x41,
	Tab = 0x42,
	Control = 0x63,
	CapsLock = 0x62,
	LeftShift = 0x60,
	RightShift = 0x61,

	Q	= 0x10,	W	= 0x11,	E	= 0x12,	R	= 0x13,	T	= 0x14,
	Y	= 0x15,	U	= 0x16,	I	= 0x17,	O	= 0x18,	P	= 0x19,
	A	= 0x20,	S	= 0x21,	D	= 0x22,	F	= 0x23,	G	= 0x24,
	H	= 0x25,	J	= 0x26,	K	= 0x27,	L	= 0x28,	Z	= 0x31,
	X	= 0x32, C	= 0x33,	V	= 0x34,	B	= 0x35,	N	= 0x36,
	M	= 0x37,

	OpenSquareBracket = 0x1a,
	CloseSquareBracket = 0x1b,
	Help = 0x5f,
	Return = 0x44,
	Semicolon = 0x29,
	Quote = 0x2a,
	Comma = 0x38,
	FullStop = 0x39,
	ForwardSlash = 0x3a,
	Alt = 0x64,
	LeftAmiga = 0x66,
	RightAmiga = 0x67,
	Space = 0x40,

	Up	= 0x4c,	Left = 0x4f, Right = 0x4e, Down = 0x4d,

	Keypad7 = 0x3d, Keypad8 = 0x3e, Keypad9 = 0x3f,
	Keypad4 = 0x2d, Keypad5 = 0x2e, Keypad6 = 0x2f,
	Keypad1 = 0x1d, Keypad2 = 0x1e, Keypad3 = 0x1f,
	Keypad0 = 0x0f, KeypadDecimalPoint = 0x3c,
	KeypadMinus = 0x4a, KeypadEnter = 0x43,
	KeypadOpenBracket = 0x5a,
	KeypadCloseBracket = 0x5b,
	KeypadDivide = 0x5c,
	KeypadMultiply = 0x5d,
	KeypadPlus = 0x5e,
};

struct KeyboardMapper: public MachineTypes::MappedKeyboardMachine::KeyboardMapper {
	uint16_t mapped_key_for_key(Inputs::Keyboard::Key key) const final;
};

class Keyboard {
	public:
		Keyboard(Serial::Line<true> &output);

//		enum Lines: uint8_t {
//			Data = (1 << 0),
//			Clock = (1 << 1),
//		};
//
//		uint8_t update(uint8_t);

		void set_key_state(uint16_t, bool);
		void clear_all_keys();

		void run_for(HalfCycles duration) {
			output_.advance_writer(duration);
		}

	private:
		enum class ShiftState {
			Shifting,
			AwaitingHandshake,
			Idle,
		} shift_state_ = ShiftState::Idle;

		enum class State {
			Startup,
		} state_ = State::Startup;

		int bit_phase_ = 0;
		uint32_t shift_sequence_ = 0;
		int bits_remaining_ = 0;

		uint8_t lines_ = 0;

		Serial::Line<true> &output_;
};

}

#endif /* Machines_Amiga_Keyboard_hpp */
