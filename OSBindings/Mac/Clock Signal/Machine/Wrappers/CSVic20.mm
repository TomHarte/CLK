//
//  CSVic20.m
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import "CSVic20.h"

#include "Vic20.hpp"
#include "TypedDynamicMachine.hpp"

@implementation CSVic20 {
	Machine::TypedDynamicMachine<Commodore::Vic20::Machine> _vic20;
}

- (instancetype)init {
	_vic20 = Machine::TypedDynamicMachine<Commodore::Vic20::Machine>(Commodore::Vic20::Machine::Vic20());
	return [super initWithMachine:&_vic20];
}

- (NSString *)userDefaultsPrefix		{	return @"vic20";	}

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

- (void)setCountry:(CSVic20Country)country {
	_country = country;
	Commodore::Vic20::Region region;
	switch(country) {
		case CSVic20CountryDanish:		region = Commodore::Vic20::Danish;		break;
		case CSVic20CountryEuropean:	region = Commodore::Vic20::European;	break;
		case CSVic20CountryJapanese:	region = Commodore::Vic20::Japanese;	break;
		case CSVic20CountrySwedish:		region = Commodore::Vic20::Swedish;		break;
		case CSVic20CountryAmerican:	region = Commodore::Vic20::American;	break;
	}

	@synchronized(self) {
		_vic20.get()->set_region(region);
	}
}

- (void)setMemorySize:(CSVic20MemorySize)memorySize {
	_memorySize = memorySize;
	@synchronized(self) {
		switch(memorySize) {
			case CSVic20MemorySize5Kb: _vic20.get()->set_memory_size(Commodore::Vic20::Default);		break;
			case CSVic20MemorySize8Kb: _vic20.get()->set_memory_size(Commodore::Vic20::ThreeKB);		break;
			case CSVic20MemorySize32Kb: _vic20.get()->set_memory_size(Commodore::Vic20::ThirtyTwoKB);	break;
		}
	}
}

@end
