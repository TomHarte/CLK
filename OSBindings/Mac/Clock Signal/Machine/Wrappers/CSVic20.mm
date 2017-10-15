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

#import "CSmachine+Subclassing.h"
#import "NSBundle+DataResource.h"

using namespace Commodore::Vic20;

@implementation CSVic20 {
	std::unique_ptr<Machine> _vic20;
	BOOL _joystickMode;
}

- (NSString *)userDefaultsPrefix		{	return @"vic20";	}

- (instancetype)init {
	Machine *machine = Commodore::Vic20::Machine::Vic20();

	self = [super initWithMachine:machine];
	if(self) {
		_vic20.reset(machine);
		[self setDriveROM:[[NSBundle mainBundle] dataForResource:@"1540" withExtension:@"bin" subdirectory:@"ROMImages/Commodore1540"]];
		[self setBASICROM:[self rom:@"basic"]];
		[self setCountry:CSVic20CountryEuropean];
	}
	return self;
}

- (NSData *)rom:(NSString *)name {
	return [[NSBundle mainBundle] dataForResource:name withExtension:@"bin" subdirectory:@"ROMImages/Vic20"];
}

#pragma mark - ROM setting

- (void)setROM:(nonnull NSData *)rom slot:(ROMSlot)slot {
	@synchronized(self) {
		_vic20->set_rom(slot, rom.length, (const uint8_t *)rom.bytes);
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

/*- (void)setKey:(uint16_t)key isPressed:(BOOL)isPressed {
		switch(key) {
			case VK_UpArrow:	_vic20->set_joystick_state(JoystickInput::Up, isPressed);	break;
			case VK_DownArrow:	_vic20->set_joystick_state(JoystickInput::Down, isPressed);	break;
			case VK_LeftArrow:	_vic20->set_joystick_state(JoystickInput::Left, isPressed);	break;
			case VK_RightArrow:	_vic20->set_joystick_state(JoystickInput::Right, isPressed);	break;
			case VK_ANSI_A:		_vic20->set_joystick_state(JoystickInput::Fire, isPressed);	break;
		}
}*/

#pragma mark - Public configuration options

- (void)setUseFastLoadingHack:(BOOL)useFastLoadingHack {
	_useFastLoadingHack = useFastLoadingHack;
	@synchronized(self) {
		_vic20->set_use_fast_tape_hack(useFastLoadingHack ? true : false);
	}
}

- (void)setCountry:(CSVic20Country)country {
	_country = country;
	NSString *charactersROM, *kernelROM;
	Commodore::Vic20::Region region;
	switch(country) {
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
		_vic20->set_region(region);
		[self setCharactersROM:[self rom:charactersROM]];
		[self setKernelROM:[self rom:kernelROM]];
	}
}

- (void)setMemorySize:(CSVic20MemorySize)memorySize {
	_memorySize = memorySize;
	@synchronized(self) {
		switch(memorySize) {
			case CSVic20MemorySize5Kb: _vic20->set_memory_size(Commodore::Vic20::Default);	break;
			case CSVic20MemorySize8Kb: _vic20->set_memory_size(Commodore::Vic20::ThreeKB);	break;
			case CSVic20MemorySize32Kb: _vic20->set_memory_size(Commodore::Vic20::ThirtyTwoKB);	break;
		}
	}
}

@end
