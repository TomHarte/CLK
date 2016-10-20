//
//  CSBestEffortUpdater.m
//  Clock Signal
//
//  Created by Thomas Harte on 16/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import "CSBestEffortUpdater.h"

@implementation CSBestEffortUpdater
{
	// these are inherently handled only by thread-safe constructions
	uint32_t _updateIsOngoing;
	dispatch_queue_t _serialDispatchQueue;

	// these are permitted for modification on _serialDispatchQueue only
	NSTimeInterval _previousTimeInterval;
	NSTimeInterval _cyclesError;
	BOOL _hasSkipped;
}

- (instancetype)init
{
	if(self = [super init])
	{
		_serialDispatchQueue = dispatch_queue_create("Best Effort Updater", DISPATCH_QUEUE_SERIAL);
	}
	return self;
}

- (void)update
{
	const uint32_t processingMask = 0x01;

	// Always post an -openGLView:didUpdateToTime: if a previous one isn't still ongoing. This is the hook upon which the substantial processing occurs.
	if(!OSAtomicTestAndSet(processingMask, &_updateIsOngoing))
	{
		dispatch_async(_serialDispatchQueue, ^{
			NSTimeInterval timeInterval = [NSDate timeIntervalSinceReferenceDate];
			if(_previousTimeInterval > DBL_EPSILON && timeInterval > _previousTimeInterval)
			{
				NSTimeInterval timeToRunFor = timeInterval - _previousTimeInterval;
				double cyclesToRunFor = timeToRunFor * self.clockRate + _cyclesError;

				_cyclesError = fmod(cyclesToRunFor, 1.0);
				NSUInteger integerCyclesToRunFor = (NSUInteger)MIN(cyclesToRunFor, self.clockRate * 0.5);

				// treat 'unlimited' as running at a factor of 10
				if(self.runAsUnlimited) integerCyclesToRunFor *= 10;
				[self.delegate bestEffortUpdater:self runForCycles:integerCyclesToRunFor didSkipPreviousUpdate:_hasSkipped];
			}
			_previousTimeInterval = timeInterval;
			_hasSkipped = NO;
			OSAtomicTestAndClear(processingMask, &_updateIsOngoing);
		});
	}
	else
	{
		dispatch_async(_serialDispatchQueue, ^{
			_hasSkipped = YES;
		});
	}
}

- (void)flush
{
	dispatch_sync(_serialDispatchQueue, ^{});
}

@end
