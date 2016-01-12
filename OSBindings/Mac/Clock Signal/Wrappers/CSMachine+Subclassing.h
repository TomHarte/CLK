//
//  CSMachine+Subclassing.h
//  Clock Signal
//
//  Created by Thomas Harte on 04/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import "CSMachine.h"
#include "../../../../Outputs/CRT.hpp"

@interface CSMachine (Subclassing)

- (void)setCRTDelegate:(Outputs::CRT::Delegate *)delegate;
- (void)doRunForNumberOfCycles:(int)numberOfCycles;
- (void)crt:(Outputs::CRT *)crt didEndFrame:(CRTFrame *)frame didDetectVSync:(BOOL)didDetectVSync;

- (void)perform:(dispatch_block_t)action;

@end
