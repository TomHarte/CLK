//
//  CSVic20.m
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import "CSVic20.h"

#import "Vic20.hpp"

@implementation CSVic20 {
	Vic20::Machine _vic20;
}

- (CRTMachine::Machine * const)machine {
	return &_vic20;
}

@end
