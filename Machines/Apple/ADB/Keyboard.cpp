//
//  Keyboard.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 13/02/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Keyboard.hpp"

using namespace Apple::ADB;

Keyboard::Keyboard(Bus &bus) : ReactiveDevice(bus, 2) {}

void Keyboard::perform_command(const Command &command) {
	switch(command.type) {
		case Command::Type::Reset:
			modifiers_ = 0xffff;
		case Command::Type::Flush: {
			std::lock_guard lock_guard(keys_mutex_);
			pending_events_.clear();
		} break;

		case Command::Type::Talk:
			switch(command.reg) {
				case 0: {
					// Post up to two key events, or nothing if there are
					// no events pending.
					std::lock_guard lock_guard(keys_mutex_);

					if(!pending_events_.empty()) {
						if(pending_events_.size() > 1) {
							post_response({pending_events_[0], pending_events_[1]});
							pending_events_.erase(pending_events_.begin(), pending_events_.begin()+2);
						} else {
							// Two bytes are required; provide a key up of the fictional
							// key zero as the second.
							// That's arbitrary; verify with real machines.
							post_response({pending_events_[0], 0x80});
							pending_events_.clear();
						}
					}
				} break;

				case 2: {
					std::lock_guard lock_guard(keys_mutex_);
					post_response({uint8_t(modifiers_ >> 8), uint8_t(modifiers_)});
				} break;

				default: break;
			}
		break;

		case Command::Type::Listen:
			// If a listen is incoming for register 2, prepare to capture LED statuses.
			if(command.reg == 2) {
				receive_bytes(2);
			}
		break;


		default: break;
	}
}

void Keyboard::did_receive_data(const Command &, const std::vector<uint8_t> &data) {
	// This must be a register 2 listen; update the LED statuses.
	// TODO: and possibly display these.
	modifiers_ = (modifiers_ & 0xfff8) | (data[1] & 7);
}


bool Keyboard::set_key_pressed(Key key, bool is_pressed) {
	// ADB keyboard events: low 7 bits are a key code; bit 7 is either 0 for pressed or 1 for released.
	std::lock_guard lock_guard(keys_mutex_);
	pending_events_.push_back(uint8_t(key) | (is_pressed ? 0x00 : 0x80));
	pressed_keys_[size_t(key)] = is_pressed;

	// Track modifier state also.

	/*
		In all cases below: 0 = pressed/on; 1 = released/off.

		b15:	None (reserved)
		b14:	Delete
		b13:	Caps lock
		b12:	Reset
		b11:	Control
		b10:	Shift
		b9:		Option
		b8:		Command

		-- From here onwards, available only on the extended keyboard.

		b7:		Num lock/clear
		b6:		Scroll lock
		b5–3:	None (reserved)
		b2:		Scroll Lock LED
		b1:		Caps Lock LED
		b0:		Num Lock LED
	*/

#define SetModifierBit(x)		modifiers_ = (modifiers_ & ~x) | (is_pressed ? 0 : x);
#define ToggleModifierBit(x)	if(is_pressed) modifiers_ ^= x;
	switch(key) {
		default: break;
		case Key::Delete:	SetModifierBit(0x4000);		break;
		case Key::CapsLock:	ToggleModifierBit(0x2000);	break;
		case Key::Power:	SetModifierBit(0x1000);		break;

		case Key::LeftControl:
		case Key::RightControl:
			SetModifierBit(0x0800);
		break;

		case Key::LeftShift:
		case Key::RightShift:
			SetModifierBit(0x0400);
		break;

		case Key::LeftOption:
		case Key::RightOption:
			SetModifierBit(0x0200);
		break;

		case Key::Command:
			SetModifierBit(0x0100);
		break;

		case Key::KeypadClear:	ToggleModifierBit(0x0080);	break;
		case Key::Help:			ToggleModifierBit(0x0040);	break;
	}
#undef SetModifierBit

	return true;
}

void Keyboard::clear_all_keys() {
	// For all keys currently marked as down, enqueue key-up actions.
	std::lock_guard lock_guard(keys_mutex_);
	for(size_t key = 0; key < pressed_keys_.size(); key++) {
		if(pressed_keys_[key]) {
			pending_events_.push_back(0x80 | uint8_t(key));
			pressed_keys_[key] = false;
		}
	}

	// Mark all modifiers as released.
	modifiers_ |= 0xfff8;
}

// MARK: - KeyboardMapper

uint16_t KeyboardMapper::mapped_key_for_key(Inputs::Keyboard::Key key) const {
	using Key = Inputs::Keyboard::Key;
	using ADBKey = Apple::ADB::Key;
	switch(key) {
		default: return MachineTypes::MappedKeyboardMachine::KeyNotMapped;

#define Bind(x, y) 		case Key::x: return uint16_t(ADBKey::y)
#define BindDirect(x)	Bind(x, x)

		BindDirect(BackTick);
		BindDirect(k1);	BindDirect(k2);	BindDirect(k3);	BindDirect(k4);	BindDirect(k5);
		BindDirect(k6);	BindDirect(k7);	BindDirect(k8);	BindDirect(k9);	BindDirect(k0);

		BindDirect(Help);
		BindDirect(Home);
		BindDirect(PageUp);
		BindDirect(Delete);
		BindDirect(End);
		BindDirect(PageDown);

		BindDirect(Escape);
		BindDirect(Hyphen);
		BindDirect(Equals);
		BindDirect(Backspace);
		BindDirect(Tab);

		BindDirect(F1);		BindDirect(F2);		BindDirect(F3);		BindDirect(F4);
		BindDirect(F5);		BindDirect(F6);		BindDirect(F7);		BindDirect(F8);
		BindDirect(F9);		BindDirect(F10);	BindDirect(F11);	BindDirect(F12);

		BindDirect(Q);	BindDirect(W);	BindDirect(E);	BindDirect(R);
		BindDirect(T);	BindDirect(Y);	BindDirect(U);	BindDirect(I);
		BindDirect(O);	BindDirect(P);	BindDirect(A);	BindDirect(S);
		BindDirect(D);	BindDirect(F);	BindDirect(G);	BindDirect(H);
		BindDirect(J);	BindDirect(K);	BindDirect(L);	BindDirect(Z);
		BindDirect(X);	BindDirect(C);	BindDirect(V);	BindDirect(B);
		BindDirect(N);	BindDirect(M);

		BindDirect(OpenSquareBracket);
		BindDirect(CloseSquareBracket);
		BindDirect(Semicolon);
		BindDirect(Quote);
		BindDirect(Comma);
		BindDirect(FullStop);
		BindDirect(ForwardSlash);

		BindDirect(CapsLock);
		BindDirect(LeftShift);		BindDirect(RightShift);
		BindDirect(LeftControl);	BindDirect(RightControl);
		BindDirect(LeftOption);		BindDirect(RightOption);
		Bind(LeftMeta, Command);	Bind(RightMeta, Command);

		BindDirect(Space);
		BindDirect(Backslash);
		Bind(Enter, Return);

		BindDirect(Left);	BindDirect(Right);
		BindDirect(Up);		BindDirect(Down);

		Bind(KeypadDelete, KeypadClear);
		BindDirect(KeypadEquals);
		BindDirect(KeypadSlash);
		BindDirect(KeypadAsterisk);
		BindDirect(KeypadMinus);
		BindDirect(KeypadPlus);
		BindDirect(KeypadEnter);
		BindDirect(KeypadDecimalPoint);

		BindDirect(Keypad9);
		BindDirect(Keypad8);
		BindDirect(Keypad7);
		BindDirect(Keypad6);
		BindDirect(Keypad5);
		BindDirect(Keypad4);
		BindDirect(Keypad3);
		BindDirect(Keypad2);
		BindDirect(Keypad1);
		BindDirect(Keypad0);

		// Leaving unmapped:
		//	Power, F13, F14, F15

#undef BindDirect
#undef Bind
	}
}
