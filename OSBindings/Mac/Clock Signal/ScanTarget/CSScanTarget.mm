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
	id<MTLBuffer> _verticesBuffer;
	id<MTLRenderPipelineState> _gouraudPipeline;
}

- (nonnull instancetype)initWithView:(nonnull MTKView *)view {
	self = [super init];
	if(self) {
		_commandQueue = [view.device newCommandQueue];

		// Generate some static buffers. AS A TEST.
		constexpr float vertices[] = {
			0.0f,	0.5f,	0.0f,	1.0f,	// Position.
			1.0f, 	0.0f, 	0.0f, 	1.0f,	// Colour.

			-0.5f,	-0.5f,	0.0f,	1.0f,
			0.0f, 	1.0f, 	0.0f, 	1.0f,

			0.5f,	-0.5f,	0.0f,	1.0f,
			0.0f, 	0.0f, 	1.0f, 	1.0f,
		};
		_verticesBuffer = [view.device newBufferWithBytes:vertices length:sizeof(vertices) options:MTLResourceOptionCPUCacheModeDefault];

		MTLVertexDescriptor *vertexDescriptor = [[MTLVertexDescriptor alloc] init];

		// Position.
		vertexDescriptor.attributes[0].bufferIndex = 0;
		vertexDescriptor.attributes[0].offset = 0;
		vertexDescriptor.attributes[0].format = MTLVertexFormatFloat4;

		// Colour.
		vertexDescriptor.attributes[1].bufferIndex = 0;
		vertexDescriptor.attributes[1].offset = sizeof(float)*4;
		vertexDescriptor.attributes[1].format = MTLVertexFormatFloat4;

		// Total vertex size.
        vertexDescriptor.layouts[0].stride = sizeof(float) * 8;

		// Generate TEST pipeline.
		id<MTLLibrary> library = [view.device newDefaultLibrary];
		MTLRenderPipelineDescriptor *pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
		pipelineDescriptor.vertexFunction = [library newFunctionWithName:@"vertex_main"];
		pipelineDescriptor.fragmentFunction = [library newFunctionWithName:@"fragment_main"];
		pipelineDescriptor.colorAttachments[0].pixelFormat = view.colorPixelFormat;
		pipelineDescriptor.vertexDescriptor = vertexDescriptor;
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
	[encoder setVertexBuffer:_verticesBuffer offset:0 atIndex:0];
	[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3 instanceCount:1];

	// Complete encoding.
	[encoder endEncoding];

	// "Register the drawable's presentation".
	[commandBuffer presentDrawable:view.currentDrawable];

	// Finalise and commit.
	[commandBuffer commit];
}

@end
