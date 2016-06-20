//
//  Atari2600.h
//  Clock Signal
//
//  Created by Thomas Harte on 14/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#include "CSMachine.h"
#include "Atari2600Inputs.h"

@interface CSAtari2600 : CSMachine

- (void)setROM:(nonnull NSData *)rom;
- (void)setState:(BOOL)state forDigitalInput:(Atari2600DigitalInput)digitalInput;
- (void)setResetLineEnabled:(BOOL)enabled;

@property (nonatomic, assign) BOOL colourButton;
@property (nonatomic, assign) BOOL leftPlayerDifficultyButton;
@property (nonatomic, assign) BOOL rightPlayerDifficultyButton;
- (void)pressResetButton;
- (void)pressSelectButton;

@end
