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

namespace Thomson::MO::Keyboard {

enum class Machine {
	MO5, MO6, Prodest128
};

namespace MO5 {
enum Key: uint16_t {
	k0	= 0x1e,		k1	= 0x2f,		k2	= 0x27,		k3 	= 0x1f,
	k4	= 0x17,		k5	= 0x0f,		k6	= 0x07,		k7 	= 0x06,
	k8	= 0x0e,		k9	= 0x16,

	A 	= 0x2d,		Z	= 0x25,		E 	= 0x1d,		R	= 0x15,
	T	= 0x0d,		Y	= 0x05,		U	= 0x04,		I	= 0x0c,
	O	= 0x14,		P	= 0x1c,		Q	= 0x2b,		S	= 0x23,
	D	= 0x1b,		F	= 0x13,		G	= 0x0b,		H	= 0x03,
	J	= 0x02,		K	= 0x0a,		L	= 0x12,		W	= 0x30,
	X	= 0x28,		C	= 0x32,		V	= 0x2a,		B	= 0x22,
	N	= 0x00,		M	= 0x1a,

	Comma		= 0x08,
	FullStop	= 0x10,
	At			= 0x18,
	Asterisk	= 0x2c,
	Minus		= 0x26,
	Plus		= 0x2e,

	Shift	= 0x38,	// On a real MO: a yellow triangle.
	BASIC	= 0x39,
	Control	= 0x35,
	RAZ		= 0x33,

	Space	= 0x20,
	Enter	= 0x34,

	Up		= 0x31,
	Down	= 0x21,
	Left	= 0x29,
	Right	= 0x19,

	INS		= 0x09,
	EFF		= 0x11,

	ACC		= 0x36,
	Stop	= 0x37,

	ForwardSlash	= 0x24,
};

inline const std::unordered_map<wchar_t, const std::vector<uint16_t>> sequences = {
	{L'\n', {Enter}},	{L'\r', {Enter}},
	{L' ', {Space}},

	{L'0', {k0}},	{L'1', {k1}},	{L'2', {k2}},	{L'3', {k3}},	{L'4', {k4}},
	{L'5', {k5}},	{L'6', {k6}},	{L'7', {k7}},	{L'8', {k8}},	{L'9', {k9}},

	{L'A', {Shift, A}},	{L'B', {Shift, B}},	{L'C', {Shift, C}},
	{L'D', {Shift, D}},	{L'E', {Shift, E}},	{L'F', {Shift, F}},
	{L'G', {Shift, G}},	{L'H', {Shift, H}},	{L'I', {Shift, I}},
	{L'J', {Shift, J}},	{L'K', {Shift, K}},	{L'L', {Shift, L}},
	{L'M', {Shift, M}},	{L'N', {Shift, N}},	{L'O', {Shift, O}},
	{L'P', {Shift, P}},	{L'Q', {Shift, Q}},	{L'R', {Shift, R}},
	{L'S', {Shift, S}},	{L'T', {Shift, T}},	{L'U', {Shift, U}},
	{L'V', {Shift, V}},	{L'W', {Shift, W}},	{L'X', {Shift, X}},
	{L'Y', {Shift, Y}},	{L'Z', {Shift, Z}},

	{L'a', {A}},	{L'b', {B}},	{L'c', {C}},
	{L'd', {D}},	{L'e', {E}},	{L'f', {F}},
	{L'g', {G}},	{L'h', {H}},	{L'i', {I}},
	{L'j', {J}},	{L'k', {K}},	{L'l', {L}},
	{L'm', {M}},	{L'n', {N}},	{L'o', {O}},
	{L'p', {P}},	{L'q', {Q}},	{L'r', {R}},
	{L's', {S}},	{L't', {T}},	{L'u', {U}},
	{L'v', {V}},	{L'w', {W}},	{L'x', {X}},
	{L'y', {Y}},	{L'z', {Z}},

	{L'!', {Shift, k1}},
	{L'"', {Shift, k2}},
	{L'#', {Shift, k3}},
	{L'$', {Shift, k4}},
	{L'%', {Shift, k5}},
	{L'&', {Shift, k6}},
	{L'\'', {Shift, k7}},
	{L'(', {Shift, k8}},
	{L')', {Shift, k9}},
	{L'\\', {Shift, k0}},

	{L'=', {Shift, Minus}},			{L'-', {Minus}},
	{L';', {Shift, Plus}},			{L'+', {Plus}},

	{L'?', {Shift, ForwardSlash}},	{L'/', {ForwardSlash}},
	{L':', {Shift, Asterisk}},		{L'*', {Asterisk}},

	{L'<', {Shift, Comma}},			{L',', {Comma}},
	{L'>', {Shift, FullStop}},		{L'.', {FullStop}},
	{L'^', {Shift, At}},			{L'@', {At}},
};
}

namespace MO6 {
enum Key: uint16_t {
	k0	= 0x1e,		k1	= 0x2f,		k2	= 0x27,		k3 	= 0x1f,
	k4	= 0x17,		k5	= 0x0f,		k6	= 0x07,		k7 	= 0x06,
	k8	= 0x0e,		k9	= 0x16,

	A 	= 0x2d,		Z	= 0x25,		E 	= 0x1d,		R	= 0x15,
	T	= 0x0d,		Y	= 0x05,		U	= 0x04,		I	= 0x0c,
	O	= 0x14,		P	= 0x1c,		Q	= 0x2b,		S	= 0x23,
	D	= 0x1b,		F	= 0x13,		G	= 0x0b,		H	= 0x03,
	J	= 0x02,		K	= 0x0a,		L	= 0x12,		W	= 0x30,
	X	= 0x28,		C	= 0x32,		V	= 0x2a,		B	= 0x22,
	N	= 0x00,		M	= 0x1a,

	Hash			= 0x18,
	CloseBracket	= 0x50,
	Minus			= 0x26,
	Equals			= 0x2e,
	ACC				= 0x36,

	Stop			= 0x37,
	Caret			= 0x58,
	Dollar 			= 0x2c,

	OpenSquareBracket	= 0x40,
	UGrave				= 0x60,	// ù
	CloseSquareBracket	= 0x48,

	Comma				= 0x08,
	Semicolon			= 0x10,
	Colon				= 0x24,
	CloseAngleBracket	= 0x11,
	RAZ					= 0x33,
	INS					= 0x09,
	EFF					= 0x01,

	Enter		= 0x34,
	Space		= 0x20,

	Up		= 0x31,
	Down	= 0x21,
	Left	= 0x29,
	Right	= 0x19,

	F1 = 0x3b,
	F2 = 0x3c,
	F3 = 0x3d,
	F4 = 0x3e,
	F5 = 0x3f,

	ShiftLock	= 0x3a,
	BASIC		= 0x39,
	Shift		= 0x38,
	Control		= 0x35,
};

static inline std::unordered_map<wchar_t, const std::vector<uint16_t>> sequences = {
	{L'\n', {Enter}},	{L'\r', {Enter}},
	{L' ', {Space}},

	{L'0', {k0}},	{L'1', {k1}},	{L'2', {k2}},	{L'3', {k3}},	{L'4', {k4}},
	{L'5', {k5}},	{L'6', {k6}},	{L'7', {k7}},	{L'8', {k8}},	{L'9', {k9}},

	{L'A', {Shift, A}},	{L'B', {Shift, B}},	{L'C', {Shift, C}},
	{L'D', {Shift, D}},	{L'E', {Shift, E}},	{L'F', {Shift, F}},
	{L'G', {Shift, G}},	{L'H', {Shift, H}},	{L'I', {Shift, I}},
	{L'J', {Shift, J}},	{L'K', {Shift, K}},	{L'L', {Shift, L}},
	{L'M', {Shift, M}},	{L'N', {Shift, N}},	{L'O', {Shift, O}},
	{L'P', {Shift, P}},	{L'Q', {Shift, Q}},	{L'R', {Shift, R}},
	{L'S', {Shift, S}},	{L'T', {Shift, T}},	{L'U', {Shift, U}},
	{L'V', {Shift, V}},	{L'W', {Shift, W}},	{L'X', {Shift, X}},
	{L'Y', {Shift, Y}},	{L'Z', {Shift, Z}},

	{L'a', {A}},	{L'b', {B}},	{L'c', {C}},
	{L'd', {D}},	{L'e', {E}},	{L'f', {F}},
	{L'g', {G}},	{L'h', {H}},	{L'i', {I}},
	{L'j', {J}},	{L'k', {K}},	{L'l', {L}},
	{L'm', {M}},	{L'n', {N}},	{L'o', {O}},
	{L'p', {P}},	{L'q', {Q}},	{L'r', {R}},
	{L's', {S}},	{L't', {T}},	{L'u', {U}},
	{L'v', {V}},	{L'w', {W}},	{L'x', {X}},
	{L'y', {Y}},	{L'z', {Z}},

	{L'@', {Shift, Hash}},	{L'#', {Hash}},
	{L'à', {Shift, k0}},	{L'*', {Shift, k1}},	{L'é', {Shift, k2}},	{L'"', {Shift, k3}},	{L'\'', {Shift, k4}},
	{L'(', {Shift, k5}},	{L'_', {Shift, k6}},	{L'è', {Shift, k7}},	{L'!', {Shift, k8}},	{L'ç', {Shift, k9}},

	{L'º', {Shift, CloseBracket}},	{L')', {CloseBracket}},
	{L'\\', {Shift, Minus}},		{L'-', {Minus}},
	{L'=', {Shift, Equals}},		{L'+', {Equals}},

	{L'¨', {Shift, Caret}},					{L'^', {Caret}},
	{L'$', {Shift, Dollar}},				{L'&', {Dollar}},
	{L'{', {Shift, OpenSquareBracket}},		{L'[', {OpenSquareBracket}},
	{L'%', {Shift, UGrave}},				{L'ù', {UGrave}},
	{L'}', {Shift, CloseSquareBracket}},	{L']', {CloseSquareBracket}},

	{L'?', {Shift, Comma}},					{L',', {Comma}},
	{L'.', {Shift, Semicolon}},				{L';', {Semicolon}},
	{L'/', {Shift, Colon}},					{L':', {Colon}},
	{L'<', {Shift, CloseAngleBracket}},		{L'>', {CloseAngleBracket}},
};
}

namespace Prodest128 {
enum Key: uint16_t {
	k0	= 0x1e,		k1	= 0x2f,		k2	= 0x27,		k3 	= 0x1f,
	k4	= 0x17,		k5	= 0x0f,		k6	= 0x07,		k7 	= 0x06,
	k8	= 0x0e,		k9	= 0x16,

	Q 	= 0x2d,		W	= 0x25,		E 	= 0x1d,		R	= 0x15,
	T	= 0x0d,		Y	= 0x05,		U	= 0x04,		I	= 0x0c,
	O	= 0x14,		P	= 0x1c,		A	= 0x2b,		S	= 0x23,
	D	= 0x1b,		F	= 0x13,		G	= 0x0b,		H	= 0x03,
	J	= 0x02,		K	= 0x0a,		L	= 0x12,		Z	= 0x30,
	X	= 0x28,		C	= 0x32,		V	= 0x2a,		B	= 0x22,
	N	= 0x00,		M	= 0x08,		NWithLine	= 0x1a,

	OpenSquareBracket	= 0x18,
	CCedilla			= 0x50,	// ç
	Quote				= 0x26,
	CloseSquareBracket	= 0x2e,
	ACC					= 0x36,

	Stop					= 0x37,
	InvertedQuestionMark	= 0x58,
	Plus 					= 0x2c,

	CloseAngleBracket		= 0x40,
	InvertedExclamationMark	= 0x60,
	Hash					= 0x48,

	Comma	= 0x10,
	Dot		= 0x24,
	Minus	= 0x11,
	RAZ		= 0x33,
	INS		= 0x09,
	EFF		= 0x01,

	Enter	= 0x34,
	Space	= 0x20,

	Up		= 0x31,
	Down	= 0x21,
	Left	= 0x29,
	Right	= 0x19,

	F1 = 0x3b,
	F2 = 0x3c,
	F3 = 0x3d,
	F4 = 0x3e,
	F5 = 0x3f,

	ShiftLock	= 0x3a,
	BASIC		= 0x39,
	Shift		= 0x38,
	Control		= 0x35,
};

static inline std::unordered_map<wchar_t, const std::vector<uint16_t>> sequences = {
	{L'\n', {Enter}},	{L'\r', {Enter}},
	{L' ', {Space}},

	{L'0', {k0}},	{L'1', {k1}},	{L'2', {k2}},	{L'3', {k3}},	{L'4', {k4}},
	{L'5', {k5}},	{L'6', {k6}},	{L'7', {k7}},	{L'8', {k8}},	{L'9', {k9}},

	{L'A', {Shift, A}},	{L'B', {Shift, B}},	{L'C', {Shift, C}},
	{L'D', {Shift, D}},	{L'E', {Shift, E}},	{L'F', {Shift, F}},
	{L'G', {Shift, G}},	{L'H', {Shift, H}},	{L'I', {Shift, I}},
	{L'J', {Shift, J}},	{L'K', {Shift, K}},	{L'L', {Shift, L}},
	{L'M', {Shift, M}},	{L'N', {Shift, N}},	{L'O', {Shift, O}},
	{L'P', {Shift, P}},	{L'Q', {Shift, Q}},	{L'R', {Shift, R}},
	{L'S', {Shift, S}},	{L'T', {Shift, T}},	{L'U', {Shift, U}},
	{L'V', {Shift, V}},	{L'W', {Shift, W}},	{L'X', {Shift, X}},
	{L'Y', {Shift, Y}},	{L'Z', {Shift, Z}},

	{L'a', {A}},	{L'b', {B}},	{L'c', {C}},
	{L'd', {D}},	{L'e', {E}},	{L'f', {F}},
	{L'g', {G}},	{L'h', {H}},	{L'i', {I}},
	{L'j', {J}},	{L'k', {K}},	{L'l', {L}},
	{L'm', {M}},	{L'n', {N}},	{L'o', {O}},
	{L'p', {P}},	{L'q', {Q}},	{L'r', {R}},
	{L's', {S}},	{L't', {T}},	{L'u', {U}},
	{L'v', {V}},	{L'w', {W}},	{L'x', {X}},
	{L'y', {Y}},	{L'z', {Z}},

	{L'{', {Shift, OpenSquareBracket}},	{L'[', {OpenSquareBracket}},
	{L'=', {Shift, k0}},	{L'!', {Shift, k1}},	{L'\"', {Shift, k2}},	{L'§', {Shift, k3}},	{L'$', {Shift, k4}},
	{L'%', {Shift, k5}},	{L'&', {Shift, k6}},	{L'/', {Shift, k7}},	{L'(', {Shift, k8}},	{L')', {Shift, k9}},

	{L'?', {Shift, CCedilla}},	{L'ç', {CCedilla}},
	{L'£', {Shift, Quote}},		{L'\'', {Quote}},		{L'`', {RAZ, Quote}},
	{L'}', {Shift, CloseSquareBracket}},				{L']', {CloseSquareBracket}},

	{L'\\', {Shift, Stop}},

	{L'@', {Shift, InvertedQuestionMark}},	{L'¿', {InvertedQuestionMark}},
	{L'*', {Shift, Plus}},					{L'+', {Plus}},

	{L'<', {Shift, CloseAngleBracket}},			{L'>', {CloseAngleBracket}},
	{L'¨', {Shift, InvertedExclamationMark}},	{L'¡', {InvertedExclamationMark}},
	{L'^', {Shift, Hash}},						{L'#', {Hash}},

	{L';', {Shift, Comma}},			{L',', {Comma}},
	{L':', {Shift, Dot}},			{L'.', {Dot}},
	{L'_', {Shift, Minus}},			{L'-', {Minus}},
};
}

struct KeyboardMapper: public MachineTypes::MappedKeyboardMachine::KeyboardMapper {
	KeyboardMapper(const Machine machine) : machine_(machine) {}

	uint16_t mapped_key_for_key(const Inputs::Keyboard::Key key) const {
		using In = Inputs::Keyboard::Key;

		switch(machine_) {
			case Machine::MO5:
				switch(key) {
					default:	return MachineTypes::MappedKeyboardMachine::KeyNotMapped;

					using enum MO5::Key;
					case In::k0:	return k0;		case In::k1:	return k1;
					case In::k2:	return k2;		case In::k3:	return k3;
					case In::k4:	return k4;		case In::k5:	return k5;
					case In::k6:	return k6;		case In::k7:	return k7;
					case In::k8:	return k8;		case In::k9:	return k9;

					case In::Q:		return A;		case In::W:		return Z;
					case In::E:		return E;		case In::R:		return R;
					case In::T:		return T;		case In::Y:		return Y;
					case In::U:		return U;		case In::I:		return I;
					case In::O:		return O;		case In::P:		return P;
					case In::A:		return Q;		case In::S:		return S;
					case In::D:		return D;		case In::F:		return F;
					case In::G:		return G;		case In::H:		return H;
					case In::J:		return J;		case In::K:		return K;
					case In::L:		return L;		case In::Z:		return W;
					case In::X:		return X;		case In::C:		return C;
					case In::V:		return V;		case In::B:		return B;
					case In::N:		return N;		case In::M:		return M;

					case In::FullStop:	return FullStop;
					case In::Comma:		return Comma;
					case In::Hyphen:	return Minus;
					case In::Equals:	return Plus;

					case In::OpenSquareBracket:	return At;
					case In::Semicolon:			return M;
					case In::ForwardSlash:		return ForwardSlash;

					case In::CloseSquareBracket:
					case In::Quote:		return Asterisk;

					case In::Space:		return Space;
					case In::Enter:		return Enter;
					case In::Backspace:	return ACC;
					case In::Escape:	return Stop;

					case In::Up:		return Up;		case In::Down:	return Down;
					case In::Left:		return Left;	case In::Right:	return Right;

					case In::LeftShift:
					case In::RightShift:	return Shift;
					case In::Tab:
					case In::LeftControl:
					case In::RightControl:	return Control;
					case In::LeftOption:
					case In::RightOption:	return BASIC;
					case In::LeftMeta:
					case In::RightMeta:		return RAZ;

					case In::Insert:		return INS;
					case In::Home:			return EFF;
				}
			break;

			case Machine::MO6:
				switch(key) {
					default:	return MachineTypes::MappedKeyboardMachine::KeyNotMapped;

					using enum MO6::Key;
					case In::k0:	return k0;		case In::k1:	return k1;
					case In::k2:	return k2;		case In::k3:	return k3;
					case In::k4:	return k4;		case In::k5:	return k5;
					case In::k6:	return k6;		case In::k7:	return k7;
					case In::k8:	return k8;		case In::k9:	return k9;

					case In::Q:		return A;		case In::W:		return Z;
					case In::E:		return E;		case In::R:		return R;
					case In::T:		return T;		case In::Y:		return Y;
					case In::U:		return U;		case In::I:		return I;
					case In::O:		return O;		case In::P:		return P;
					case In::A:		return Q;		case In::S:		return S;
					case In::D:		return D;		case In::F:		return F;
					case In::G:		return G;		case In::H:		return H;
					case In::J:		return J;		case In::K:		return K;
					case In::L:		return L;		case In::Z:		return W;
					case In::X:		return X;		case In::C:		return C;
					case In::V:		return V;		case In::B:		return B;
					case In::N:		return N;		case In::M:		return M;

					case In::Space:		return Space;
					case In::Enter:		return Enter;

					case In::BackTick:	return Hash;
					case In::Backslash:	return CloseBracket;
					case In::Hyphen:	return Minus;
					case In::Equals:	return Equals;

					case In::OpenSquareBracket:		return Caret;
					case In::CloseSquareBracket:	return Dollar;

					// TODO: locate OpenSquareBracket somewhere.
					case In::Semicolon:		return UGrave;
					case In::Quote:			return CloseSquareBracket;

					case In::Comma:			return Comma;
					case In::FullStop:		return Semicolon;
					case In::ForwardSlash:	return Colon;
					// TODO: locate CloseAngleBracket.

					case In::Up:		return Up;		case In::Down:	return Down;
					case In::Left:		return Left;	case In::Right:	return Right;

					case In::Backspace:		return ACC;
					case In::Insert:		return INS;
					case In::Delete:		return EFF;

					case In::Tab:			return Stop;

					case In::LeftShift:
					case In::RightShift:	return Shift;
					case In::LeftControl:
					case In::RightControl:	return Control;
					case In::LeftOption:
					case In::RightOption:	return BASIC;
					case In::LeftMeta:
					case In::RightMeta:		return RAZ;
					case In::CapsLock:		return ShiftLock;

					case In::F1:	return F1;
					case In::F2:	return F2;
					case In::F3:	return F3;
					case In::F4:	return F4;
					case In::F5:	return F5;
				}
			break;

			case Machine::Prodest128:
				switch(key) {
					default:	return MachineTypes::MappedKeyboardMachine::KeyNotMapped;

					using enum Prodest128::Key;
					case In::k0:	return k0;		case In::k1:	return k1;
					case In::k2:	return k2;		case In::k3:	return k3;
					case In::k4:	return k4;		case In::k5:	return k5;
					case In::k6:	return k6;		case In::k7:	return k7;
					case In::k8:	return k8;		case In::k9:	return k9;

					case In::Q:		return Q;		case In::W:		return W;
					case In::E:		return E;		case In::R:		return R;
					case In::T:		return T;		case In::Y:		return Y;
					case In::U:		return U;		case In::I:		return I;
					case In::O:		return O;		case In::P:		return P;
					case In::A:		return A;		case In::S:		return S;
					case In::D:		return D;		case In::F:		return F;
					case In::G:		return G;		case In::H:		return H;
					case In::J:		return J;		case In::K:		return K;
					case In::L:		return L;		case In::Z:		return Z;
					case In::X:		return X;		case In::C:		return C;
					case In::V:		return V;		case In::B:		return B;
					case In::N:		return N;		case In::M:		return M;

					case In::Space:		return Space;
					case In::Enter:		return Enter;

					case In::BackTick:	return OpenSquareBracket;
					case In::Hyphen:	return CCedilla;
					case In::Equals:	return Quote;
					case In::Backslash:	return CloseSquareBracket;

					case In::Tab:		return Stop;

					case In::OpenSquareBracket:		return InvertedQuestionMark;
					case In::CloseSquareBracket:	return Plus;

					case In::Semicolon:		return NWithLine;
					case In::Quote:			return InvertedExclamationMark;

					case In::Comma:			return Comma;
					case In::FullStop:		return Dot;
					case In::ForwardSlash:	return Minus;

					case In::Up:		return Up;		case In::Down:	return Down;
					case In::Left:		return Left;	case In::Right:	return Right;

					case In::Backspace:		return ACC;
					case In::Insert:		return INS;
					case In::Delete:		return EFF;

					case In::LeftShift:
					case In::RightShift:	return Shift;
					case In::LeftControl:
					case In::RightControl:	return Control;
					case In::LeftOption:
					case In::RightOption:	return BASIC;
					case In::LeftMeta:
					case In::RightMeta:		return RAZ;
					case In::CapsLock:		return ShiftLock;

					case In::F1:	return F1;
					case In::F2:	return F2;
					case In::F3:	return F3;
					case In::F4:	return F4;
					case In::F5:	return F5;
				}
			break;

			default: __builtin_unreachable();
		}
	}

private:
	Machine machine_;
};

struct CharacterMapper: public ::Utility::CharacterMapper {
	CharacterMapper(const Machine machine) : machine_(machine) {}

	std::span<const uint16_t> sequence_for_character(const wchar_t character) const final {
		const auto sequences = [&]() -> const std::unordered_map<wchar_t, const std::vector<uint16_t>> & {
			switch(machine_) {
				case Machine::MO5:			return MO5::sequences;
				case Machine::MO6:			return MO6::sequences;
				case Machine::Prodest128:	return Prodest128::sequences;
				default: __builtin_unreachable();
			}
		};
		return lookup_sequence(sequences(), character);
	}

private:
	Machine machine_;
};

}
