//
//  ScanTarget.m
//  Clock Signal
//
//  Created by Thomas Harte on 02/08/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#import "CSScanTarget.h"

#import <Metal/Metal.h>
#include "BufferingScanTarget.hpp"

namespace {

struct Uniforms {
	int32_t scale[2];
	float lineWidth;
};

constexpr size_t NumBufferedScans = 2048;

}

@implementation CSScanTarget {
	id<MTLCommandQueue> _commandQueue;

	id<MTLFunction> _vertexShader;
	id<MTLFunction> _fragmentShader;
	id<MTLRenderPipelineState> _gouraudPipeline;

	// Buffers.
	id<MTLBuffer> _quadBuffer;		// i.e. four vertices defining a quad.
	id<MTLBuffer> _uniformsBuffer;
	id<MTLBuffer> _scansBuffer;
}

- (nonnull instancetype)initWithView:(nonnull MTKView *)view {
	self = [super init];
	if(self) {
		_commandQueue = [view.device newCommandQueue];

		// Install the standard quad.
		constexpr float vertices[] = {
			-0.9f,	-0.9f,
			-0.9f,	0.9f,
			0.9f,	-0.9f,
			0.9f,	0.9f,
		};
		_quadBuffer = [view.device newBufferWithBytes:vertices length:sizeof(vertices) options:MTLResourceCPUCacheModeDefaultCache];

		// Allocate space for uniforms.
		_uniformsBuffer = [view.device newBufferWithLength:16 options:MTLResourceCPUCacheModeWriteCombined];
		const Uniforms testUniforms = {
			.scale = {1024, 1024},
			.lineWidth = 0.1f
		};
		[self setUniforms:testUniforms];

		// Allocate a large buffer for scans.
		_scansBuffer = [view.device
			newBufferWithLength:sizeof(Outputs::Display::BufferingScanTarget::Scan)*NumBufferedScans
			options:MTLResourceCPUCacheModeWriteCombined | MTLResourceStorageModeShared];
		[self setTestScans];

		// The quad buffer has only 2d positions; the scan buffer is a bit more complicated
		MTLVertexDescriptor *vertexDescriptor = [[MTLVertexDescriptor alloc] init];
		vertexDescriptor.attributes[0].bufferIndex = 0;
		vertexDescriptor.attributes[0].offset = 0;
		vertexDescriptor.attributes[0].format = MTLVertexFormatFloat2;
		vertexDescriptor.layouts[0].stride = sizeof(float)*2;

		// Create a scans buffer, and for now just put two in there.

		// Generate TEST pipeline.
		id<MTLLibrary> library = [view.device newDefaultLibrary];
		MTLRenderPipelineDescriptor *pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
		pipelineDescriptor.vertexFunction = [library newFunctionWithName:@"scanVertexMain"];
		pipelineDescriptor.fragmentFunction = [library newFunctionWithName:@"scanFragmentMain"];
		pipelineDescriptor.colorAttachments[0].pixelFormat = view.colorPixelFormat;
		pipelineDescriptor.vertexDescriptor = vertexDescriptor;
		_gouraudPipeline = [view.device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:nil];
	}

	return self;
}

- (void)setUniforms:(const Uniforms &)uniforms {
	memcpy(_uniformsBuffer.contents, &uniforms, sizeof(Uniforms));
}

- (void)setTestScans {
	Outputs::Display::BufferingScanTarget::Scan scans[2];
	scans[0].scan.end_points[0].x = 0;
	scans[0].scan.end_points[0].y = 0;
	scans[0].scan.end_points[1].x = 1024;
	scans[0].scan.end_points[1].y = 256;

	scans[1].scan.end_points[0].x = 0;
	scans[1].scan.end_points[0].y = 768;
	scans[1].scan.end_points[1].x = 512;
	scans[1].scan.end_points[1].y = 128;

	memcpy(_scansBuffer.contents, scans, sizeof(scans));
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

	[encoder setVertexBuffer:_quadBuffer offset:0 atIndex:0];
	[encoder setVertexBuffer:_uniformsBuffer offset:0 atIndex:1];
	[encoder setVertexBuffer:_scansBuffer offset:0 atIndex:2];

	[encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4 instanceCount:2];

	// Complete encoding.
	[encoder endEncoding];

	// "Register the drawable's presentation".
	[commandBuffer presentDrawable:view.currentDrawable];

	// Finalise and commit.
	[commandBuffer commit];
}

@end
