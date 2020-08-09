//
//  ScanTarget.m
//  Clock Signal
//
//  Created by Thomas Harte on 02/08/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#import "CSScanTarget.h"

#include <atomic>
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

#define uniforms() reinterpret_cast<Uniforms *>(_uniformsBuffer.contents)

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

	// The scan target in C++-world terms and the non-GPU storage for it.
	BufferingScanTarget _scanTarget;
	BufferingScanTarget::LineMetadata _lineMetadataBuffer[NumBufferedLines];
	std::atomic_bool _isDrawing;
}

- (nonnull instancetype)initWithView:(nonnull MTKView *)view {
	self = [super init];
	if(self) {
		_commandQueue = [view.device newCommandQueue];

		// Allocate space for uniforms.
		_uniformsBuffer = [view.device
			newBufferWithLength:sizeof(Uniforms)
			options:MTLResourceCPUCacheModeWriteCombined | MTLResourceStorageModeShared];

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

/*!
 @method mtkView:drawableSizeWillChange:
 @abstract Called whenever the drawableSize of the view will change
 @discussion Delegate can recompute view and projection matricies or regenerate any buffers to be compatible with the new view size or resolution
 @param view MTKView which called this method
 @param size New drawable size in pixels
 */
- (void)mtkView:(nonnull MTKView *)view drawableSizeWillChange:(CGSize)size {
	uniforms()->aspectRatioMultiplier = float((4.0 / 3.0) / (size.width / size.height));
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

	const Outputs::Display::ScanTarget::Modals *const newModals = _scanTarget.new_modals();
	if(newModals) {
		uniforms()->scale[0] = newModals->output_scale.x;
		uniforms()->scale[1] = newModals->output_scale.y;
		uniforms()->lineWidth = 1.0f / newModals->expected_vertical_lines;

		// TODO: establish at least a texture. Obey the rest of the modals generally.
	}

	// Drawing. Just the test triangle, as described above.
	[encoder setRenderPipelineState:_gouraudPipeline];

	[encoder setVertexBuffer:_scansBuffer offset:0 atIndex:0];
	[encoder setVertexBuffer:_uniformsBuffer offset:0 atIndex:1];

	_scanTarget.perform([=] (const BufferingScanTarget::OutputArea &outputArea) {
		// TEMPORARY: just draw the scans.
		if(outputArea.start.scan != outputArea.end.scan) {
			if(outputArea.start.scan < outputArea.end.scan) {
				[encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4 instanceCount:outputArea.end.scan - outputArea.start.scan baseInstance:outputArea.start.scan];
			} else {
				[encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4 instanceCount:NumBufferedScans - outputArea.start.scan baseInstance:outputArea.start.scan];
				if(outputArea.end.scan) {
					[encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4 instanceCount:outputArea.end.scan];
				}
			}
		}
	});

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
