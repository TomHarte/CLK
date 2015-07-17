//
//  Atari2600.m
//  ElectrEm
//
//  Created by Thomas Harte on 14/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#import "Atari2600.h"
#import "Atari2600.hpp"

@implementation CSAtari2600 {
	Atari2600::Machine _atari2600;
}

- (void)runForNumberOfCycles:(int)cycles {
	_atari2600.run_for_cycles(cycles);
}

- (void)setROM:(NSData *)rom {
	_atari2600.set_rom(rom.length, (const uint8_t *)rom.bytes);
}

@end
