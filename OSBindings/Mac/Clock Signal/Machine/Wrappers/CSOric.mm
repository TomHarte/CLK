//
//  CSOric.m
//  Clock Signal
//
//  Created by Thomas Harte on 11/10/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import "CSOric.h"

#include "Oric.hpp"
#include "StaticAnalyser.hpp"

#import "CSMachine+Subclassing.h"
#import "NSData+StdVector.h"
#import "NSBundle+DataResource.h"

@implementation CSOric
{
	Oric::Machine _oric;
}

- (instancetype)init
{
	self = [super init];
	if(self)
	{
		NSData *rom = [self rom:@"test108j"];
		if(rom) _oric.set_rom(rom.stdVector8);
	}
	return self;
}

- (NSData *)rom:(NSString *)name
{
	return [[NSBundle mainBundle] dataForResource:name withExtension:@"rom" subdirectory:@"ROMImages/Oric"];
}

- (CRTMachine::Machine * const)machine
{
	return &_oric;
}

#pragma mark - CSKeyboardMachine

- (void)setKey:(uint16_t)key isPressed:(BOOL)isPressed
{
	@synchronized(self) {
		switch(key)
		{
/*			case VK_ANSI_0:		_oric.set_key_state(Oric::Key::Key0, isPressed);	break;
			case VK_ANSI_1:		_oric.set_key_state(Oric::Key::Key1, isPressed);	break;
			case VK_ANSI_2:		_oric.set_key_state(Oric::Key::Key2, isPressed);	break;
			case VK_ANSI_3:		_oric.set_key_state(Oric::Key::Key3, isPressed);	break;
			case VK_ANSI_4:		_oric.set_key_state(Oric::Key::Key4, isPressed);	break;
			case VK_ANSI_5:		_oric.set_key_state(Oric::Key::Key5, isPressed);	break;
			case VK_ANSI_6:		_oric.set_key_state(Oric::Key::Key6, isPressed);	break;
			case VK_ANSI_7:		_oric.set_key_state(Oric::Key::Key7, isPressed);	break;
			case VK_ANSI_8:		_oric.set_key_state(Oric::Key::Key8, isPressed);	break;
			case VK_ANSI_9:		_oric.set_key_state(Oric::Key::Key9, isPressed);	break;

			case VK_ANSI_Q:		_oric.set_key_state(Oric::Key::KeyQ, isPressed);	break;
			case VK_ANSI_W:		_oric.set_key_state(Oric::Key::KeyW, isPressed);	break;
			case VK_ANSI_E:		_oric.set_key_state(Oric::Key::KeyE, isPressed);	break;
			case VK_ANSI_R:		_oric.set_key_state(Oric::Key::KeyR, isPressed);	break;
			case VK_ANSI_T:		_oric.set_key_state(Oric::Key::KeyT, isPressed);	break;
			case VK_ANSI_Y:		_oric.set_key_state(Oric::Key::KeyY, isPressed);	break;
			case VK_ANSI_U:		_oric.set_key_state(Oric::Key::KeyU, isPressed);	break;
			case VK_ANSI_I:		_oric.set_key_state(Oric::Key::KeyI, isPressed);	break;
			case VK_ANSI_O:		_oric.set_key_state(Oric::Key::KeyO, isPressed);	break;
			case VK_ANSI_P:		_oric.set_key_state(Oric::Key::KeyP, isPressed);	break;
			case VK_ANSI_A:		_oric.set_key_state(Oric::Key::KeyA, isPressed);	break;
			case VK_ANSI_S:		_oric.set_key_state(Oric::Key::KeyS, isPressed);	break;
			case VK_ANSI_D:		_oric.set_key_state(Oric::Key::KeyD, isPressed);	break;
			case VK_ANSI_F:		_oric.set_key_state(Oric::Key::KeyF, isPressed);	break;
			case VK_ANSI_G:		_oric.set_key_state(Oric::Key::KeyG, isPressed);	break;
			case VK_ANSI_H:		_oric.set_key_state(Oric::Key::KeyH, isPressed);	break;
			case VK_ANSI_J:		_oric.set_key_state(Oric::Key::KeyJ, isPressed);	break;
			case VK_ANSI_K:		_oric.set_key_state(Oric::Key::KeyK, isPressed);	break;
			case VK_ANSI_L:		_oric.set_key_state(Oric::Key::KeyL, isPressed);	break;
			case VK_ANSI_Z:		_oric.set_key_state(Oric::Key::KeyZ, isPressed);	break;
			case VK_ANSI_X:		_oric.set_key_state(Oric::Key::KeyX, isPressed);	break;
			case VK_ANSI_C:		_oric.set_key_state(Oric::Key::KeyC, isPressed);	break;
			case VK_ANSI_V:		_oric.set_key_state(Oric::Key::KeyV, isPressed);	break;
			case VK_ANSI_B:		_oric.set_key_state(Oric::Key::KeyB, isPressed);	break;
			case VK_ANSI_N:		_oric.set_key_state(Oric::Key::KeyN, isPressed);	break;
			case VK_ANSI_M:		_oric.set_key_state(Oric::Key::KeyM, isPressed);	break;

			case VK_Space:			_oric.set_key_state(Oric::Key::KeySpace, isPressed);		break;
			case VK_Return:			_oric.set_key_state(Oric::Key::KeyReturn, isPressed);		break;
			case VK_ANSI_Minus:		_oric.set_key_state(Oric::Key::KeyMinus, isPressed);		break;

			case VK_RightArrow:		_oric.set_key_state(Oric::Key::KeyRight, isPressed);		break;
			case VK_LeftArrow:		_oric.set_key_state(Oric::Key::KeyLeft, isPressed);			break;
			case VK_DownArrow:		_oric.set_key_state(Oric::Key::KeyDown, isPressed);			break;
			case VK_UpArrow:		_oric.set_key_state(Oric::Key::KeyUp, isPressed);			break;

			case VK_Delete:			_oric.set_key_state(Oric::Key::KeyDelete, isPressed);		break;
			case VK_Escape:			_oric.set_key_state(Oric::Key::KeyEscape, isPressed);		break;

			case VK_ANSI_Comma:		_oric.set_key_state(Oric::Key::KeyComma, isPressed);		break;
			case VK_ANSI_Period:	_oric.set_key_state(Oric::Key::KeyFullStop, isPressed);		break;

			case VK_ANSI_Semicolon:
									_oric.set_key_state(Oric::Key::KeySemiColon, isPressed);	break;

			case VK_Shift:			_oric.set_key_state(Oric::Key::KeyLeftShift, isPressed);	break;
			case VK_RightShift:		_oric.set_key_state(Oric::Key::KeyRightShift, isPressed);	break;
			case VK_Control:		_oric.set_key_state(Oric::Key::KeyControl, isPressed);		break;
			case VK_Command:*/

			case VK_ANSI_Grave:
			case VK_F12:			_oric.set_key_state(Oric::Key::KeyNMI, isPressed);		break;

			default:
				printf("%02x\n", key);
			break;
		}
	}
}

- (void)clearAllKeys
{
}

@end
