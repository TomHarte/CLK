//
//  CSVic20.m
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import "CSVic20.h"

#import "Vic20.hpp"

@implementation CSVic20 {
	Vic20::Machine _vic20;
}

- (CRTMachine::Machine * const)machine {
	return &_vic20;
}

- (void)setROM:(nonnull NSData *)rom slot:(Vic20::ROMSlot)slot {
	_vic20.set_rom(slot, rom.length, (const uint8_t *)rom.bytes);
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

- (void)setPRG:(nonnull NSData *)prg {
	_vic20.add_prg(prg.length, (const uint8_t *)prg.bytes);
}

- (void)setKey:(uint16_t)key isPressed:(BOOL)isPressed {
}

- (void)clearAllKeys {
}

@end
