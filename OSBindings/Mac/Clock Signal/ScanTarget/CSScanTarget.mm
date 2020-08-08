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
	float aspectRatioMultiplier;
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

	// Current uniforms.
	Uniforms _uniforms;
}

- (nonnull instancetype)initWithView:(nonnull MTKView *)view {
	self = [super init];
	if(self) {
		_commandQueue = [view.device newCommandQueue];

		// Install the standard quad.
		constexpr float vertices[] = {
			0.0f,	0.0f,
			0.0f,	1.0f,
			1.0f,	0.0f,
			1.0f,	1.0f,
		};
		_quadBuffer = [view.device newBufferWithBytes:vertices length:sizeof(vertices) options:MTLResourceCPUCacheModeDefaultCache];

		// Allocate space for uniforms.
		_uniformsBuffer = [view.device newBufferWithLength:16 options:MTLResourceCPUCacheModeWriteCombined];
		_uniforms.scale[0] = 1024;
		_uniforms.scale[1] = 1024;
		_uniforms.lineWidth = 1.0f / 312.0f;
		_uniforms.aspectRatioMultiplier = 1.0f;
		[self setUniforms];

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

		// TODO: shouldn't I need to explain the Scan layout, too? Or do these things not need to be specified when compatible?

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

- (void)setUniforms {
	memcpy(_uniformsBuffer.contents, &_uniforms, sizeof(Uniforms));
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
	scans[1].scan.end_points[1].y = 512;

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
	_uniforms.aspectRatioMultiplier = float((4.0 / 3.0) / (size.width / size.height));
	[self setUniforms];
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
