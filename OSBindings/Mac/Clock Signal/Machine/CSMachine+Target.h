//
//  Target.h
//  Clock Signal
//
//  Created by Thomas Harte on 31/08/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>

#include "StaticAnalyser.hpp"

@interface CSMachine(Target)

- (void)applyMedia:(const Analyser::Static::Media &)media;

@end
