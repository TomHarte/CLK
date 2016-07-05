//
//  CSVic20.m
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import "CSVic20.h"

#include "Vic20.hpp"
#include "CommodoreTAP.hpp"

@implementation CSVic20 {
	Vic20::Machine _vic20;
	BOOL _joystickMode;
}

- (CRTMachine::Machine * const)machine {
	return &_vic20;
}

- (void)setROM:(nonnull NSData *)rom slot:(Vic20::ROMSlot)slot {
	@synchronized(self) {
		_vic20.set_rom(slot, rom.length, (const uint8_t *)rom.bytes);
	}
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

- (BOOL)openTAPAtURL:(NSURL *)URL {
	@synchronized(self) {
		try {
			std::shared_ptr<Storage::CommodoreTAP> tape(new Storage::CommodoreTAP([URL fileSystemRepresentation]));
			_vic20.set_tape(tape);
			return YES;
		} catch(int exception) {
			return NO;
		}
	}
}


- (void)setPRG:(nonnull NSData *)prg {
	@synchronized(self) {
		_vic20.add_prg(prg.length, (const uint8_t *)prg.bytes);
	}
}

- (void)setKey:(uint16_t)key isPressed:(BOOL)isPressed {
	static NSDictionary<NSNumber *, NSNumber *> *vicKeysByKeys = @{
		@(VK_ANSI_1):	@(Vic20::Key::Key1),	@(VK_ANSI_2):	@(Vic20::Key::Key2),
		@(VK_ANSI_3):	@(Vic20::Key::Key3),	@(VK_ANSI_4):	@(Vic20::Key::Key4),
		@(VK_ANSI_5):	@(Vic20::Key::Key5),	@(VK_ANSI_6):	@(Vic20::Key::Key6),
		@(VK_ANSI_7):	@(Vic20::Key::Key7),	@(VK_ANSI_8):	@(Vic20::Key::Key8),
		@(VK_ANSI_9):	@(Vic20::Key::Key9),	@(VK_ANSI_0):	@(Vic20::Key::Key0),

		@(VK_ANSI_Q):	@(Vic20::Key::KeyQ),	@(VK_ANSI_W):	@(Vic20::Key::KeyW),
		@(VK_ANSI_E):	@(Vic20::Key::KeyE),	@(VK_ANSI_R):	@(Vic20::Key::KeyR),
		@(VK_ANSI_T):	@(Vic20::Key::KeyT),	@(VK_ANSI_Y):	@(Vic20::Key::KeyY),
		@(VK_ANSI_U):	@(Vic20::Key::KeyU),	@(VK_ANSI_I):	@(Vic20::Key::KeyI),
		@(VK_ANSI_O):	@(Vic20::Key::KeyO),	@(VK_ANSI_P):	@(Vic20::Key::KeyP),
		@(VK_ANSI_A):	@(Vic20::Key::KeyA),	@(VK_ANSI_S):	@(Vic20::Key::KeyS),
		@(VK_ANSI_D):	@(Vic20::Key::KeyD),	@(VK_ANSI_F):	@(Vic20::Key::KeyF),
		@(VK_ANSI_G):	@(Vic20::Key::KeyG),	@(VK_ANSI_H):	@(Vic20::Key::KeyH),
		@(VK_ANSI_J):	@(Vic20::Key::KeyJ),	@(VK_ANSI_K):	@(Vic20::Key::KeyK),
		@(VK_ANSI_L):	@(Vic20::Key::KeyL),	@(VK_ANSI_Z):	@(Vic20::Key::KeyZ),
		@(VK_ANSI_X):	@(Vic20::Key::KeyX),	@(VK_ANSI_C):	@(Vic20::Key::KeyC),
		@(VK_ANSI_V):	@(Vic20::Key::KeyV),	@(VK_ANSI_B):	@(Vic20::Key::KeyB),
		@(VK_ANSI_N):	@(Vic20::Key::KeyN),	@(VK_ANSI_M):	@(Vic20::Key::KeyM),

		@(VK_Space):			@(Vic20::Key::KeySpace),
		@(VK_Return):			@(Vic20::Key::KeyReturn),
		@(VK_Delete):			@(Vic20::Key::KeyDelete),
		@(VK_ANSI_Comma):		@(Vic20::Key::KeyComma),
		@(VK_ANSI_Period):		@(Vic20::Key::KeyFullStop),
		@(VK_ANSI_Minus):		@(Vic20::Key::KeyDash),
		@(VK_ANSI_Equal):		@(Vic20::Key::KeyEquals),
		@(VK_ANSI_Semicolon):	@(Vic20::Key::KeyColon),
		@(VK_ANSI_Quote):		@(Vic20::Key::KeySemicolon),
		@(VK_ANSI_Slash):		@(Vic20::Key::KeySlash),
		@(VK_Option):			@(Vic20::Key::KeyCBM),
		@(VK_Control):			@(Vic20::Key::KeyControl),

		@(VK_F1):	@(Vic20::Key::KeyF1),	@(VK_F3):	@(Vic20::Key::KeyF3),
		@(VK_F5):	@(Vic20::Key::KeyF5),	@(VK_F7):	@(Vic20::Key::KeyF7),

		@(VK_ANSI_Grave):			@(Vic20::Key::KeyLeft),
		@(VK_Tab):					@(Vic20::Key::KeyRunStop),
		@(VK_ANSI_LeftBracket):		@(Vic20::Key::KeyAt),
		@(VK_ANSI_RightBracket):	@(Vic20::Key::KeyAsterisk),
		@(VK_ANSI_Backslash):		@(Vic20::Key::KeyUp),

		@(VK_RightArrow):			@(Vic20::Key::KeyRight),
		@(VK_DownArrow):			@(Vic20::Key::KeyDown),
	};

	// Not yet mapped:
	//	KeyHome
	//	KeyPlus
	//	KeyGBP

	if(key == VK_Tab && isPressed)
	{
		_joystickMode ^= YES;
	}

	@synchronized(self) {
		if(_joystickMode)
		{
			switch(key)
			{
				case VK_UpArrow:	_vic20.set_joystick_state(Vic20::JoystickInput::Up, isPressed);		break;
				case VK_DownArrow:	_vic20.set_joystick_state(Vic20::JoystickInput::Down, isPressed);	break;
				case VK_LeftArrow:	_vic20.set_joystick_state(Vic20::JoystickInput::Left, isPressed);	break;
				case VK_RightArrow:	_vic20.set_joystick_state(Vic20::JoystickInput::Right, isPressed);	break;
				case VK_ANSI_A:		_vic20.set_joystick_state(Vic20::JoystickInput::Fire, isPressed);	break;
			}
		}
		else
		{
			switch(key)
			{
				default: {
					NSNumber *targetKey = vicKeysByKeys[@(key)];
					if(targetKey)
					{
						_vic20.set_key_state((Vic20::Key)targetKey.integerValue, isPressed);
					}
					else
						NSLog(@"Unmapped: %02x", key);
				} break;

				case VK_Shift:
					// Yuck
					_vic20.set_key_state(Vic20::Key::KeyLShift, isPressed);
					_vic20.set_key_state(Vic20::Key::KeyRShift, isPressed);
				break;
			}
		}
	}
}

- (void)clearAllKeys {
	@synchronized(self) {
		_vic20.clear_all_keys();
	}
}

- (void)setUseFastLoadingHack:(BOOL)useFastLoadingHack {
	@synchronized(self) {
		_useFastLoadingHack = useFastLoadingHack;
		_vic20.set_use_fast_tape_hack(useFastLoadingHack ? true : false);
	}
}

@end
