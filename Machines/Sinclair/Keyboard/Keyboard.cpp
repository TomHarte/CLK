//
//  Keyboard.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/10/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "Keyboard.hpp"

#include <algorithm>

using namespace Sinclair::ZX::Keyboard;

KeyboardMapper::KeyboardMapper(const Machine machine) : machine_(machine) {}

uint16_t KeyboardMapper::mapped_key_for_key(const Inputs::Keyboard::Key key) const {
#define BIND(source, dest)	case Inputs::Keyboard::Key::source:	return dest
	switch(key) {
		default: break;

		BIND(k0, Key0);		BIND(k1, Key1);		BIND(k2, Key2);		BIND(k3, Key3);		BIND(k4, Key4);
		BIND(k5, Key5);		BIND(k6, Key6);		BIND(k7, Key7);		BIND(k8, Key8);		BIND(k9, Key9);
		BIND(Q, KeyQ);		BIND(W, KeyW);		BIND(E, KeyE);		BIND(R, KeyR);		BIND(T, KeyT);
		BIND(Y, KeyY);		BIND(U, KeyU);		BIND(I, KeyI);		BIND(O, KeyO);		BIND(P, KeyP);
		BIND(A, KeyA);		BIND(S, KeyS);		BIND(D, KeyD);		BIND(F, KeyF);		BIND(G, KeyG);
		BIND(H, KeyH);		BIND(J, KeyJ);		BIND(K, KeyK);		BIND(L, KeyL);
		BIND(Z, KeyZ);		BIND(X, KeyX);		BIND(C, KeyC);		BIND(V, KeyV);
		BIND(B, KeyB);		BIND(N, KeyN);		BIND(M, KeyM);

		BIND(LeftShift, KeyShift);	BIND(RightShift, KeyShift);
		BIND(Enter, KeyEnter);
		BIND(Space, KeySpace);

		// Full stop has a key on the ZX80 and ZX81; it doesn't have a dedicated key on the Spectrum.
		case Inputs::Keyboard::Key::FullStop:
			if(machine_ == Machine::ZXSpectrum) {
				return KeySpectrumDot;
			} else {
				return KeyDot;
			}
		break;

		// Map controls and options to symbol shift, if this is a ZX Spectrum.
		case Inputs::Keyboard::Key::LeftOption:
		case Inputs::Keyboard::Key::RightOption:
		case Inputs::Keyboard::Key::LeftControl:
		case Inputs::Keyboard::Key::RightControl:
			if(machine_ == Machine::ZXSpectrum) {
				return KeySymbolShift;
			}
		break;

		// Virtual keys follow.
		BIND(Backspace, KeyDelete);
		BIND(Escape, KeyBreak);
		BIND(Up, KeyUp);
		BIND(Down, KeyDown);
		BIND(Left, KeyLeft);
		BIND(Right, KeyRight);
		BIND(BackTick, KeyEdit);	BIND(F1, KeyEdit);
		BIND(Comma, KeyComma);
	}
#undef BIND
	return MachineTypes::MappedKeyboardMachine::KeyNotMapped;
}

CharacterMapper::CharacterMapper(Machine machine) : machine_(machine) {}

namespace {
const std::unordered_map<wchar_t, const std::vector<uint16_t>> zx80_key_sequences = {
	{L'\b', {KeyShift, Key0}},
	{L'\n', {KeyEnter}},	{L'\r', {KeyEnter}},
	{L' ', {KeySpace}},

	{L'"', {KeyShift, KeyY}},
	{L'$', {KeyShift, KeyU}},
	{L'(', {KeyShift, KeyI}},
	{L')', {KeyShift, KeyO}},
	{L'*', {KeyShift, KeyP}},

	{L'-', {KeyShift, KeyJ}},
	{L'+', {KeyShift, KeyK}},
	{L'=', {KeyShift, KeyL}},

	{L':', {KeyShift, KeyZ}},
	{L';', {KeyShift, KeyX}},
	{L'?', {KeyShift, KeyC}},
	{L'/', {KeyShift, KeyV}},
	{L'<', {KeyShift, KeyN}},
	{L'>', {KeyShift, KeyM}},
	{L',', {KeyShift, KeyDot}},
	{L'£', {KeyShift, KeySpace}},

	{L'0', {Key0}},	{L'1', {Key1}},	{L'2', {Key2}},	{L'3', {Key3}},	{L'4', {Key4}},
	{L'5', {Key5}},	{L'6', {Key6}},	{L'7', {Key7}},	{L'8', {Key8}},	{L'9', {Key9}},

	{L'a', {KeyA}},	{L'b', {KeyB}},	{L'c', {KeyC}},
	{L'd', {KeyD}},	{L'e', {KeyE}},	{L'f', {KeyF}},
	{L'g', {KeyG}},	{L'h', {KeyH}},	{L'i', {KeyI}},
	{L'j', {KeyJ}},	{L'k', {KeyK}},	{L'l', {KeyL}},
	{L'm', {KeyM}},	{L'n', {KeyN}},	{L'o', {KeyO}},
	{L'p', {KeyP}},	{L'q', {KeyQ}},	{L'r', {KeyR}},
	{L's', {KeyS}},	{L't', {KeyT}},	{L'u', {KeyU}},
	{L'v', {KeyV}},	{L'w', {KeyW}},	{L'x', {KeyX}},
	{L'y', {KeyY}},	{L'z', {KeyZ}},	{L'.', {KeyDot}},
};

const std::unordered_map<wchar_t, const std::vector<uint16_t>> zx81_key_sequences = {
	{L'\b', {KeyShift, Key0}},
	{L'\n', {KeyEnter}},	{L'\r', {KeyEnter}},
	{L' ', {KeySpace}},

	{L'$', {KeyShift, KeyU}},
	{L'(', {KeyShift, KeyI}},
	{L')', {KeyShift, KeyO}},
	{L'"', {KeyShift, KeyP}},

	{L'-', {KeyShift, KeyJ}},
	{L'+', {KeyShift, KeyK}},
	{L'=', {KeyShift, KeyL}},

	{L':', {KeyShift, KeyZ}},
	{L';', {KeyShift, KeyX}},
	{L'?', {KeyShift, KeyC}},
	{L'/', {KeyShift, KeyV}},
	{L'*', {KeyShift, KeyB}},
	{L'<', {KeyShift, KeyN}},
	{L'>', {KeyShift, KeyM}},
	{L',', {KeyShift, KeyDot}},
	{L'£', {KeyShift, KeySpace}},

	{L'0', {Key0}},	{L'1', {Key1}},	{L'2', {Key2}},	{L'3', {Key3}},	{L'4', {Key4}},
	{L'5', {Key5}},	{L'6', {Key6}},	{L'7', {Key7}},	{L'8', {Key8}},	{L'9', {Key9}},

	{L'a', {KeyA}},	{L'b', {KeyB}},	{L'c', {KeyC}},
	{L'd', {KeyD}},	{L'e', {KeyE}},	{L'f', {KeyF}},
	{L'g', {KeyG}},	{L'h', {KeyH}},	{L'i', {KeyI}},
	{L'j', {KeyJ}},	{L'k', {KeyK}},	{L'l', {KeyL}},
	{L'm', {KeyM}},	{L'n', {KeyN}},	{L'o', {KeyO}},
	{L'p', {KeyP}},	{L'q', {KeyQ}},	{L'r', {KeyR}},
	{L's', {KeyS}},	{L't', {KeyT}},	{L'u', {KeyU}},
	{L'v', {KeyV}},	{L'w', {KeyW}},	{L'x', {KeyX}},
	{L'y', {KeyY}},	{L'z', {KeyZ}},	{L'.', {KeyDot}},
};

const std::unordered_map<wchar_t, const std::vector<uint16_t>> spectrum_key_sequences = {
	{L'\b', {KeyShift, Key0}},
	{L'\n', {KeyEnter}},	{L'\r', {KeyEnter}},

	{L'!', {KeySymbolShift, Key1}},
	{L'@', {KeySymbolShift, Key2}},
	{L'#', {KeySymbolShift, Key3}},
	{L'$', {KeySymbolShift, Key4}},
	{L'%', {KeySymbolShift, Key5}},
	{L'&', {KeySymbolShift, Key6}},
	{L'\'', {KeySymbolShift, Key7}},
	{L'(', {KeySymbolShift, Key8}},
	{L')', {KeySymbolShift, Key9}},
	{L'_', {KeySymbolShift, Key0}},

	{L'<', {KeySymbolShift, KeyR}},
	{L'>', {KeySymbolShift, KeyT}},
	{L';', {KeySymbolShift, KeyO}},
	{L'"', {KeySymbolShift, KeyP}},

	{L'↑', {KeySymbolShift, KeyH}},
	{L'-', {KeySymbolShift, KeyJ}},
	{L'+', {KeySymbolShift, KeyK}},
	{L'=', {KeySymbolShift, KeyL}},

	{L':', {KeySymbolShift, KeyZ}},
	{L'£', {KeySymbolShift, KeyX}},
	{L'?', {KeySymbolShift, KeyC}},
	{L'/', {KeySymbolShift, KeyV}},
	{L'*', {KeySymbolShift, KeyB}},
	{L',', {KeySymbolShift, KeyN}},
	{L'.', {KeySymbolShift, KeyM}},

	{L'~', {KeyExtendedMode, KeyA}},
	{L'|', {KeyExtendedMode, KeyS}},
	{L'\\', {KeyExtendedMode, KeyD}},
	{L'{', {KeyExtendedMode, KeyF}},
	{L'}', {KeyExtendedMode, KeyG}},
	{L'[', {KeyExtendedMode, KeyY}},
	{L']', {KeyExtendedMode, KeyU}},
	{L'©', {KeyExtendedMode, KeyP}},

	{L' ', {KeySpace}},

	{L'0', {Key0}},	{L'1', {Key1}},	{L'2', {Key2}},	{L'3', {Key3}},	{L'4', {Key4}},
	{L'5', {Key5}},	{L'6', {Key6}},	{L'7', {Key7}},	{L'8', {Key8}},	{L'9', {Key9}},

	{L'A', {KeyShift, KeyA}},	{L'B', {KeyShift, KeyB}},	{L'C', {KeyShift, KeyC}},
	{L'D', {KeyShift, KeyD}},	{L'E', {KeyShift, KeyE}},	{L'F', {KeyShift, KeyF}},
	{L'G', {KeyShift, KeyG}},	{L'H', {KeyShift, KeyH}},	{L'I', {KeyShift, KeyI}},
	{L'J', {KeyShift, KeyJ}},	{L'K', {KeyShift, KeyK}},	{L'L', {KeyShift, KeyL}},
	{L'M', {KeyShift, KeyM}},	{L'N', {KeyShift, KeyN}},	{L'O', {KeyShift, KeyO}},
	{L'P', {KeyShift, KeyP}},	{L'Q', {KeyShift, KeyQ}},	{L'R', {KeyShift, KeyR}},
	{L'S', {KeyShift, KeyS}},	{L'T', {KeyShift, KeyT}},	{L'U', {KeyShift, KeyU}},
	{L'V', {KeyShift, KeyV}},	{L'W', {KeyShift, KeyW}},	{L'X', {KeyShift, KeyX}},
	{L'Y', {KeyShift, KeyY}},	{L'Z', {KeyShift, KeyZ}},

	{L'a', {KeyA}},	{L'b', {KeyB}},	{L'c', {KeyC}},
	{L'd', {KeyD}},	{L'e', {KeyE}},	{L'f', {KeyF}},
	{L'g', {KeyG}},	{L'h', {KeyH}},	{L'i', {KeyI}},
	{L'j', {KeyJ}},	{L'k', {KeyK}},	{L'l', {KeyL}},
	{L'm', {KeyM}},	{L'n', {KeyN}},	{L'o', {KeyO}},
	{L'p', {KeyP}},	{L'q', {KeyQ}},	{L'r', {KeyR}},
	{L's', {KeyS}},	{L't', {KeyT}},	{L'u', {KeyU}},
	{L'v', {KeyV}},	{L'w', {KeyW}},	{L'x', {KeyX}},
	{L'y', {KeyY}},	{L'z', {KeyZ}},
};
}

const std::vector<uint16_t> *CharacterMapper::sequence_for_character(const wchar_t character) const {
	switch(machine_) {
		case Machine::ZX80:			return lookup_sequence(zx80_key_sequences, character);
		case Machine::ZX81:			return lookup_sequence(zx81_key_sequences, character);
		default:
		case Machine::ZXSpectrum:	return lookup_sequence(spectrum_key_sequences, character);
	}
}

bool CharacterMapper::needs_pause_after_key(const uint16_t key) const {
	return key != KeyShift && !(machine_ == Machine::ZXSpectrum && key == KeySymbolShift);
}

Keyboard::Keyboard(Machine machine) : machine_(machine) {
	clear_all_keys();
}

void Keyboard::set_key_state(const uint16_t key, const bool is_pressed) {
	const auto line = key >> 8;

	// Check for special cases.
	if(line > 7) {
		switch(key) {
#define ShiftedKey(source, base, shift)	\
			case source:				\
				set_key_state(shift, is_pressed);	\
				set_key_state(base, is_pressed);	\
			break;

			ShiftedKey(KeyDelete, Key0, KeyShift);
			ShiftedKey(KeyBreak, KeySpace, KeyShift);
			ShiftedKey(KeyUp, Key7, KeyShift);
			ShiftedKey(KeyDown, Key6, KeyShift);
			ShiftedKey(KeyLeft, Key5, KeyShift);
			ShiftedKey(KeyRight, Key8, KeyShift);
			ShiftedKey(KeyEdit, (machine_ == Machine::ZX80) ? KeyEnter : Key1, KeyShift);

			ShiftedKey(KeySpectrumDot, KeyM, KeySymbolShift);

			case KeyComma:
				if(machine_ == Machine::ZXSpectrum) {
					// Spectrum: comma = symbol shift + n.
					set_key_state(KeySymbolShift, is_pressed);
					set_key_state(KeyN, is_pressed);
				} else {
					// ZX80/81: comma = shift + dot.
					set_key_state(KeyShift, is_pressed);
					set_key_state(KeyDot, is_pressed);
				}
			break;

#undef ShiftedKey
		}
	} else {
		if(is_pressed)
			key_states_[line] &= uint8_t(~key);
		else
			key_states_[line] |= uint8_t(key);
	}
}

void Keyboard::clear_all_keys() {
	std::fill(std::begin(key_states_), std::end(key_states_), 0xff);
}

uint8_t Keyboard::read(const uint16_t address) {
	uint8_t value = 0xff;

	uint16_t mask = 0x100;
	for(int c = 0; c < 8; c++) {
		if(!(address & mask)) value &= key_states_[c];
		mask <<= 1;
	}

	return value;
}
