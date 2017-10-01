//
//  CSBestEffortUpdater.m
//  Clock Signal
//
//  Created by Thomas Harte on 16/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import "CSBestEffortUpdater.h"

#include <stdatomic.h>

@implementation CSBestEffortUpdater
{
	// these are inherently handled only by thread-safe constructions
	atomic_flag _updateIsOngoing;
	dispatch_queue_t _serialDispatchQueue;

	// these are permitted for modification on _serialDispatchQueue only
	NSTimeInterval _previousTimeInterval;
	NSTimeInterval _cyclesError;
	BOOL _hasSkipped;
	id<CSBestEffortUpdaterDelegate> _delegate;
}

- (instancetype)init
{
	if(self = [super init])
	{
		_serialDispatchQueue = dispatch_queue_create("Best Effort Updater", DISPATCH_QUEUE_SERIAL);

		// This is a workaround for assigning the correct initial value within Objective-C's form.
		atomic_flag initialFlagValue = ATOMIC_FLAG_INIT;
		_updateIsOngoing = initialFlagValue;
	}
	return self;
}

- (void)update {
	// Always post an -openGLView:didUpdateToTime: if a previous one isn't still ongoing. This is the hook upon which the substantial processing occurs.
	if(!atomic_flag_test_and_set(&_updateIsOngoing)) {
		dispatch_async(_serialDispatchQueue, ^{
			NSTimeInterval timeInterval = [NSDate timeIntervalSinceReferenceDate];
			if(_previousTimeInterval > DBL_EPSILON && timeInterval > _previousTimeInterval) {
				NSTimeInterval timeToRunFor = timeInterval - _previousTimeInterval;
				double cyclesToRunFor = timeToRunFor * self.clockRate + _cyclesError;

				_cyclesError = fmod(cyclesToRunFor, 1.0);
				NSUInteger integerCyclesToRunFor = (NSUInteger)MIN(cyclesToRunFor, self.clockRate * 0.5);

				// treat 'unlimited' as running at a factor of 10
				if(self.runAsUnlimited) integerCyclesToRunFor *= 10;
				[_delegate bestEffortUpdater:self runForCycles:integerCyclesToRunFor didSkipPreviousUpdate:_hasSkipped];
			}
			_previousTimeInterval = timeInterval;
			_hasSkipped = NO;
			atomic_flag_clear(&_updateIsOngoing);
		});
	} else {
		dispatch_async(_serialDispatchQueue, ^{
			_hasSkipped = YES;
		});
	}
}

- (void)flush {
	dispatch_sync(_serialDispatchQueue, ^{});
}

- (void)setDelegate:(id<CSBestEffortUpdaterDelegate>)delegate {
	dispatch_sync(_serialDispatchQueue, ^{
		_delegate = delegate;
	});
}

- (id<CSBestEffortUpdaterDelegate>)delegate {
	__block id<CSBestEffortUpdaterDelegate> delegate;
	dispatch_sync(_serialDispatchQueue, ^{
		delegate = _delegate;
	});
	return delegate;
}

@end
