//
//  ScanTarget.m
//  Clock Signal
//
//  Created by Thomas Harte on 02/08/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#import "CSScanTarget.h"

#import <Metal/Metal.h>

@implementation CSScanTarget {
	id<MTLDevice> _device;
	id<MTLCommandQueue> _commandQueue;
}

- (nonnull instancetype)init {
	self = [super init];
	if(self) {
		_device = MTLCreateSystemDefaultDevice();
		_commandQueue = [_device newCommandQueue];
		NSLog(@"%@; %@", _device, _commandQueue);
	}
	return self;
}

@end
