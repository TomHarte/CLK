//
//  CSVic20.m
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import "CSVic20.h"

#import "Vic20.hpp"

@implementation CSVic20 {
	Vic20::Machine _vic20;
}

- (CRTMachine::Machine * const)machine {
	return &_vic20;
}

- (void)setROM:(nonnull NSData *)rom slot:(Vic20::ROMSlot)slot {
	_vic20.set_rom(slot, rom.length, (const uint8_t *)rom.bytes);
}

- (void)setKernelROM:(nonnull NSData *)rom {
	[self setROM:rom slot:Vic20::ROMSlotKernel];
}

- (void)setBASICROM:(nonnull NSData *)rom {
	[self setROM:rom slot:Vic20::ROMSlotBASIC];
}

- (void)setCharactersROM:(nonnull NSData *)rom {
	[self setROM:rom slot:Vic20::ROMSlotCharacters];
}

- (void)setPRG:(nonnull NSData *)prg {
	_vic20.add_prg(prg.length, (const uint8_t *)prg.bytes);
}

- (void)setKey:(uint16_t)key isPressed:(BOOL)isPressed {
	@synchronized(self) {
		switch(key)
		{
			case VK_ANSI_1:		_vic20.set_key_state(Vic20::Key::Key1, isPressed);	break;
			case VK_ANSI_2:		_vic20.set_key_state(Vic20::Key::Key2, isPressed);	break;
			case VK_ANSI_3:		_vic20.set_key_state(Vic20::Key::Key3, isPressed);	break;
			case VK_ANSI_4:		_vic20.set_key_state(Vic20::Key::Key4, isPressed);	break;
			case VK_ANSI_5:		_vic20.set_key_state(Vic20::Key::Key5, isPressed);	break;
			case VK_ANSI_6:		_vic20.set_key_state(Vic20::Key::Key6, isPressed);	break;
			case VK_ANSI_7:		_vic20.set_key_state(Vic20::Key::Key7, isPressed);	break;
			case VK_ANSI_8:		_vic20.set_key_state(Vic20::Key::Key8, isPressed);	break;
			case VK_ANSI_9:		_vic20.set_key_state(Vic20::Key::Key9, isPressed);	break;
			case VK_ANSI_0:		_vic20.set_key_state(Vic20::Key::Key0, isPressed);	break;

			case VK_ANSI_Q:		_vic20.set_key_state(Vic20::Key::KeyQ, isPressed);	break;
			case VK_ANSI_W:		_vic20.set_key_state(Vic20::Key::KeyW, isPressed);	break;
			case VK_ANSI_E:		_vic20.set_key_state(Vic20::Key::KeyE, isPressed);	break;
			case VK_ANSI_R:		_vic20.set_key_state(Vic20::Key::KeyR, isPressed);	break;
			case VK_ANSI_T:		_vic20.set_key_state(Vic20::Key::KeyT, isPressed);	break;
			case VK_ANSI_Y:		_vic20.set_key_state(Vic20::Key::KeyY, isPressed);	break;
			case VK_ANSI_U:		_vic20.set_key_state(Vic20::Key::KeyU, isPressed);	break;
			case VK_ANSI_I:		_vic20.set_key_state(Vic20::Key::KeyI, isPressed);	break;
			case VK_ANSI_O:		_vic20.set_key_state(Vic20::Key::KeyO, isPressed);	break;
			case VK_ANSI_P:		_vic20.set_key_state(Vic20::Key::KeyP, isPressed);	break;

			case VK_ANSI_A:		_vic20.set_key_state(Vic20::Key::KeyA, isPressed);	break;
			case VK_ANSI_S:		_vic20.set_key_state(Vic20::Key::KeyS, isPressed);	break;
			case VK_ANSI_D:		_vic20.set_key_state(Vic20::Key::KeyD, isPressed);	break;
			case VK_ANSI_F:		_vic20.set_key_state(Vic20::Key::KeyF, isPressed);	break;
			case VK_ANSI_G:		_vic20.set_key_state(Vic20::Key::KeyG, isPressed);	break;
			case VK_ANSI_H:		_vic20.set_key_state(Vic20::Key::KeyH, isPressed);	break;
			case VK_ANSI_J:		_vic20.set_key_state(Vic20::Key::KeyI, isPressed);	break;
			case VK_ANSI_K:		_vic20.set_key_state(Vic20::Key::KeyO, isPressed);	break;
			case VK_ANSI_L:		_vic20.set_key_state(Vic20::Key::KeyP, isPressed);	break;

			case VK_ANSI_Z:		_vic20.set_key_state(Vic20::Key::KeyZ, isPressed);	break;
			case VK_ANSI_X:		_vic20.set_key_state(Vic20::Key::KeyX, isPressed);	break;
			case VK_ANSI_C:		_vic20.set_key_state(Vic20::Key::KeyC, isPressed);	break;
			case VK_ANSI_V:		_vic20.set_key_state(Vic20::Key::KeyV, isPressed);	break;
			case VK_ANSI_B:		_vic20.set_key_state(Vic20::Key::KeyB, isPressed);	break;
			case VK_ANSI_N:		_vic20.set_key_state(Vic20::Key::KeyN, isPressed);	break;
			case VK_ANSI_M:		_vic20.set_key_state(Vic20::Key::KeyM, isPressed);	break;
		}
	}
}

- (void)clearAllKeys {
}

@end
