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
constexpr size_t NumBufferedLines = 2048;

}

using BufferingScanTarget = Outputs::Display::BufferingScanTarget;

@implementation CSScanTarget {
	id<MTLCommandQueue> _commandQueue;

	id<MTLFunction> _vertexShader;
	id<MTLFunction> _fragmentShader;
	id<MTLRenderPipelineState> _gouraudPipeline;

	// Buffers.
	id<MTLBuffer> _uniformsBuffer;

	id<MTLBuffer> _scansBuffer;
	id<MTLBuffer> _linesBuffer;
	id<MTLBuffer> _writeAreaBuffer;

	// Current uniforms.
	Uniforms _uniforms;

	// The scan target in C++-world terms and the non-GPU storage for it.
	BufferingScanTarget _scanTarget;
	BufferingScanTarget::LineMetadata _lineMetadataBuffer[NumBufferedLines];
}

- (nonnull instancetype)initWithView:(nonnull MTKView *)view {
	self = [super init];
	if(self) {
		_commandQueue = [view.device newCommandQueue];

		// Allocate space for uniforms and FOR TEST PURPOSES set some.
		_uniformsBuffer = [view.device newBufferWithLength:16 options:MTLResourceCPUCacheModeWriteCombined];
		_uniforms.scale[0] = 1024;
		_uniforms.scale[1] = 1024;
		_uniforms.lineWidth = 1.0f / 312.0f;
		_uniforms.aspectRatioMultiplier = 1.0f;
		[self setUniforms];

		// Allocate buffers for scans and lines and for the write area texture.
		_scansBuffer = [view.device
			newBufferWithLength:sizeof(Outputs::Display::BufferingScanTarget::Scan)*NumBufferedScans
			options:MTLResourceCPUCacheModeWriteCombined | MTLResourceStorageModeShared];
		_linesBuffer = [view.device
			newBufferWithLength:sizeof(Outputs::Display::BufferingScanTarget::Line)*NumBufferedLines
			options:MTLResourceCPUCacheModeWriteCombined | MTLResourceStorageModeShared];
		_writeAreaBuffer = [view.device
			newBufferWithLength:BufferingScanTarget::WriteAreaWidth*BufferingScanTarget::WriteAreaHeight*4
			options:MTLResourceCPUCacheModeWriteCombined | MTLResourceStorageModeShared];

		// Install all that storage in the buffering scan target.
		_scanTarget.set_write_area(reinterpret_cast<uint8_t *>(_writeAreaBuffer.contents));
		_scanTarget.set_line_buffer(reinterpret_cast<BufferingScanTarget::Line *>(_linesBuffer.contents), _lineMetadataBuffer, NumBufferedLines);
		_scanTarget.set_scan_buffer(reinterpret_cast<BufferingScanTarget::Scan *>(_scansBuffer.contents), NumBufferedScans);

		// TEST ONLY: set some test data.
		[self setTestScans];

		// Generate TEST pipeline.
		id<MTLLibrary> library = [view.device newDefaultLibrary];
		MTLRenderPipelineDescriptor *pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
		pipelineDescriptor.vertexFunction = [library newFunctionWithName:@"scanVertexMain"];
		pipelineDescriptor.fragmentFunction = [library newFunctionWithName:@"scanFragmentMain"];
		pipelineDescriptor.colorAttachments[0].pixelFormat = view.colorPixelFormat;
		_gouraudPipeline = [view.device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:nil];
	}

	return self;
}

- (void)setUniforms {
	memcpy(_uniformsBuffer.contents, &_uniforms, sizeof(Uniforms));
}

- (void)setTestScans {
	BufferingScanTarget::Scan scans[2];
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

	[encoder setVertexBuffer:_scansBuffer offset:0 atIndex:0];
	[encoder setVertexBuffer:_uniformsBuffer offset:0 atIndex:1];

	[encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4 instanceCount:2];

	// Complete encoding.
	[encoder endEncoding];

	// "Register the drawable's presentation".
	[commandBuffer presentDrawable:view.currentDrawable];

	// Finalise and commit.
	[commandBuffer commit];
}

-  (Outputs::Display::ScanTarget *)scanTarget {
	return &_scanTarget;
}

@end
