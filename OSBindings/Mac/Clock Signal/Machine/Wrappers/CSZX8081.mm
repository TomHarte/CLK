//
//  CSZX8081.m
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#import "CSZX8081.h"

#include "ZX8081.hpp"

#import "CSMachine+Subclassing.h"
#import "NSData+StdVector.h"
#import "NSBundle+DataResource.h"

@implementation CSZX8081 {
	ZX8081::Machine _zx8081;
}

- (CRTMachine::Machine * const)machine {
	return &_zx8081;
}

- (instancetype)init {
	self = [super init];
	if(self) {
		_zx8081.set_rom(ZX8081::ROMType::ZX80, [self rom:@"zx80"].stdVector8);
		_zx8081.set_rom(ZX8081::ROMType::ZX81, [self rom:@"zx81"].stdVector8);
	}
	return self;
}

- (NSData *)rom:(NSString *)name {
	return [[NSBundle mainBundle] dataForResource:name withExtension:@"rom" subdirectory:@"ROMImages/ZX8081"];
}

#pragma mark - Keyboard Mapping

- (void)clearAllKeys {
	@synchronized(self) {
		_zx8081.clear_all_keys();
	}
}

- (void)setKey:(uint16_t)key isPressed:(BOOL)isPressed {
	@synchronized(self) {
		switch(key)
		{
			case VK_ANSI_0:		_zx8081.set_key_state(ZX8081::Key::Key0, isPressed);	break;
			case VK_ANSI_1:		_zx8081.set_key_state(ZX8081::Key::Key1, isPressed);	break;
			case VK_ANSI_2:		_zx8081.set_key_state(ZX8081::Key::Key2, isPressed);	break;
			case VK_ANSI_3:		_zx8081.set_key_state(ZX8081::Key::Key3, isPressed);	break;
			case VK_ANSI_4:		_zx8081.set_key_state(ZX8081::Key::Key4, isPressed);	break;
			case VK_ANSI_5:		_zx8081.set_key_state(ZX8081::Key::Key5, isPressed);	break;
			case VK_ANSI_6:		_zx8081.set_key_state(ZX8081::Key::Key6, isPressed);	break;
			case VK_ANSI_7:		_zx8081.set_key_state(ZX8081::Key::Key7, isPressed);	break;
			case VK_ANSI_8:		_zx8081.set_key_state(ZX8081::Key::Key8, isPressed);	break;
			case VK_ANSI_9:		_zx8081.set_key_state(ZX8081::Key::Key9, isPressed);	break;

			case VK_ANSI_Q:		_zx8081.set_key_state(ZX8081::Key::KeyQ, isPressed);	break;
			case VK_ANSI_W:		_zx8081.set_key_state(ZX8081::Key::KeyW, isPressed);	break;
			case VK_ANSI_E:		_zx8081.set_key_state(ZX8081::Key::KeyE, isPressed);	break;
			case VK_ANSI_R:		_zx8081.set_key_state(ZX8081::Key::KeyR, isPressed);	break;
			case VK_ANSI_T:		_zx8081.set_key_state(ZX8081::Key::KeyT, isPressed);	break;
			case VK_ANSI_Y:		_zx8081.set_key_state(ZX8081::Key::KeyY, isPressed);	break;
			case VK_ANSI_U:		_zx8081.set_key_state(ZX8081::Key::KeyU, isPressed);	break;
			case VK_ANSI_I:		_zx8081.set_key_state(ZX8081::Key::KeyI, isPressed);	break;
			case VK_ANSI_O:		_zx8081.set_key_state(ZX8081::Key::KeyO, isPressed);	break;
			case VK_ANSI_P:		_zx8081.set_key_state(ZX8081::Key::KeyP, isPressed);	break;

			case VK_ANSI_A:		_zx8081.set_key_state(ZX8081::Key::KeyA, isPressed);	break;
			case VK_ANSI_S:		_zx8081.set_key_state(ZX8081::Key::KeyS, isPressed);	break;
			case VK_ANSI_D:		_zx8081.set_key_state(ZX8081::Key::KeyD, isPressed);	break;
			case VK_ANSI_F:		_zx8081.set_key_state(ZX8081::Key::KeyF, isPressed);	break;
			case VK_ANSI_G:		_zx8081.set_key_state(ZX8081::Key::KeyG, isPressed);	break;
			case VK_ANSI_H:		_zx8081.set_key_state(ZX8081::Key::KeyH, isPressed);	break;
			case VK_ANSI_J:		_zx8081.set_key_state(ZX8081::Key::KeyJ, isPressed);	break;
			case VK_ANSI_K:		_zx8081.set_key_state(ZX8081::Key::KeyK, isPressed);	break;
			case VK_ANSI_L:		_zx8081.set_key_state(ZX8081::Key::KeyL, isPressed);	break;

			case VK_ANSI_Z:		_zx8081.set_key_state(ZX8081::Key::KeyZ, isPressed);	break;
			case VK_ANSI_X:		_zx8081.set_key_state(ZX8081::Key::KeyX, isPressed);	break;
			case VK_ANSI_C:		_zx8081.set_key_state(ZX8081::Key::KeyC, isPressed);	break;
			case VK_ANSI_V:		_zx8081.set_key_state(ZX8081::Key::KeyV, isPressed);	break;
			case VK_ANSI_B:		_zx8081.set_key_state(ZX8081::Key::KeyB, isPressed);	break;
			case VK_ANSI_N:		_zx8081.set_key_state(ZX8081::Key::KeyN, isPressed);	break;
			case VK_ANSI_M:		_zx8081.set_key_state(ZX8081::Key::KeyM, isPressed);	break;

			case VK_Shift:
			case VK_RightShift:
								_zx8081.set_key_state(ZX8081::Key::KeyShift, isPressed);	break;
			break;

			case VK_ANSI_Period:_zx8081.set_key_state(ZX8081::Key::KeyDot, isPressed);		break;
			case VK_Return:		_zx8081.set_key_state(ZX8081::Key::KeyEnter, isPressed);	break;
			case VK_Space:		_zx8081.set_key_state(ZX8081::Key::KeySpace, isPressed);	break;
		}
	}
}

- (NSString *)userDefaultsPrefix {	return @"zx8081";	}

@end
