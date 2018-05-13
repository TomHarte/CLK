//
//  Atari2600.h
//  Clock Signal
//
//  Created by Thomas Harte on 14/07/2015.
//  Copyright 2015 Thomas Harte. All rights reserved.
//

@class CSAtari2600;
#import "CSMachine.h"

@interface CSAtari2600 : NSObject

- (instancetype)initWithAtari2600:(void *)atari2600 owner:(CSMachine *)machine;

@property (nonatomic, assign) BOOL colourButton;
@property (nonatomic, assign) BOOL leftPlayerDifficultyButton;
@property (nonatomic, assign) BOOL rightPlayerDifficultyButton;
- (void)pressResetButton;
- (void)pressSelectButton;

@end
