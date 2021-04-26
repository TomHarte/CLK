//
//  Keyboard.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/10/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "Keyboard.hpp"

#include <cstring>

using namespace Sinclair::ZX::Keyboard;

KeyboardMapper::KeyboardMapper(Machine machine) : machine_(machine) {}

uint16_t KeyboardMapper::mapped_key_for_key(Inputs::Keyboard::Key key) const {
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

const uint16_t *CharacterMapper::sequence_for_character(char character) const {
#define KEYS(...)		{__VA_ARGS__, MachineTypes::MappedKeyboardMachine::KeyEndSequence}
#define SHIFT(...)		{KeyShift, __VA_ARGS__, MachineTypes::MappedKeyboardMachine::KeyEndSequence}
#define SYMSHIFT(...)	{KeySymbolShift, __VA_ARGS__, MachineTypes::MappedKeyboardMachine::KeyEndSequence}
#define X				{MachineTypes::MappedKeyboardMachine::KeyNotMapped}
	static constexpr KeySequence spectrum_key_sequences[] = {
		/* NUL */	X,							/* SOH */	X,
		/* STX */	X,							/* ETX */	X,
		/* EOT */	X,							/* ENQ */	X,
		/* ACK */	X,							/* BEL */	X,
		/* BS */	SHIFT(Key0),				/* HT */	X,
		/* LF */	KEYS(KeyEnter),				/* VT */	X,
		/* FF */	X,							/* CR */	KEYS(KeyEnter),
		/* SO */	X,							/* SI */	X,
		/* DLE */	X,							/* DC1 */	X,
		/* DC2 */	X,							/* DC3 */	X,
		/* DC4 */	X,							/* NAK */	X,
		/* SYN */	X,							/* ETB */	X,
		/* CAN */	X,							/* EM */	X,
		/* SUB */	X,							/* ESC */	X,
		/* FS */	X,							/* GS */	X,
		/* RS */	X,							/* US */	X,
		/* space */	KEYS(KeySpace),				/* ! */		SYMSHIFT(Key1),
		/* " */		SYMSHIFT(KeyP),				/* # */		SYMSHIFT(Key3),
		/* $ */		SYMSHIFT(Key4),				/* % */		SYMSHIFT(Key5),
		/* & */		SYMSHIFT(Key6),				/* ' */		SYMSHIFT(Key7),
		/* ( */		SYMSHIFT(Key8),				/* ) */		SYMSHIFT(Key9),
		/* * */		SYMSHIFT(KeyB),				/* + */		SYMSHIFT(KeyK),
		/* , */		SYMSHIFT(KeyN),				/* - */		SYMSHIFT(KeyJ),
		/* . */		SYMSHIFT(KeyM),				/* / */		SYMSHIFT(KeyV),
		/* 0 */		KEYS(Key0),					/* 1 */		KEYS(Key1),
		/* 2 */		KEYS(Key2),					/* 3 */		KEYS(Key3),
		/* 4 */		KEYS(Key4),					/* 5 */		KEYS(Key5),
		/* 6 */		KEYS(Key6),					/* 7 */		KEYS(Key7),
		/* 8 */		KEYS(Key8),					/* 9 */		KEYS(Key9),
		/* : */		SYMSHIFT(KeyZ),				/* ; */		SYMSHIFT(KeyO),
		/* < */		SYMSHIFT(KeyR),				/* = */		SYMSHIFT(KeyL),
		/* > */		SYMSHIFT(KeyT),				/* ? */		SYMSHIFT(KeyC),
		/* @ */		SYMSHIFT(Key2),				/* A */		SHIFT(KeyA),
		/* B */		SHIFT(KeyB),				/* C */		SHIFT(KeyC),
		/* D */		SHIFT(KeyD),				/* E */		SHIFT(KeyE),
		/* F */		SHIFT(KeyF),				/* G */		SHIFT(KeyG),
		/* H */		SHIFT(KeyH),				/* I */		SHIFT(KeyI),
		/* J */		SHIFT(KeyJ),				/* K */		SHIFT(KeyK),
		/* L */		SHIFT(KeyL),				/* M */		SHIFT(KeyM),
		/* N */		SHIFT(KeyN),				/* O */		SHIFT(KeyO),
		/* P */		SHIFT(KeyP),				/* Q */		SHIFT(KeyQ),
		/* R */		SHIFT(KeyR),				/* S */		SHIFT(KeyS),
		/* T */		SHIFT(KeyT),				/* U */		SHIFT(KeyU),
		/* V */		SHIFT(KeyV),				/* W */		SHIFT(KeyW),
		/* X */		SHIFT(KeyX),				/* Y */		SHIFT(KeyY),
		/* Z */		SHIFT(KeyZ),				/* [ */		X,
		/* \ */		X,							/* ] */		X,
		/* ^ */		SYMSHIFT(KeyH),				/* _ */		SYMSHIFT(Key0),
		/* ` */		X,							/* a */		KEYS(KeyA),
		/* b */		KEYS(KeyB),					/* c */		KEYS(KeyC),
		/* d */		KEYS(KeyD),					/* e */		KEYS(KeyE),
		/* f */		KEYS(KeyF),					/* g */		KEYS(KeyG),
		/* h */		KEYS(KeyH),					/* i */		KEYS(KeyI),
		/* j */		KEYS(KeyJ),					/* k */		KEYS(KeyK),
		/* l */		KEYS(KeyL),					/* m */		KEYS(KeyM),
		/* n */		KEYS(KeyN),					/* o */		KEYS(KeyO),
		/* p */		KEYS(KeyP),					/* q */		KEYS(KeyQ),
		/* r */		KEYS(KeyR),					/* s */		KEYS(KeyS),
		/* t */		KEYS(KeyT),					/* u */		KEYS(KeyU),
		/* v */		KEYS(KeyV),					/* w */		KEYS(KeyW),
		/* x */		KEYS(KeyX),					/* y */		KEYS(KeyY),
		/* z */		KEYS(KeyZ),
	};

	static constexpr KeySequence zx81_key_sequences[] = {
		/* NUL */	X,							/* SOH */	X,
		/* STX */	X,							/* ETX */	X,
		/* EOT */	X,							/* ENQ */	X,
		/* ACK */	X,							/* BEL */	X,
		/* BS */	SHIFT(Key0),				/* HT */	X,
		/* LF */	KEYS(KeyEnter),				/* VT */	X,
		/* FF */	X,							/* CR */	KEYS(KeyEnter),
		/* SO */	X,							/* SI */	X,
		/* DLE */	X,							/* DC1 */	X,
		/* DC2 */	X,							/* DC3 */	X,
		/* DC4 */	X,							/* NAK */	X,
		/* SYN */	X,							/* ETB */	X,
		/* CAN */	X,							/* EM */	X,
		/* SUB */	X,							/* ESC */	X,
		/* FS */	X,							/* GS */	X,
		/* RS */	X,							/* US */	X,
		/* space */	KEYS(KeySpace),				/* ! */		X,
		/* " */		SHIFT(KeyP),				/* # */		X,
		/* $ */		SHIFT(KeyU),				/* % */		X,
		/* & */		X,							/* ' */		X,
		/* ( */		SHIFT(KeyI),				/* ) */		SHIFT(KeyO),
		/* * */		SHIFT(KeyB),				/* + */		SHIFT(KeyK),
		/* , */		SHIFT(KeyDot),				/* - */		SHIFT(KeyJ),
		/* . */		KEYS(KeyDot),				/* / */		SHIFT(KeyV),
		/* 0 */		KEYS(Key0),					/* 1 */		KEYS(Key1),
		/* 2 */		KEYS(Key2),					/* 3 */		KEYS(Key3),
		/* 4 */		KEYS(Key4),					/* 5 */		KEYS(Key5),
		/* 6 */		KEYS(Key6),					/* 7 */		KEYS(Key7),
		/* 8 */		KEYS(Key8),					/* 9 */		KEYS(Key9),
		/* : */		SHIFT(KeyZ),				/* ; */		SHIFT(KeyX),
		/* < */		SHIFT(KeyN),				/* = */		SHIFT(KeyL),
		/* > */		SHIFT(KeyM),				/* ? */		SHIFT(KeyC),
		/* @ */		X,							/* A */		KEYS(KeyA),
		/* B */		KEYS(KeyB),					/* C */		KEYS(KeyC),
		/* D */		KEYS(KeyD),					/* E */		KEYS(KeyE),
		/* F */		KEYS(KeyF),					/* G */		KEYS(KeyG),
		/* H */		KEYS(KeyH),					/* I */		KEYS(KeyI),
		/* J */		KEYS(KeyJ),					/* K */		KEYS(KeyK),
		/* L */		KEYS(KeyL),					/* M */		KEYS(KeyM),
		/* N */		KEYS(KeyN),					/* O */		KEYS(KeyO),
		/* P */		KEYS(KeyP),					/* Q */		KEYS(KeyQ),
		/* R */		KEYS(KeyR),					/* S */		KEYS(KeyS),
		/* T */		KEYS(KeyT),					/* U */		KEYS(KeyU),
		/* V */		KEYS(KeyV),					/* W */		KEYS(KeyW),
		/* X */		KEYS(KeyX),					/* Y */		KEYS(KeyY),
		/* Z */		KEYS(KeyZ),					/* [ */		X,
		/* \ */		X,							/* ] */		X,
		/* ^ */		X,							/* _ */		X,
		/* ` */		X,							/* a */		KEYS(KeyA),
		/* b */		KEYS(KeyB),					/* c */		KEYS(KeyC),
		/* d */		KEYS(KeyD),					/* e */		KEYS(KeyE),
		/* f */		KEYS(KeyF),					/* g */		KEYS(KeyG),
		/* h */		KEYS(KeyH),					/* i */		KEYS(KeyI),
		/* j */		KEYS(KeyJ),					/* k */		KEYS(KeyK),
		/* l */		KEYS(KeyL),					/* m */		KEYS(KeyM),
		/* n */		KEYS(KeyN),					/* o */		KEYS(KeyO),
		/* p */		KEYS(KeyP),					/* q */		KEYS(KeyQ),
		/* r */		KEYS(KeyR),					/* s */		KEYS(KeyS),
		/* t */		KEYS(KeyT),					/* u */		KEYS(KeyU),
		/* v */		KEYS(KeyV),					/* w */		KEYS(KeyW),
		/* x */		KEYS(KeyX),					/* y */		KEYS(KeyY),
		/* z */		KEYS(KeyZ),					/* { */		X,
		/* | */		X,							/* } */		X,
	};

	static constexpr KeySequence zx80_key_sequences[] = {
		/* NUL */	X,							/* SOH */	X,
		/* STX */	X,							/* ETX */	X,
		/* EOT */	X,							/* ENQ */	X,
		/* ACK */	X,							/* BEL */	X,
		/* BS */	SHIFT(Key0),				/* HT */	X,
		/* LF */	KEYS(KeyEnter),				/* VT */	X,
		/* FF */	X,							/* CR */	KEYS(KeyEnter),
		/* SO */	X,							/* SI */	X,
		/* DLE */	X,							/* DC1 */	X,
		/* DC2 */	X,							/* DC3 */	X,
		/* DC4 */	X,							/* NAK */	X,
		/* SYN */	X,							/* ETB */	X,
		/* CAN */	X,							/* EM */	X,
		/* SUB */	X,							/* ESC */	X,
		/* FS */	X,							/* GS */	X,
		/* RS */	X,							/* US */	X,
		/* space */	KEYS(KeySpace),				/* ! */		X,
		/* " */		SHIFT(KeyY),				/* # */		X,
		/* $ */		SHIFT(KeyU),				/* % */		X,
		/* & */		X,							/* ' */		X,
		/* ( */		SHIFT(KeyI),				/* ) */		SHIFT(KeyO),
		/* * */		SHIFT(KeyP),				/* + */		SHIFT(KeyK),
		/* , */		SHIFT(KeyDot),				/* - */		SHIFT(KeyJ),
		/* . */		KEYS(KeyDot),				/* / */		SHIFT(KeyV),
		/* 0 */		KEYS(Key0),					/* 1 */		KEYS(Key1),
		/* 2 */		KEYS(Key2),					/* 3 */		KEYS(Key3),
		/* 4 */		KEYS(Key4),					/* 5 */		KEYS(Key5),
		/* 6 */		KEYS(Key6),					/* 7 */		KEYS(Key7),
		/* 8 */		KEYS(Key8),					/* 9 */		KEYS(Key9),
		/* : */		SHIFT(KeyZ),				/* ; */		SHIFT(KeyX),
		/* < */		SHIFT(KeyN),				/* = */		SHIFT(KeyL),
		/* > */		SHIFT(KeyM),				/* ? */		SHIFT(KeyC),
		/* @ */		X,							/* A */		KEYS(KeyA),
		/* B */		KEYS(KeyB),					/* C */		KEYS(KeyC),
		/* D */		KEYS(KeyD),					/* E */		KEYS(KeyE),
		/* F */		KEYS(KeyF),					/* G */		KEYS(KeyG),
		/* H */		KEYS(KeyH),					/* I */		KEYS(KeyI),
		/* J */		KEYS(KeyJ),					/* K */		KEYS(KeyK),
		/* L */		KEYS(KeyL),					/* M */		KEYS(KeyM),
		/* N */		KEYS(KeyN),					/* O */		KEYS(KeyO),
		/* P */		KEYS(KeyP),					/* Q */		KEYS(KeyQ),
		/* R */		KEYS(KeyR),					/* S */		KEYS(KeyS),
		/* T */		KEYS(KeyT),					/* U */		KEYS(KeyU),
		/* V */		KEYS(KeyV),					/* W */		KEYS(KeyW),
		/* X */		KEYS(KeyX),					/* Y */		KEYS(KeyY),
		/* Z */		KEYS(KeyZ),					/* [ */		X,
		/* \ */		X,							/* ] */		X,
		/* ^ */		X,							/* _ */		X,
		/* ` */		X,							/* a */		KEYS(KeyA),
		/* b */		KEYS(KeyB),					/* c */		KEYS(KeyC),
		/* d */		KEYS(KeyD),					/* e */		KEYS(KeyE),
		/* f */		KEYS(KeyF),					/* g */		KEYS(KeyG),
		/* h */		KEYS(KeyH),					/* i */		KEYS(KeyI),
		/* j */		KEYS(KeyJ),					/* k */		KEYS(KeyK),
		/* l */		KEYS(KeyL),					/* m */		KEYS(KeyM),
		/* n */		KEYS(KeyN),					/* o */		KEYS(KeyO),
		/* p */		KEYS(KeyP),					/* q */		KEYS(KeyQ),
		/* r */		KEYS(KeyR),					/* s */		KEYS(KeyS),
		/* t */		KEYS(KeyT),					/* u */		KEYS(KeyU),
		/* v */		KEYS(KeyV),					/* w */		KEYS(KeyW),
		/* x */		KEYS(KeyX),					/* y */		KEYS(KeyY),
		/* z */		KEYS(KeyZ),					/* { */		X,
		/* | */		X,							/* } */		X,
	};
#undef KEYS
#undef SHIFT
#undef SYMSHIFT
#undef X

	switch(machine_) {
		case Machine::ZX80:
		return table_lookup_sequence_for_character(zx80_key_sequences, character);

		case Machine::ZX81:
		return table_lookup_sequence_for_character(zx81_key_sequences, character);

		case Machine::ZXSpectrum:
		return table_lookup_sequence_for_character(spectrum_key_sequences, character);
	}
}

bool CharacterMapper::needs_pause_after_key(uint16_t key) const {
	return key != KeyShift && !(machine_ == Machine::ZXSpectrum && key == KeySymbolShift);
}

Keyboard::Keyboard(Machine machine) : machine_(machine) {
	clear_all_keys();
}

void Keyboard::set_key_state(uint16_t key, bool is_pressed) {
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
	memset(key_states_, 0xff, 8);
}

uint8_t Keyboard::read(uint16_t address) {
	uint8_t value = 0xff;

	uint16_t mask = 0x100;
	for(int c = 0; c < 8; c++) {
		if(!(address & mask)) value &= key_states_[c];
		mask <<= 1;
	}

	return value;
}
