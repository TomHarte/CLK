//
//  Keyboard.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/09/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include "Machines/KeyboardMachine.hpp"

#include <cstdint>
#include <unordered_map>

namespace BBCMicro {

struct KeyboardMapper: public MachineTypes::MappedKeyboardMachine::KeyboardMapper {
	uint16_t mapped_key_for_key(const Inputs::Keyboard::Key key) const override {
		const auto found = key_map.find(key);
		return found != key_map.end() ? found->second : MachineTypes::MappedKeyboardMachine::KeyNotMapped;
	}

private:
	using Key = Inputs::Keyboard::Key;
	static inline const std::unordered_map<Key, uint16_t> key_map{
		{Key::Escape, 0x70},

		{Key::F10, 0x20},	{Key::F1, 0x71},	{Key::F2, 0x72},	{Key::F3, 0x73},	{Key::F4, 0x14},
		{Key::F5, 0x75},	{Key::F6, 0x75},	{Key::F7, 0x16},	{Key::F8, 0x76},	{Key::F9, 0x77},

		{Key::Backslash, 0x78},

		{Key::Left, 0x19},	{Key::Right, 0x79},	{Key::Up, 0x39},	{Key::Down, 0x29},

		{Key::Q, 0x10},		{Key::W, 0x21},		{Key::E, 0x22},		{Key::R, 0x33},		{Key::T, 0x23},
		{Key::Y, 0x44},		{Key::U, 0x35},		{Key::I, 0x25},		{Key::O, 0x36},		{Key::P, 0x37},
		{Key::A, 0x41},		{Key::S, 0x51},		{Key::D, 0x32},		{Key::F, 0x43},		{Key::G, 0x53},
		{Key::H, 0x54},		{Key::J, 0x45},		{Key::K, 0x46},		{Key::L, 0x56},		{Key::Z, 0x61},
		{Key::X, 0x42},		{Key::C, 0x52},		{Key::V, 0x63},		{Key::B, 0x64},		{Key::N, 0x55},
		{Key::M, 0x65},

		{Key::k0, 0x27},	{Key::k1, 0x30},	{Key::k2, 0x31},	{Key::k3, 0x11},	{Key::k4, 0x12},
		{Key::k5, 0x13},	{Key::k6, 0x34},	{Key::k7, 0x24},	{Key::k8, 0x15},	{Key::k9, 0x26},

		{Key::Comma, 0x66},
		{Key::FullStop, 0x67},
		{Key::ForwardSlash, 0x68},

		{Key::Hyphen, 0x17},
		{Key::Equals, 0x18},
		{Key::Quote, 0x69},

		{Key::OpenSquareBracket, 0x38},
		{Key::CloseSquareBracket, 0x58},
		{Key::Semicolon, 0x48},

		{Key::Enter, 0x49},
		{Key::Backspace, 0x59},

		{Key::LeftShift, 0x00},		{Key::RightShift, 0x00},
		{Key::LeftControl, 0x01},	{Key::RightControl, 0x01},

		{Key::Space, 0x62},
	};
};

}
