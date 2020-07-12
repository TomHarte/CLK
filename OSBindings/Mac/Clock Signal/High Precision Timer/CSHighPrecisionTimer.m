//
//  CSHighPrecisionTimer.m
//  Clock Signal
//
//  Created by Thomas Harte on 07/03/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#import "CSHighPrecisionTimer.h"

@implementation CSHighPrecisionTimer {
	dispatch_source_t _timer;
	dispatch_queue_t _queue;
}

- (instancetype)initWithTask:(dispatch_block_t)task interval:(uint64_t)interval {
	self = [super init];
	if(self) {
		_queue = dispatch_queue_create("High precision timer queue", DISPATCH_QUEUE_SERIAL);
		_timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, _queue);
		if(_timer) {
			dispatch_source_set_timer(_timer, dispatch_walltime(NULL, 0), interval, 0);
			dispatch_source_set_event_handler(_timer, task);

			// TODO: dispatch_activate is preferred to dispatch_resume, but arrives only
			// as of macOS 10.12. So switch if/when upping the OS requirements.
			dispatch_resume(_timer);
		}
	}
	return self;
}

- (void)invalidate {
	NSConditionLock *lock = [[NSConditionLock alloc] initWithCondition:0];

	dispatch_source_set_cancel_handler(_timer, ^{
		[lock lock];
		[lock unlockWithCondition:1];
	});

	dispatch_source_cancel(_timer);
	[lock lockWhenCondition:1];
}

@end
