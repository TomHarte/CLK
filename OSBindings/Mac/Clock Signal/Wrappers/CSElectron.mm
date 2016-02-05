//
//  CSElectron.m
//  Clock Signal
//
//  Created by Thomas Harte on 04/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import "CSElectron.h"

#import "Electron.hpp"
#import "CSMachine+Subclassing.h"
#import "TapeUEF.hpp"

@implementation CSElectron {
	Electron::Machine _electron;
}

- (void)doRunForNumberOfCycles:(int)numberOfCycles {
	_electron.run_for_cycles(numberOfCycles);
}

- (void)setOSROM:(nonnull NSData *)rom {
	_electron.set_rom(Electron::ROMSlotOS, rom.length, (const uint8_t *)rom.bytes);
}

- (void)setBASICROM:(nonnull NSData *)rom {
	_electron.set_rom(Electron::ROMSlotBASIC, rom.length, (const uint8_t *)rom.bytes);
}

- (void)setROM:(nonnull NSData *)rom slot:(int)slot {
	_electron.set_rom((Electron::ROMSlot)slot, rom.length, (const uint8_t *)rom.bytes);
}

- (void)drawViewForPixelSize:(CGSize)pixelSize {
	_electron.get_crt()->draw_frame((int)pixelSize.width, (int)pixelSize.height);
}

- (BOOL)openUEFAtURL:(NSURL *)URL {
	try {
		std::shared_ptr<Storage::UEF> tape(new Storage::UEF([URL fileSystemRepresentation]));
		_electron.set_tape(tape);
		return YES;
	} catch(int exception) {
		return NO;
	}
}

- (BOOL)setSpeakerDelegate:(Outputs::Speaker::Delegate *)delegate sampleRate:(int)sampleRate {
	_electron.get_speaker()->set_output_rate(sampleRate, 256);
	_electron.get_speaker()->set_output_quality(15);
	_electron.get_speaker()->set_delegate(delegate);
	return YES;
}

- (void)setView:(CSCathodeRayView *)view {
	[super setView:view];
}

- (void)setKey:(uint16_t)key isPressed:(BOOL)isPressed {
	switch(key)
	{
		case kVK_ANSI_0:		_electron.set_key_state(Electron::Key::Key0, isPressed);	break;
		case kVK_ANSI_1:		_electron.set_key_state(Electron::Key::Key1, isPressed);	break;
		case kVK_ANSI_2:		_electron.set_key_state(Electron::Key::Key2, isPressed);	break;
		case kVK_ANSI_3:		_electron.set_key_state(Electron::Key::Key3, isPressed);	break;
		case kVK_ANSI_4:		_electron.set_key_state(Electron::Key::Key4, isPressed);	break;
		case kVK_ANSI_5:		_electron.set_key_state(Electron::Key::Key5, isPressed);	break;
		case kVK_ANSI_6:		_electron.set_key_state(Electron::Key::Key6, isPressed);	break;
		case kVK_ANSI_7:		_electron.set_key_state(Electron::Key::Key7, isPressed);	break;
		case kVK_ANSI_8:		_electron.set_key_state(Electron::Key::Key8, isPressed);	break;
		case kVK_ANSI_9:		_electron.set_key_state(Electron::Key::Key9, isPressed);	break;

		case kVK_ANSI_Q:		_electron.set_key_state(Electron::Key::KeyQ, isPressed);	break;
		case kVK_ANSI_W:		_electron.set_key_state(Electron::Key::KeyW, isPressed);	break;
		case kVK_ANSI_E:		_electron.set_key_state(Electron::Key::KeyE, isPressed);	break;
		case kVK_ANSI_R:		_electron.set_key_state(Electron::Key::KeyR, isPressed);	break;
		case kVK_ANSI_T:		_electron.set_key_state(Electron::Key::KeyT, isPressed);	break;
		case kVK_ANSI_Y:		_electron.set_key_state(Electron::Key::KeyY, isPressed);	break;
		case kVK_ANSI_U:		_electron.set_key_state(Electron::Key::KeyU, isPressed);	break;
		case kVK_ANSI_I:		_electron.set_key_state(Electron::Key::KeyI, isPressed);	break;
		case kVK_ANSI_O:		_electron.set_key_state(Electron::Key::KeyO, isPressed);	break;
		case kVK_ANSI_P:		_electron.set_key_state(Electron::Key::KeyP, isPressed);	break;
		case kVK_ANSI_A:		_electron.set_key_state(Electron::Key::KeyA, isPressed);	break;
		case kVK_ANSI_S:		_electron.set_key_state(Electron::Key::KeyS, isPressed);	break;
		case kVK_ANSI_D:		_electron.set_key_state(Electron::Key::KeyD, isPressed);	break;
		case kVK_ANSI_F:		_electron.set_key_state(Electron::Key::KeyF, isPressed);	break;
		case kVK_ANSI_G:		_electron.set_key_state(Electron::Key::KeyG, isPressed);	break;
		case kVK_ANSI_H:		_electron.set_key_state(Electron::Key::KeyH, isPressed);	break;
		case kVK_ANSI_J:		_electron.set_key_state(Electron::Key::KeyJ, isPressed);	break;
		case kVK_ANSI_K:		_electron.set_key_state(Electron::Key::KeyK, isPressed);	break;
		case kVK_ANSI_L:		_electron.set_key_state(Electron::Key::KeyL, isPressed);	break;
		case kVK_ANSI_Z:		_electron.set_key_state(Electron::Key::KeyZ, isPressed);	break;
		case kVK_ANSI_X:		_electron.set_key_state(Electron::Key::KeyX, isPressed);	break;
		case kVK_ANSI_C:		_electron.set_key_state(Electron::Key::KeyC, isPressed);	break;
		case kVK_ANSI_V:		_electron.set_key_state(Electron::Key::KeyV, isPressed);	break;
		case kVK_ANSI_B:		_electron.set_key_state(Electron::Key::KeyB, isPressed);	break;
		case kVK_ANSI_N:		_electron.set_key_state(Electron::Key::KeyN, isPressed);	break;
		case kVK_ANSI_M:		_electron.set_key_state(Electron::Key::KeyM, isPressed);	break;

		case kVK_Space:			_electron.set_key_state(Electron::Key::KeySpace, isPressed);		break;
		case kVK_ANSI_Grave:
		case kVK_ANSI_Backslash:
								_electron.set_key_state(Electron::Key::KeyCopy, isPressed);			break;
		case kVK_Return:		_electron.set_key_state(Electron::Key::KeyReturn, isPressed);		break;
		case kVK_ANSI_Minus:	_electron.set_key_state(Electron::Key::KeyMinus, isPressed);		break;

		case kVK_RightArrow:	_electron.set_key_state(Electron::Key::KeyRight, isPressed);		break;
		case kVK_LeftArrow:		_electron.set_key_state(Electron::Key::KeyLeft, isPressed);			break;
		case kVK_DownArrow:		_electron.set_key_state(Electron::Key::KeyDown, isPressed);			break;
		case kVK_UpArrow:		_electron.set_key_state(Electron::Key::KeyUp, isPressed);			break;

		case kVK_Delete:		_electron.set_key_state(Electron::Key::KeyDelete, isPressed);		break;
		case kVK_Escape:		_electron.set_key_state(Electron::Key::KeyEscape, isPressed);		break;

		case kVK_ANSI_Comma:	_electron.set_key_state(Electron::Key::KeyComma, isPressed);		break;
		case kVK_ANSI_Period:	_electron.set_key_state(Electron::Key::KeyFullStop, isPressed);		break;

		case kVK_ANSI_Semicolon:
								_electron.set_key_state(Electron::Key::KeySemiColon, isPressed);	break;
		case kVK_ANSI_Quote:	_electron.set_key_state(Electron::Key::KeyColon, isPressed);		break;

		case kVK_ANSI_Slash:	_electron.set_key_state(Electron::Key::KeySlash, isPressed);		break;

		case kVK_Shift:			_electron.set_key_state(Electron::Key::KeyShift, isPressed);		break;
		case kVK_Control:		_electron.set_key_state(Electron::Key::KeyControl, isPressed);		break;
		case kVK_Command:		_electron.set_key_state(Electron::Key::KeyFunc, isPressed);			break;

		case kVK_F12:			_electron.set_key_state(Electron::Key::KeyBreak, isPressed);		break;

		default:
//			printf("%02x\n", key);
		break;
	}
}

@end
