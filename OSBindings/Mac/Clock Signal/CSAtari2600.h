//
//  Atari2600.h
//  Clock Signal
//
//  Created by Thomas Harte on 14/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "CSCathodeRayView.h"
#include "Atari2600Inputs.h"

@interface CSAtari2600 : NSObject

@property (nonatomic, weak) CSCathodeRayView *view;

- (void)runForNumberOfCycles:(int)cycles;
- (void)setROM:(NSData * __nonnull)rom;

- (void)setState:(BOOL)state forDigitalInput:(Atari2600DigitalInput)digitalInput;
- (void)setResetLineEnabled:(BOOL)enabled;

@end
