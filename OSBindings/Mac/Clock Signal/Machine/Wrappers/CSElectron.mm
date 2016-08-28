//
//  CSElectron.m
//  Clock Signal
//
//  Created by Thomas Harte on 04/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import "CSElectron.h"

#include "Electron.hpp"
#import "CSMachine+Subclassing.h"
#include "StaticAnalyser.hpp"
#include "TapeUEF.hpp"

@implementation CSElectron {
	Electron::Machine _electron;
}

- (CRTMachine::Machine * const)machine {
	return &_electron;
}

- (void)analyse:(NSURL *)url {
	StaticAnalyser::GetTargets([url fileSystemRepresentation]);
}

- (void)setOSROM:(nonnull NSData *)rom {
	@synchronized(self) {
		_electron.set_rom(Electron::ROMSlotOS, rom.length, (const uint8_t *)rom.bytes);
	}
}

- (void)setBASICROM:(nonnull NSData *)rom {
	@synchronized(self) {
		_electron.set_rom(Electron::ROMSlotBASIC, rom.length, (const uint8_t *)rom.bytes);
	}
}

- (void)setROM:(nonnull NSData *)rom slot:(int)slot {
	@synchronized(self) {
		_electron.set_rom((Electron::ROMSlot)slot, rom.length, (const uint8_t *)rom.bytes);
	}
}

- (BOOL)openUEFAtURL:(NSURL *)URL {
	@synchronized(self) {
		try {
			std::shared_ptr<Storage::Tape::UEF> tape(new Storage::Tape::UEF([URL fileSystemRepresentation]));
			_electron.set_tape(tape);
			return YES;
		} catch(...) {
			return NO;
		}
	}
}

- (void)clearAllKeys {
	@synchronized(self) {
		_electron.clear_all_keys();
	}
}

- (void)setKey:(uint16_t)key isPressed:(BOOL)isPressed {
	@synchronized(self) {
		switch(key)
		{
			case VK_ANSI_0:		_electron.set_key_state(Electron::Key::Key0, isPressed);	break;
			case VK_ANSI_1:		_electron.set_key_state(Electron::Key::Key1, isPressed);	break;
			case VK_ANSI_2:		_electron.set_key_state(Electron::Key::Key2, isPressed);	break;
			case VK_ANSI_3:		_electron.set_key_state(Electron::Key::Key3, isPressed);	break;
			case VK_ANSI_4:		_electron.set_key_state(Electron::Key::Key4, isPressed);	break;
			case VK_ANSI_5:		_electron.set_key_state(Electron::Key::Key5, isPressed);	break;
			case VK_ANSI_6:		_electron.set_key_state(Electron::Key::Key6, isPressed);	break;
			case VK_ANSI_7:		_electron.set_key_state(Electron::Key::Key7, isPressed);	break;
			case VK_ANSI_8:		_electron.set_key_state(Electron::Key::Key8, isPressed);	break;
			case VK_ANSI_9:		_electron.set_key_state(Electron::Key::Key9, isPressed);	break;

			case VK_ANSI_Q:		_electron.set_key_state(Electron::Key::KeyQ, isPressed);	break;
			case VK_ANSI_W:		_electron.set_key_state(Electron::Key::KeyW, isPressed);	break;
			case VK_ANSI_E:		_electron.set_key_state(Electron::Key::KeyE, isPressed);	break;
			case VK_ANSI_R:		_electron.set_key_state(Electron::Key::KeyR, isPressed);	break;
			case VK_ANSI_T:		_electron.set_key_state(Electron::Key::KeyT, isPressed);	break;
			case VK_ANSI_Y:		_electron.set_key_state(Electron::Key::KeyY, isPressed);	break;
			case VK_ANSI_U:		_electron.set_key_state(Electron::Key::KeyU, isPressed);	break;
			case VK_ANSI_I:		_electron.set_key_state(Electron::Key::KeyI, isPressed);	break;
			case VK_ANSI_O:		_electron.set_key_state(Electron::Key::KeyO, isPressed);	break;
			case VK_ANSI_P:		_electron.set_key_state(Electron::Key::KeyP, isPressed);	break;
			case VK_ANSI_A:		_electron.set_key_state(Electron::Key::KeyA, isPressed);	break;
			case VK_ANSI_S:		_electron.set_key_state(Electron::Key::KeyS, isPressed);	break;
			case VK_ANSI_D:		_electron.set_key_state(Electron::Key::KeyD, isPressed);	break;
			case VK_ANSI_F:		_electron.set_key_state(Electron::Key::KeyF, isPressed);	break;
			case VK_ANSI_G:		_electron.set_key_state(Electron::Key::KeyG, isPressed);	break;
			case VK_ANSI_H:		_electron.set_key_state(Electron::Key::KeyH, isPressed);	break;
			case VK_ANSI_J:		_electron.set_key_state(Electron::Key::KeyJ, isPressed);	break;
			case VK_ANSI_K:		_electron.set_key_state(Electron::Key::KeyK, isPressed);	break;
			case VK_ANSI_L:		_electron.set_key_state(Electron::Key::KeyL, isPressed);	break;
			case VK_ANSI_Z:		_electron.set_key_state(Electron::Key::KeyZ, isPressed);	break;
			case VK_ANSI_X:		_electron.set_key_state(Electron::Key::KeyX, isPressed);	break;
			case VK_ANSI_C:		_electron.set_key_state(Electron::Key::KeyC, isPressed);	break;
			case VK_ANSI_V:		_electron.set_key_state(Electron::Key::KeyV, isPressed);	break;
			case VK_ANSI_B:		_electron.set_key_state(Electron::Key::KeyB, isPressed);	break;
			case VK_ANSI_N:		_electron.set_key_state(Electron::Key::KeyN, isPressed);	break;
			case VK_ANSI_M:		_electron.set_key_state(Electron::Key::KeyM, isPressed);	break;

			case VK_Space:			_electron.set_key_state(Electron::Key::KeySpace, isPressed);		break;
			case VK_ANSI_Grave:
			case VK_ANSI_Backslash:
									_electron.set_key_state(Electron::Key::KeyCopy, isPressed);			break;
			case VK_Return:			_electron.set_key_state(Electron::Key::KeyReturn, isPressed);		break;
			case VK_ANSI_Minus:		_electron.set_key_state(Electron::Key::KeyMinus, isPressed);		break;

			case VK_RightArrow:		_electron.set_key_state(Electron::Key::KeyRight, isPressed);		break;
			case VK_LeftArrow:		_electron.set_key_state(Electron::Key::KeyLeft, isPressed);			break;
			case VK_DownArrow:		_electron.set_key_state(Electron::Key::KeyDown, isPressed);			break;
			case VK_UpArrow:		_electron.set_key_state(Electron::Key::KeyUp, isPressed);			break;

			case VK_Delete:			_electron.set_key_state(Electron::Key::KeyDelete, isPressed);		break;
			case VK_Escape:			_electron.set_key_state(Electron::Key::KeyEscape, isPressed);		break;

			case VK_ANSI_Comma:		_electron.set_key_state(Electron::Key::KeyComma, isPressed);		break;
			case VK_ANSI_Period:	_electron.set_key_state(Electron::Key::KeyFullStop, isPressed);		break;

			case VK_ANSI_Semicolon:
									_electron.set_key_state(Electron::Key::KeySemiColon, isPressed);	break;
			case VK_ANSI_Quote:		_electron.set_key_state(Electron::Key::KeyColon, isPressed);		break;

			case VK_ANSI_Slash:		_electron.set_key_state(Electron::Key::KeySlash, isPressed);		break;

			case VK_Shift:			_electron.set_key_state(Electron::Key::KeyShift, isPressed);		break;
			case VK_Control:		_electron.set_key_state(Electron::Key::KeyControl, isPressed);		break;
			case VK_Command:
			case VK_Option:			_electron.set_key_state(Electron::Key::KeyFunc, isPressed);			break;

			case VK_F12:			_electron.set_key_state(Electron::Key::KeyBreak, isPressed);		break;

			default:
//				printf("%02x\n", key);
			break;
		}
	}
}

- (void)setUseFastLoadingHack:(BOOL)useFastLoadingHack {
	@synchronized(self) {
		_useFastLoadingHack = useFastLoadingHack;
		_electron.set_use_fast_tape_hack(useFastLoadingHack ? true : false);
	}
}

- (void)setUseTelevisionOutput:(BOOL)useTelevisionOutput {
	@synchronized(self) {
		_useTelevisionOutput = useTelevisionOutput;
		_electron.get_crt()->set_output_device(useTelevisionOutput ? Outputs::CRT::Television : Outputs::CRT::Monitor);
	}
}

@end
