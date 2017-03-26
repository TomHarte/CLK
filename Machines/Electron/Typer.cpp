//
//  Typer.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/11/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Electron.hpp"

int Electron::Machine::get_typer_delay() {
	return get_is_resetting() ? 625*25*128 : 0;	// wait one second if resetting
}

int Electron::Machine::get_typer_frequency() {
	return 625*128*2;	// accept a new character every two frames
}

uint16_t *Electron::Machine::sequence_for_character(Utility::Typer *typer, char character) {
#define KEYS(...)	{__VA_ARGS__, TerminateSequence}
#define SHIFT(...)	{KeyShift, __VA_ARGS__, TerminateSequence}
#define CTRL(...)	{KeyControl, __VA_ARGS__, TerminateSequence}
#define X			{NotMapped}
	static Key key_sequences[][3] = {
		/* NUL */	X,							/* SOH */	X,
		/* STX */	X,							/* ETX */	X,
		/* EOT */	X,							/* ENQ */	X,
		/* ACK */	X,							/* BEL */	X,
		/* BS */	KEYS(KeyDelete),			/* HT */	X,
		/* LF */	KEYS(KeyReturn),			/* VT */	X,
		/* FF */	X,							/* CR */	X,
		/* SO */	X,							/* SI */	X,
		/* DLE */	X,							/* DC1 */	X,
		/* DC2 */	X,							/* DC3 */	X,
		/* DC4 */	X,							/* NAK */	X,
		/* SYN */	X,							/* ETB */	X,
		/* CAN */	X,							/* EM */	X,
		/* SUB */	X,							/* ESC */	X,
		/* FS */	X,							/* GS */	X,
		/* RS */	X,							/* US */	X,
		/* space */	KEYS(KeySpace),				/* ! */		SHIFT(Key1),
		/* " */		SHIFT(Key2),				/* # */		SHIFT(Key3),
		/* $ */		SHIFT(Key4),				/* % */		SHIFT(Key5),
		/* & */		SHIFT(Key6),				/* ' */		SHIFT(Key7),
		/* ( */		SHIFT(Key8),				/* ) */		SHIFT(Key9),
		/* * */		SHIFT(KeyColon),			/* + */		SHIFT(KeySemiColon),
		/* , */		KEYS(KeyComma),				/* - */		KEYS(KeyMinus),
		/* . */		KEYS(KeyFullStop),			/* / */		KEYS(KeySlash),
		/* 0 */		KEYS(Key0),					/* 1 */		KEYS(Key1),
		/* 2 */		KEYS(Key2),					/* 3 */		KEYS(Key3),
		/* 4 */		KEYS(Key4),					/* 5 */		KEYS(Key5),
		/* 6 */		KEYS(Key6),					/* 7 */		KEYS(Key7),
		/* 8 */		KEYS(Key8),					/* 9 */		KEYS(Key9),
		/* : */		KEYS(KeyColon),				/* ; */		KEYS(KeySemiColon),
		/* < */		SHIFT(KeyComma),			/* = */		SHIFT(KeyMinus),
		/* > */		SHIFT(KeyFullStop),			/* ? */		SHIFT(KeySlash),
		/* @ */		SHIFT(Key0),				/* A */		KEYS(KeyA),
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
		/* Z */		KEYS(KeyZ),					/* [ */		SHIFT(KeyCopy),
		/* \ */		CTRL(KeyRight),				/* ] */		CTRL(KeyCopy),
		/* ^ */		SHIFT(KeyLeft),				/* _ */		SHIFT(KeyDown),
		/* ` */		X,							/* a */		SHIFT(KeyA),
		/* b */		SHIFT(KeyB),				/* c */		SHIFT(KeyC),
		/* d */		SHIFT(KeyD),				/* e */		SHIFT(KeyE),
		/* f */		SHIFT(KeyF),				/* g */		SHIFT(KeyG),
		/* h */		SHIFT(KeyH),				/* i */		SHIFT(KeyI),
		/* j */		SHIFT(KeyJ),				/* k */		SHIFT(KeyK),
		/* l */		SHIFT(KeyL),				/* m */		SHIFT(KeyM),
		/* n */		SHIFT(KeyN),				/* o */		SHIFT(KeyO),
		/* p */		SHIFT(KeyP),				/* q */		SHIFT(KeyQ),
		/* r */		SHIFT(KeyR),				/* s */		SHIFT(KeyS),
		/* t */		SHIFT(KeyT),				/* u */		SHIFT(KeyU),
		/* v */		SHIFT(KeyV),				/* w */		SHIFT(KeyW),
		/* x */		SHIFT(KeyX),				/* y */		SHIFT(KeyY),
		/* z */		SHIFT(KeyZ),				/* { */		CTRL(KeyUp),
		/* | */		SHIFT(KeyRight),			/* } */		CTRL(KeyDown),
		/* ~ */		CTRL(KeyLeft)
	};
#undef KEYS
#undef SHIFT
#undef X

	if(character > sizeof(key_sequences) / sizeof(*key_sequences)) return nullptr;
	if(key_sequences[character][0] == NotMapped) return nullptr;
	return (uint16_t *)key_sequences[character];
}
