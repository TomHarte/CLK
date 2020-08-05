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

	// TEST ONLY: to check that I'm drawing _something_, I'm heading towards ye standard
	// Gouraud shading triangle. https://metalbyexample.com/up-and-running-2/ is providing
	// much of the inspiration, albeit that I'm proceeding via MKLView.
	id<MTLFunction> _vertexShader;
	id<MTLFunction> _fragmentShader;
	id<MTLBuffer> _positionBuffer;
	id<MTLBuffer> _colourBuffer;
	id<MTLRenderPipelineState> _gouraudPipeline;
}

- (nonnull instancetype)initWithView:(nonnull MTKView *)view {
	self = [super init];
	if(self) {
		_commandQueue = [view.device newCommandQueue];

		// Generate some static buffers. AS A TEST.
		constexpr float positions[] = {
			0.0f,	0.5f,	0.0f,	1.0f,
			-0.5f,	-0.5f,	0.0f,	1.0f,
			0.5f,	-0.5f,	0.0f,	1.0f,
		};
		constexpr float colours[] = {
			1.0f, 0.0f, 0.0f, 1.0f,
			0.0f, 1.0f, 0.0f, 1.0f,
			0.0f, 0.0f, 1.0f, 1.0f,
		};
		_positionBuffer = [view.device newBufferWithBytes:positions length:sizeof(positions) options:MTLResourceOptionCPUCacheModeDefault];
		_colourBuffer = [view.device newBufferWithBytes:colours length:sizeof(colours) options:MTLResourceOptionCPUCacheModeDefault];

		// Generate TEST pipeline.
		id<MTLLibrary> library = [view.device newDefaultLibrary];
		MTLRenderPipelineDescriptor *pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
		pipelineDescriptor.vertexFunction = [library newFunctionWithName:@"vertex_main"];
		pipelineDescriptor.fragmentFunction = [library newFunctionWithName:@"fragment_main"];
		pipelineDescriptor.colorAttachments[0].pixelFormat = view.colorPixelFormat;
		_gouraudPipeline = [view.device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:nil];
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
	// I think (?) I don't care about this; the MKLView has already handled resizing the backing,
	// which will naturally change the viewport.
}

/*!
 @method drawInMTKView:
 @abstract Called on the delegate when it is asked to render into the view
 @discussion Called on the delegate when it is asked to render into the view
 */
- (void)drawInMTKView:(nonnull MTKView *)view {
	// Generate a command encoder for the view.
	id <MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];
	MTLRenderPassDescriptor *const descriptor = view.currentRenderPassDescriptor;
	id <MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:descriptor];

	// Drawing. Just the test triangle, as described above.
	[encoder setRenderPipelineState:_gouraudPipeline];
	[encoder setVertexBuffer:_positionBuffer offset:0 atIndex:0];
	[encoder setVertexBuffer:_colourBuffer offset:0 atIndex:1];
	[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3 instanceCount:1];

	// Complete encoding.
	[encoder endEncoding];

	// "Register the drawable's presentation".
	[commandBuffer presentDrawable:view.currentDrawable];

	// Finalise and commit.
	[commandBuffer commit];
}

@end
