//
//  TestMachine+ForSubclassEyesOnly.h
//  Clock Signal
//
//  Created by Thomas Harte on 03/06/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#import "TestMachine.h"
#include "AllRAMProcessor.hpp"

@interface CSTestMachine (ForSubclassEyesOnly)
- (CPU::AllRAMProcessor *)processor;
@end
