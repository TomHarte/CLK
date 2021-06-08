//
//  CSAppleII.m
//  Clock Signal
//
//  Created by Thomas Harte on 07/06/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#import "CSAppleII.h"

#include "AppleII.hpp"

@implementation CSAppleII {
	Apple::II::Machine *_appleII;
	__weak CSMachine *_machine;
}

- (instancetype)initWithAppleII:(void *)appleII owner:(CSMachine *)machine {
	self = [super init];
	if(self) {
		_appleII = (Apple::II::Machine *)appleII;
		_machine = machine;
	}
	return self;
}

#pragma mark - Options

- (void)setUseSquarePixels:(BOOL)useSquarePixels {
	Configurable::Device *const configurable = dynamic_cast<Configurable::Device *>(_appleII);
#ifndef NDEBUG
	assert(configurable);
#endif

	@synchronized(_machine) {
		auto options = configurable->get_options();
#ifndef NDEBUG
		assert(dynamic_cast<Apple::II::Machine::Options *>(options.get()));
#endif

		auto appleii_configurable = static_cast<Apple::II::Machine::Options *>(options.get());
		appleii_configurable->use_square_pixels = useSquarePixels;
		configurable->set_options(options);
	}
}

- (BOOL)useSquarePixels {
	Configurable::Device *const configurable = dynamic_cast<Configurable::Device *>(_appleII);

	@synchronized(_machine) {
		auto options = configurable->get_options();
		auto appleii_configurable = dynamic_cast<Apple::II::Machine::Options *>(options.get());
		return appleii_configurable->use_square_pixels ? YES : NO;
	}
}

@end
