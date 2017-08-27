//
//  CSMachine+Subclassing.h
//  Clock Signal
//
//  Created by Thomas Harte on 04/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import "CSMachine.h"
#include "CRTMachine.hpp"

@interface CSMachine (Subclassing)

- (void)setupOutputWithAspectRatio:(float)aspectRatio;
@end
