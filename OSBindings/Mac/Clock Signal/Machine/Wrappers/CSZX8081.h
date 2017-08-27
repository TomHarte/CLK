//
//  CSZX8081.h
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#import "CSMachine.h"
#import "CSKeyboardMachine.h"
#import "CSFastLoading.h"

@interface CSZX8081 : CSMachine <CSKeyboardMachine, CSFastLoading>

@property (nonatomic, assign) BOOL useFastLoadingHack;

@property (nonatomic, assign) BOOL useAutomaticTapeMotorControl;
@property (nonatomic, assign) BOOL tapeIsPlaying;

@end
