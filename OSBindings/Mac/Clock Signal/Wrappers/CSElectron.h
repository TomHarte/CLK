//
//  CSElectron.h
//  Clock Signal
//
//  Created by Thomas Harte on 04/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "CSMachine.h"
#import "KeyCodes.h"

@interface CSElectron : CSMachine

- (void)setOSROM:(nonnull NSData *)rom;
- (void)setBASICROM:(nonnull NSData *)rom;

- (void)setKey:(uint16_t)key isPressed:(BOOL)isPressed;

@end
