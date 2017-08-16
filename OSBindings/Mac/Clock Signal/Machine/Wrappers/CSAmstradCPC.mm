//
//  CSAmstradCPC.m
//  Clock Signal
//
//  Created by Thomas Harte on 30/07/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#import "CSAmstradCPC.h"

#include "AmstradCPC.hpp"

#import "CSMachine+Subclassing.h"
#import "NSData+StdVector.h"
#import "NSBundle+DataResource.h"

@implementation CSAmstradCPC {
	std::unique_ptr<AmstradCPC::Machine> _amstradCPC;
}

- (CRTMachine::Machine * const)machine {
	if(!_amstradCPC) {
		_amstradCPC.reset(AmstradCPC::Machine::AmstradCPC());
	}
	return _amstradCPC.get();
}

- (instancetype)init {
	self = [super init];
	if(self) {
		[self machine];
		NSDictionary *roms = @{
			@(AmstradCPC::ROMType::OS464) : @"os464",
			@(AmstradCPC::ROMType::OS664) : @"os664",
			@(AmstradCPC::ROMType::OS6128) : @"os6128",
			@(AmstradCPC::ROMType::BASIC464) : @"basic464",
			@(AmstradCPC::ROMType::BASIC664) : @"basic664",
			@(AmstradCPC::ROMType::BASIC6128) : @"basic6128",
			@(AmstradCPC::ROMType::AMSDOS) : @"amsdos",
		};

		for(NSNumber *key in roms.allKeys) {
			AmstradCPC::ROMType type = (AmstradCPC::ROMType)key.integerValue;
			NSString *name = roms[key];
			NSData *data = [self rom:name];
			if(data) {
				_amstradCPC->set_rom(type, data.stdVector8);
			} else {
				NSLog(@"Amstrad CPC ROM missing: %@", name);
			}
		}
	}
	return self;
}

- (NSData *)rom:(NSString *)name {
	return [[NSBundle mainBundle] dataForResource:name withExtension:@"rom" subdirectory:@"ROMImages/AmstradCPC"];
}

- (NSString *)userDefaultsPrefix {	return @"amstradCPC";	}

#pragma mark - Keyboard Mapping

- (void)clearAllKeys {
	@synchronized(self) {
		_amstradCPC->clear_all_keys();
	}
}

- (void)setKey:(uint16_t)key isPressed:(BOOL)isPressed {
	@synchronized(self) {
		switch(key) {
			case VK_ANSI_0:		_amstradCPC->set_key_state(AmstradCPC::Key::Key0, isPressed);	break;
			case VK_ANSI_1:		_amstradCPC->set_key_state(AmstradCPC::Key::Key1, isPressed);	break;
			case VK_ANSI_2:		_amstradCPC->set_key_state(AmstradCPC::Key::Key2, isPressed);	break;
			case VK_ANSI_3:		_amstradCPC->set_key_state(AmstradCPC::Key::Key3, isPressed);	break;
			case VK_ANSI_4:		_amstradCPC->set_key_state(AmstradCPC::Key::Key4, isPressed);	break;
			case VK_ANSI_5:		_amstradCPC->set_key_state(AmstradCPC::Key::Key5, isPressed);	break;
			case VK_ANSI_6:		_amstradCPC->set_key_state(AmstradCPC::Key::Key6, isPressed);	break;
			case VK_ANSI_7:		_amstradCPC->set_key_state(AmstradCPC::Key::Key7, isPressed);	break;
			case VK_ANSI_8:		_amstradCPC->set_key_state(AmstradCPC::Key::Key8, isPressed);	break;
			case VK_ANSI_9:		_amstradCPC->set_key_state(AmstradCPC::Key::Key9, isPressed);	break;

			case VK_ANSI_Q:		_amstradCPC->set_key_state(AmstradCPC::Key::KeyQ, isPressed);	break;
			case VK_ANSI_W:		_amstradCPC->set_key_state(AmstradCPC::Key::KeyW, isPressed);	break;
			case VK_ANSI_E:		_amstradCPC->set_key_state(AmstradCPC::Key::KeyE, isPressed);	break;
			case VK_ANSI_R:		_amstradCPC->set_key_state(AmstradCPC::Key::KeyR, isPressed);	break;
			case VK_ANSI_T:		_amstradCPC->set_key_state(AmstradCPC::Key::KeyT, isPressed);	break;
			case VK_ANSI_Y:		_amstradCPC->set_key_state(AmstradCPC::Key::KeyY, isPressed);	break;
			case VK_ANSI_U:		_amstradCPC->set_key_state(AmstradCPC::Key::KeyU, isPressed);	break;
			case VK_ANSI_I:		_amstradCPC->set_key_state(AmstradCPC::Key::KeyI, isPressed);	break;
			case VK_ANSI_O:		_amstradCPC->set_key_state(AmstradCPC::Key::KeyO, isPressed);	break;
			case VK_ANSI_P:		_amstradCPC->set_key_state(AmstradCPC::Key::KeyP, isPressed);	break;
			case VK_ANSI_A:		_amstradCPC->set_key_state(AmstradCPC::Key::KeyA, isPressed);	break;
			case VK_ANSI_S:		_amstradCPC->set_key_state(AmstradCPC::Key::KeyS, isPressed);	break;
			case VK_ANSI_D:		_amstradCPC->set_key_state(AmstradCPC::Key::KeyD, isPressed);	break;
			case VK_ANSI_F:		_amstradCPC->set_key_state(AmstradCPC::Key::KeyF, isPressed);	break;
			case VK_ANSI_G:		_amstradCPC->set_key_state(AmstradCPC::Key::KeyG, isPressed);	break;
			case VK_ANSI_H:		_amstradCPC->set_key_state(AmstradCPC::Key::KeyH, isPressed);	break;
			case VK_ANSI_J:		_amstradCPC->set_key_state(AmstradCPC::Key::KeyJ, isPressed);	break;
			case VK_ANSI_K:		_amstradCPC->set_key_state(AmstradCPC::Key::KeyK, isPressed);	break;
			case VK_ANSI_L:		_amstradCPC->set_key_state(AmstradCPC::Key::KeyL, isPressed);	break;
			case VK_ANSI_Z:		_amstradCPC->set_key_state(AmstradCPC::Key::KeyZ, isPressed);	break;
			case VK_ANSI_X:		_amstradCPC->set_key_state(AmstradCPC::Key::KeyX, isPressed);	break;
			case VK_ANSI_C:		_amstradCPC->set_key_state(AmstradCPC::Key::KeyC, isPressed);	break;
			case VK_ANSI_V:		_amstradCPC->set_key_state(AmstradCPC::Key::KeyV, isPressed);	break;
			case VK_ANSI_B:		_amstradCPC->set_key_state(AmstradCPC::Key::KeyB, isPressed);	break;
			case VK_ANSI_N:		_amstradCPC->set_key_state(AmstradCPC::Key::KeyN, isPressed);	break;
			case VK_ANSI_M:		_amstradCPC->set_key_state(AmstradCPC::Key::KeyM, isPressed);	break;

			case VK_Space:			_amstradCPC->set_key_state(AmstradCPC::Key::KeySpace, isPressed);		break;
			case VK_ANSI_Grave:		_amstradCPC->set_key_state(AmstradCPC::Key::KeyCopy, isPressed);		break;
			case VK_Return:			_amstradCPC->set_key_state(AmstradCPC::Key::KeyReturn, isPressed);		break;
			case VK_ANSI_Minus:		_amstradCPC->set_key_state(AmstradCPC::Key::KeyMinus, isPressed);		break;

			case VK_RightArrow:		_amstradCPC->set_key_state(AmstradCPC::Key::KeyRight, isPressed);		break;
			case VK_LeftArrow:		_amstradCPC->set_key_state(AmstradCPC::Key::KeyLeft, isPressed);		break;
			case VK_DownArrow:		_amstradCPC->set_key_state(AmstradCPC::Key::KeyDown, isPressed);		break;
			case VK_UpArrow:		_amstradCPC->set_key_state(AmstradCPC::Key::KeyUp, isPressed);			break;

			case VK_Delete:			_amstradCPC->set_key_state(AmstradCPC::Key::KeyDelete, isPressed);		break;
			case VK_Escape:			_amstradCPC->set_key_state(AmstradCPC::Key::KeyEscape, isPressed);		break;

			case VK_ANSI_Comma:		_amstradCPC->set_key_state(AmstradCPC::Key::KeyComma, isPressed);		break;
			case VK_ANSI_Period:	_amstradCPC->set_key_state(AmstradCPC::Key::KeyFullStop, isPressed);	break;

			case VK_ANSI_Semicolon:
									_amstradCPC->set_key_state(AmstradCPC::Key::KeySemicolon, isPressed);	break;
			case VK_ANSI_Quote:		_amstradCPC->set_key_state(AmstradCPC::Key::KeyColon, isPressed);		break;

			case VK_ANSI_Slash:		_amstradCPC->set_key_state(AmstradCPC::Key::KeyForwardSlash, isPressed);	break;
			case VK_ANSI_Backslash:	_amstradCPC->set_key_state(AmstradCPC::Key::KeyBackSlash, isPressed);		break;

			case VK_Shift:			_amstradCPC->set_key_state(AmstradCPC::Key::KeyShift, isPressed);		break;
			case VK_Control:		_amstradCPC->set_key_state(AmstradCPC::Key::KeyControl, isPressed);		break;

			case VK_F1:				_amstradCPC->set_key_state(AmstradCPC::Key::KeyF1, isPressed);			break;
			case VK_F2:				_amstradCPC->set_key_state(AmstradCPC::Key::KeyF2, isPressed);			break;
			case VK_F3:				_amstradCPC->set_key_state(AmstradCPC::Key::KeyF3, isPressed);			break;
			case VK_F4:				_amstradCPC->set_key_state(AmstradCPC::Key::KeyF4, isPressed);			break;
			case VK_F5:				_amstradCPC->set_key_state(AmstradCPC::Key::KeyF5, isPressed);			break;
			case VK_F6:				_amstradCPC->set_key_state(AmstradCPC::Key::KeyF6, isPressed);			break;
			case VK_F7:				_amstradCPC->set_key_state(AmstradCPC::Key::KeyF7, isPressed);			break;
			case VK_F8:				_amstradCPC->set_key_state(AmstradCPC::Key::KeyF8, isPressed);			break;
			case VK_F9:				_amstradCPC->set_key_state(AmstradCPC::Key::KeyF9, isPressed);			break;
			case VK_F10:			_amstradCPC->set_key_state(AmstradCPC::Key::KeyF0, isPressed);			break;
			case VK_F12:			_amstradCPC->set_key_state(AmstradCPC::Key::KeyFDot, isPressed);		break;

			default:
//				printf("%02x\n", key);
			break;
		}
	}
}

@end
