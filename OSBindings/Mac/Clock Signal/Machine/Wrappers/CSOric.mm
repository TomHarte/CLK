//
//  CSOric.m
//  Clock Signal
//
//  Created by Thomas Harte on 11/10/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import "CSOric.h"

#include "Oric.hpp"
#include "StaticAnalyser.hpp"

#import "CSMachine+Subclassing.h"
#import "NSData+StdVector.h"
#import "NSBundle+DataResource.h"

@implementation CSOric {
	Oric::Machine _oric;
}

- (CRTMachine::Machine * const)machine {
	return &_oric;
}

@end
