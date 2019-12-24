//
//  DigitalPhaseLockedLoopBridge.m
//  Clock Signal
//
//  Created by Thomas Harte on 12/07/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#import "DigitalPhaseLockedLoopBridge.h"

#include "DigitalPhaseLockedLoop.hpp"
#include <memory>

@interface DigitalPhaseLockedLoopBridge(BitPushing)
- (void)pushBit:(int)value;
@end

class DigitalPhaseLockedLoopDelegate: public Storage::DigitalPhaseLockedLoop::Delegate {
	public:
		__weak DigitalPhaseLockedLoopBridge *bridge;

		void digital_phase_locked_loop_output_bit(int value) {
			[bridge pushBit:value ? 1 : 0];
		}
};

@implementation DigitalPhaseLockedLoopBridge {
	std::unique_ptr<Storage::DigitalPhaseLockedLoop> _digitalPhaseLockedLoop;
	DigitalPhaseLockedLoopDelegate _delegate;
}

- (instancetype)initWithClocksPerBit:(NSUInteger)clocksPerBit historyLength:(NSUInteger)historyLength {
	self = [super init];
	if(self) {
		_digitalPhaseLockedLoop = std::make_unique<Storage::DigitalPhaseLockedLoop>((unsigned int)clocksPerBit, (unsigned int)historyLength);
		_delegate.bridge = self;
		_digitalPhaseLockedLoop->set_delegate(&_delegate);
	}
	return self;
}

- (void)runForCycles:(NSUInteger)cycles {
	_digitalPhaseLockedLoop->run_for(Cycles((int)cycles));
}

- (void)addPulse {
	_digitalPhaseLockedLoop->add_pulse();
}

- (void)pushBit:(int)value {
	_stream = (_stream << 1) | value;
}

@end
