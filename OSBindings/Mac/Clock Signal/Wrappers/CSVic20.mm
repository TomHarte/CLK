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

- (void)setKernelROM:(nonnull NSData *)rom {
}

- (void)setBASICROM:(nonnull NSData *)rom {
}

- (void)setCharactersROM:(nonnull NSData *)rom {
}

- (void)setROM:(nonnull NSData *)rom address:(uint16_t)address {
}

- (void)setKey:(uint16_t)key isPressed:(BOOL)isPressed {
}

- (void)clearAllKeys {
}

@end
