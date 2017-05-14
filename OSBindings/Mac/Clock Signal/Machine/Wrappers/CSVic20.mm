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
#include "G64.hpp"
#include "D64.hpp"

#import "NSBundle+DataResource.h"

using namespace Commodore::Vic20;

@implementation CSVic20 {
	Machine _vic20;
	BOOL _joystickMode;
}

- (CRTMachine::Machine * const)machine	{	return &_vic20;		}
- (NSString *)userDefaultsPrefix		{	return @"vic20";	}

- (instancetype)init {
	self = [super init];
	if(self)
	{
		[self setDriveROM:[[NSBundle mainBundle] dataForResource:@"1540" withExtension:@"bin" subdirectory:@"ROMImages/Commodore1540"]];
		[self setBASICROM:[self rom:@"basic"]];
		[self setCountry:CSVic20CountryEuropean];
	}
	return self;
}

- (NSData *)rom:(NSString *)name
{
	return [[NSBundle mainBundle] dataForResource:name withExtension:@"bin" subdirectory:@"ROMImages/Vic20"];
}

#pragma mark - ROM setting

- (void)setROM:(nonnull NSData *)rom slot:(ROMSlot)slot {
	@synchronized(self) {
		_vic20.set_rom(slot, rom.length, (const uint8_t *)rom.bytes);
	}
}

- (void)setKernelROM:(nonnull NSData *)rom {
	[self setROM:rom slot:Kernel];
}

- (void)setBASICROM:(nonnull NSData *)rom {
	[self setROM:rom slot:BASIC];
}

- (void)setCharactersROM:(nonnull NSData *)rom {
	[self setROM:rom slot:Characters];
}

- (void)setDriveROM:(nonnull NSData *)rom {
	[self setROM:rom slot:Drive];
}

#pragma mark - Keyboard map

- (void)setKey:(uint16_t)key isPressed:(BOOL)isPressed {
	static NSDictionary<NSNumber *, NSNumber *> *vicKeysByKeys = @{
		@(VK_ANSI_1):	@(Key::Key1),	@(VK_ANSI_2):	@(Key::Key2),
		@(VK_ANSI_3):	@(Key::Key3),	@(VK_ANSI_4):	@(Key::Key4),
		@(VK_ANSI_5):	@(Key::Key5),	@(VK_ANSI_6):	@(Key::Key6),
		@(VK_ANSI_7):	@(Key::Key7),	@(VK_ANSI_8):	@(Key::Key8),
		@(VK_ANSI_9):	@(Key::Key9),	@(VK_ANSI_0):	@(Key::Key0),

		@(VK_ANSI_Q):	@(Key::KeyQ),	@(VK_ANSI_W):	@(Key::KeyW),
		@(VK_ANSI_E):	@(Key::KeyE),	@(VK_ANSI_R):	@(Key::KeyR),
		@(VK_ANSI_T):	@(Key::KeyT),	@(VK_ANSI_Y):	@(Key::KeyY),
		@(VK_ANSI_U):	@(Key::KeyU),	@(VK_ANSI_I):	@(Key::KeyI),
		@(VK_ANSI_O):	@(Key::KeyO),	@(VK_ANSI_P):	@(Key::KeyP),
		@(VK_ANSI_A):	@(Key::KeyA),	@(VK_ANSI_S):	@(Key::KeyS),
		@(VK_ANSI_D):	@(Key::KeyD),	@(VK_ANSI_F):	@(Key::KeyF),
		@(VK_ANSI_G):	@(Key::KeyG),	@(VK_ANSI_H):	@(Key::KeyH),
		@(VK_ANSI_J):	@(Key::KeyJ),	@(VK_ANSI_K):	@(Key::KeyK),
		@(VK_ANSI_L):	@(Key::KeyL),	@(VK_ANSI_Z):	@(Key::KeyZ),
		@(VK_ANSI_X):	@(Key::KeyX),	@(VK_ANSI_C):	@(Key::KeyC),
		@(VK_ANSI_V):	@(Key::KeyV),	@(VK_ANSI_B):	@(Key::KeyB),
		@(VK_ANSI_N):	@(Key::KeyN),	@(VK_ANSI_M):	@(Key::KeyM),

		@(VK_Space):			@(Key::KeySpace),
		@(VK_Return):			@(Key::KeyReturn),
		@(VK_Delete):			@(Key::KeyDelete),
		@(VK_ANSI_Comma):		@(Key::KeyComma),
		@(VK_ANSI_Period):		@(Key::KeyFullStop),
		@(VK_ANSI_Minus):		@(Key::KeyDash),
		@(VK_ANSI_Equal):		@(Key::KeyEquals),
		@(VK_ANSI_Semicolon):	@(Key::KeyColon),
		@(VK_ANSI_Quote):		@(Key::KeySemicolon),
		@(VK_ANSI_Slash):		@(Key::KeySlash),
		@(VK_Option):			@(Key::KeyCBM),
		@(VK_Control):			@(Key::KeyControl),

		@(VK_F1):	@(Key::KeyF1),	@(VK_F3):	@(Key::KeyF3),
		@(VK_F5):	@(Key::KeyF5),	@(VK_F7):	@(Key::KeyF7),

		@(VK_ANSI_Grave):			@(Key::KeyLeft),
		@(VK_Tab):					@(Key::KeyRunStop),
		@(VK_ANSI_LeftBracket):		@(Key::KeyAt),
		@(VK_ANSI_RightBracket):	@(Key::KeyAsterisk),
		@(VK_ANSI_Backslash):		@(Key::KeyUp),

		@(VK_RightArrow):			@(Key::KeyRight),
		@(VK_DownArrow):			@(Key::KeyDown),
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
				case VK_UpArrow:	_vic20.set_joystick_state(JoystickInput::Up, isPressed);	break;
				case VK_DownArrow:	_vic20.set_joystick_state(JoystickInput::Down, isPressed);	break;
				case VK_LeftArrow:	_vic20.set_joystick_state(JoystickInput::Left, isPressed);	break;
				case VK_RightArrow:	_vic20.set_joystick_state(JoystickInput::Right, isPressed);	break;
				case VK_ANSI_A:		_vic20.set_joystick_state(JoystickInput::Fire, isPressed);	break;
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
						_vic20.set_key_state((Key)targetKey.integerValue, isPressed);
					}
					else
					{
						NSLog(@"Unmapped: %02x", key);
					}
				} break;

				case VK_Shift:
					// Yuck
					_vic20.set_key_state(Key::KeyLShift, isPressed);
					_vic20.set_key_state(Key::KeyRShift, isPressed);
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

#pragma mark - Public configuration options

- (void)setUseFastLoadingHack:(BOOL)useFastLoadingHack {
	_useFastLoadingHack = useFastLoadingHack;
	@synchronized(self) {
		_vic20.set_use_fast_tape_hack(useFastLoadingHack ? true : false);
	}
}

- (void)setCountry:(CSVic20Country)country {
	_country = country;
	NSString *charactersROM, *kernelROM;
	Commodore::Vic20::Region region;
	switch(country)
	{
		case CSVic20CountryDanish:
			region = Commodore::Vic20::Region::PAL;
			charactersROM = @"characters-danish";
			kernelROM = @"kernel-danish";
		break;
		case CSVic20CountryEuropean:
			region = Commodore::Vic20::Region::PAL;
			charactersROM = @"characters-english";
			kernelROM = @"kernel-pal";
		break;
		case CSVic20CountryJapanese:
			region = Commodore::Vic20::Region::NTSC;
			charactersROM = @"characters-japanese";
			kernelROM = @"kernel-japanese";
		break;
		case CSVic20CountrySwedish:
			region = Commodore::Vic20::Region::PAL;
			charactersROM = @"characters-swedish";
			kernelROM = @"kernel-swedish";
		break;
		case CSVic20CountryAmerican:
			region = Commodore::Vic20::Region::NTSC;
			charactersROM = @"characters-english";
			kernelROM = @"kernel-ntsc";
		break;
	}

	@synchronized(self) {
		_vic20.set_region(region);
		[self setCharactersROM:[self rom:charactersROM]];
		[self setKernelROM:[self rom:kernelROM]];
	}
}

- (void)setMemorySize:(CSVic20MemorySize)memorySize {
	_memorySize = memorySize;
	@synchronized(self) {
		switch(memorySize) {
			case CSVic20MemorySize5Kb: _vic20.set_memory_size(Commodore::Vic20::Default);	break;
			case CSVic20MemorySize8Kb: _vic20.set_memory_size(Commodore::Vic20::ThreeKB);	break;
			case CSVic20MemorySize32Kb: _vic20.set_memory_size(Commodore::Vic20::ThirtyTwoKB);	break;
		}
	}
}

@end
