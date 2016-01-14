//
//  CSMachine+Subclassing.h
//  Clock Signal
//
//  Created by Thomas Harte on 04/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import "CSMachine.h"
#include "CRT.hpp"
#include "Speaker.hpp"

@interface CSMachine (Subclassing)

- (BOOL)setSpeakerDelegate:(Outputs::Speaker::Delegate *)delegate sampleRate:(int)sampleRate;
- (void)setCRTDelegate:(Outputs::CRT::Delegate *)delegate;

- (void)doRunForNumberOfCycles:(int)numberOfCycles;
- (void)perform:(dispatch_block_t)action;

- (void)crt:(Outputs::CRT *)crt didEndFrame:(CRTFrame *)frame didDetectVSync:(BOOL)didDetectVSync;
- (void)speaker:(Outputs::Speaker *)speaker didCompleteSamples:(const uint16_t *)samples length:(int)length;

@end
