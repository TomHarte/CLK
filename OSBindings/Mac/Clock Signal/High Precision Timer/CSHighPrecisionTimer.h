//
//  CSHighPrecisionTimer.h
//  Clock Signal
//
//  Created by Thomas Harte on 07/03/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>

/*!
	Provides a high-precision timer; provide it with a block and an
	interval, and it will ensure the block is performed as regularly
	as the system will allow at the specified intervals.

	The block will be executed on an arbitrary thread.
*/
@interface CSHighPrecisionTimer: NSObject

/// Initialises a new instance of the high precision timer; the timer will begin
/// ticking immediately.
///
/// @param task The block to perform each time the timer fires.
/// @param interval The interval at which to fire the timer, in nanoseconds.
- (instancetype)initWithTask:(dispatch_block_t)task interval:(uint64_t)interval;

/// Stops the timer.
- (void)invalidate;

@end
