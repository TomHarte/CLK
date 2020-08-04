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
	id<MTLCommandQueue> _commandQueue;
}

- (nonnull instancetype)initWithView:(nonnull MTKView *)view {
	self = [super init];
	if(self) {
		_commandQueue = [view.device newCommandQueue];
	}
	return self;
}

/*!
 @method mtkView:drawableSizeWillChange:
 @abstract Called whenever the drawableSize of the view will change
 @discussion Delegate can recompute view and projection matricies or regenerate any buffers to be compatible with the new view size or resolution
 @param view MTKView which called this method
 @param size New drawable size in pixels
 */
- (void)mtkView:(nonnull MTKView *)view drawableSizeWillChange:(CGSize)size {
	NSLog(@"New size: %@", NSStringFromSize(size));
}

/*!
 @method drawInMTKView:
 @abstract Called on the delegate when it is asked to render into the view
 @discussion Called on the delegate when it is asked to render into the view
 */
- (void)drawInMTKView:(nonnull MTKView *)view {
	id <MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];
	MTLRenderPassDescriptor *const descriptor = view.currentRenderPassDescriptor;
	id <MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:descriptor];

	// TODO: the drawing (!)

	[encoder endEncoding];

	// "Register the drawable's presentation".
	[commandBuffer presentDrawable:view.currentDrawable];

	// Finalise and commit.
	[commandBuffer commit];
}

@end
