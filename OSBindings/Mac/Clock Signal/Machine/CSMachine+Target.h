//
//  Target.h
//  Clock Signal
//
//  Created by Thomas Harte on 31/08/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>

#include "StaticAnalyser.hpp"

@interface CSMachine(Target)

- (void)applyTarget:(StaticAnalyser::Target)target;

@end
