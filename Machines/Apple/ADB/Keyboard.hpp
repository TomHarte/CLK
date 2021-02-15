//
//  Keyboard.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 13/02/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef Keyboard_hpp
#define Keyboard_hpp

#include "ReactiveDevice.hpp"
#include "../../../Inputs/Keyboard.hpp"
#include "../../KeyboardMachine.hpp"

#include <array>
#include <cstdint>
#include <mutex>
#include <vector>

namespace Apple {
namespace ADB {

/*!
	Defines the keycodes that could be passed directly via set_key_pressed; these
	are based on the Apple Extended Keyboard.
*/
enum class Key: uint16_t {
	/*
		These are transcribed from Page 19-11 of
		the Macintosh Family Hardware Reference.
	*/
	BackTick = 0x32,
	k1 = 0x12,	k2 = 0x13,	k3 = 0x14,	k4 = 0x15,	k5 = 0x17,
	k6 = 0x16,	k7 = 0x1a,	k8 = 0x1c,	k9 = 0x19,	k0 = 0x1d,

	Help = 0x72,
	Home = 0x73,
	PageUp = 0x74,
	Delete = 0x75,
	End = 0x77,
	PageDown = 0x79,

	Escape = 0x35,
	Hyphen = 0x1b,
	Equals = 0x18,
	Backspace = 0x33,
	Tab = 0x30,
	Power = 0x7f,

	F1 = 0x7a,	F2 = 0x78,	F3 = 0x63,	F4 = 0x76,
	F5 = 0x60,	F6 = 0x61,	F7 = 0x62,	F8 = 0x64,
	F9 = 0x65,	F10 = 0x6d,	F11 = 0x67,	F12 = 0x6f,
	F13 = 0x69,	F14 = 0x6b,	F15 = 0x71,

	Q = 0x0c, W = 0x0d, E = 0x0e, R = 0x0f, T = 0x11, Y = 0x10, U = 0x20, I = 0x22, O = 0x1f, P = 0x23,
	A = 0x00, S = 0x01, D = 0x02, F = 0x03, G = 0x05, H = 0x04, J = 0x26, K = 0x28, L = 0x25,
	Z = 0x06, X = 0x07, C = 0x08, V = 0x09, B = 0x0b, N = 0x2d, M = 0x2e,

	OpenSquareBracket = 0x21,
	CloseSquareBracket = 0x1e,
	Semicolon = 0x29,
	Quote = 0x27,
	Comma = 0x2b,
	FullStop = 0x2f,
	ForwardSlash = 0x2c,

	CapsLock = 0x39,
	LeftShift = 0x38,		RightShift = 0x7b,
	LeftControl = 0x36,		RightControl = 0x7d,
	LeftOption = 0x3a,		RightOption = 0x7c,
	Command = 0x37,

	Space = 0x31,
	Backslash = 0x2a,
	Return = 0x24,

	Left = 0x3b,
	Right = 0x3c,
	Up = 0x3e,
	Down = 0x3d,

	KeypadClear = 0x47,
	KeypadEquals = 0x51,
	KeypadSlash = 0x4b,
	KeypadAsterisk = 0x43,
	KeypadMinus = 0x4e,
	KeypadPlus = 0x45,
	KeypadEnter = 0x4c,
	KeypadDecimalPoint = 0x41,

	Keypad9 = 0x5c,	Keypad8 = 0x5b,	Keypad7 = 0x59,
	Keypad6 = 0x58,	Keypad5 = 0x57,	Keypad4 = 0x56,
	Keypad3 = 0x55,	Keypad2 = 0x54,	Keypad1 = 0x53,
	Keypad0 = 0x52,
};

class Keyboard: public ReactiveDevice {
	public:
		Keyboard(Bus &);

		bool set_key_pressed(Key key, bool is_pressed);
		void clear_all_keys();

	private:
		void perform_command(const Command &command) override;
		void did_receive_data(const Command &, const std::vector<uint8_t> &) override;

		std::mutex keys_mutex_;
		std::array<bool, 128> pressed_keys_;
		std::vector<uint8_t> pending_events_;
		uint16_t modifiers_ = 0xffff;
};

/*!
	Provides a mapping from idiomatic PC keys to ADB keys.
*/
class KeyboardMapper: public MachineTypes::MappedKeyboardMachine::KeyboardMapper {
	uint16_t mapped_key_for_key(Inputs::Keyboard::Key key) const final;
};

}
}

#endif /* Keyboard_hpp */
