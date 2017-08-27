//
//  CSZX8081+Instantiation.h
//  Clock Signal
//
//  Created by Thomas Harte on 27/08/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"
#import "CSZX8081.h"

@interface CSZX8081 (Instantiation)

- (instancetype)initWithIntendedTarget:(const StaticAnalyser::Target &)target;

@end
