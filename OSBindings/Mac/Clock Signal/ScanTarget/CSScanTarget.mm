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

/// The shared resource options this app would most favour; applied as widely as possible.
constexpr MTLResourceOptions SharedResourceOptionsStandard = MTLResourceCPUCacheModeWriteCombined | MTLResourceStorageModeShared;

/// The shared resource options used for the write-area texture; on macOS it can't be MTLResourceStorageModeShared so this is a carve-out.
constexpr MTLResourceOptions SharedResourceOptionsTexture = MTLResourceCPUCacheModeWriteCombined | MTLResourceStorageModeManaged;

#define uniforms() reinterpret_cast<Uniforms *>(_uniformsBuffer.contents)

}

using BufferingScanTarget = Outputs::Display::BufferingScanTarget;

@implementation CSScanTarget {
	id<MTLCommandQueue> _commandQueue;

	id<MTLFunction> _vertexShader;
	id<MTLFunction> _fragmentShader;
	id<MTLRenderPipelineState> _scanPipeline;

	// Buffers.
	id<MTLBuffer> _uniformsBuffer;

	id<MTLBuffer> _scansBuffer;
	id<MTLBuffer> _linesBuffer;
	id<MTLBuffer> _writeAreaBuffer;

	// Textures.
	id<MTLTexture> _writeAreaTexture;
	size_t _bytesPerInputPixel;
	size_t _totalTextureBytes;

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
			options:SharedResourceOptionsStandard];
		_linesBuffer = [view.device
			newBufferWithLength:sizeof(Outputs::Display::BufferingScanTarget::Line)*NumBufferedLines
			options:SharedResourceOptionsStandard];
		_writeAreaBuffer = [view.device
			newBufferWithLength:BufferingScanTarget::WriteAreaWidth*BufferingScanTarget::WriteAreaHeight*4
			options:SharedResourceOptionsTexture];

		// Install all that storage in the buffering scan target.
		_scanTarget.set_write_area(reinterpret_cast<uint8_t *>(_writeAreaBuffer.contents));
		_scanTarget.set_line_buffer(reinterpret_cast<BufferingScanTarget::Line *>(_linesBuffer.contents), _lineMetadataBuffer, NumBufferedLines);
		_scanTarget.set_scan_buffer(reinterpret_cast<BufferingScanTarget::Scan *>(_scansBuffer.contents), NumBufferedScans);

		// Set initial aspect-ratio multiplier.
		[self mtkView:view drawableSizeWillChange:view.drawableSize];
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
	const Outputs::Display::ScanTarget::Modals *const newModals = _scanTarget.new_modals();
	if(newModals) {
		uniforms()->scale[0] = newModals->output_scale.x;
		uniforms()->scale[1] = newModals->output_scale.y;
		uniforms()->lineWidth = 1.0f / newModals->expected_vertical_lines;

		// TODO: obey the rest of the modals generally.

		// Generate the appropriate input texture.
		MTLPixelFormat pixelFormat;
		_bytesPerInputPixel = size_for_data_type(newModals->input_data_type);
		switch(_bytesPerInputPixel) {
			default:
			case 1: pixelFormat = MTLPixelFormatR8Unorm;	break;
			case 2: pixelFormat = MTLPixelFormatRG8Unorm;	break;
			case 4: pixelFormat = MTLPixelFormatRGBA8Unorm;	break;
		}
		MTLTextureDescriptor *const textureDescriptor = [MTLTextureDescriptor
			texture2DDescriptorWithPixelFormat:pixelFormat
			width:BufferingScanTarget::WriteAreaWidth
			height:BufferingScanTarget::WriteAreaHeight
			mipmapped:NO];
		textureDescriptor.resourceOptions = SharedResourceOptionsTexture;

		// TODO: the call below is the only reason why this project now requires macOS 10.13; is it all that helpful versus just uploading each frame?
		const NSUInteger bytesPerRow = BufferingScanTarget::WriteAreaWidth * _bytesPerInputPixel;
		_writeAreaTexture = [_writeAreaBuffer
			newTextureWithDescriptor:textureDescriptor
			offset:0
			bytesPerRow:bytesPerRow];
		_totalTextureBytes = bytesPerRow * BufferingScanTarget::WriteAreaHeight;

		// Generate pipeline.
		id<MTLLibrary> library = [view.device newDefaultLibrary];
		MTLRenderPipelineDescriptor *pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
		pipelineDescriptor.colorAttachments[0].pixelFormat = view.colorPixelFormat;

		// TODO: logic somewhat more complicated than this, probably
		pipelineDescriptor.vertexFunction = [library newFunctionWithName:@"scanVertexMain"];
		switch(newModals->input_data_type) {
			default:
				pipelineDescriptor.fragmentFunction = [library newFunctionWithName:@"scanFragmentMainRGB"];
			break;
			case Outputs::Display::InputDataType::Luminance1:
				pipelineDescriptor.fragmentFunction = [library newFunctionWithName:@"scanFragmentMainL1"];
			break;
		}

		_scanPipeline = [view.device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:nil];
	}

	// Generate a command encoder for the view.
	id <MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];
	MTLRenderPassDescriptor *const descriptor = view.currentRenderPassDescriptor;
	id <MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:descriptor];

	// Drawing. Just scans.
	[encoder setRenderPipelineState:_scanPipeline];

	[encoder setFragmentTexture:_writeAreaTexture atIndex:0];
	[encoder setVertexBuffer:_scansBuffer offset:0 atIndex:0];
	[encoder setVertexBuffer:_uniformsBuffer offset:0 atIndex:1];

	_scanTarget.perform([=] (const BufferingScanTarget::OutputArea &outputArea) {
		// Ensure texture changes are noted.
		const auto writeAreaModificationStart = size_t(outputArea.start.write_area_x + outputArea.start.write_area_y * 2048) * _bytesPerInputPixel;
		const auto writeAreaModificationEnd = size_t(outputArea.end.write_area_x + outputArea.end.write_area_y * 2048) * _bytesPerInputPixel;
		if(writeAreaModificationStart != writeAreaModificationEnd) {
			if(writeAreaModificationStart < writeAreaModificationEnd) {
				[_writeAreaBuffer didModifyRange:NSMakeRange(writeAreaModificationStart, writeAreaModificationEnd - writeAreaModificationStart)];
			} else {
				[_writeAreaBuffer didModifyRange:NSMakeRange(writeAreaModificationStart, _totalTextureBytes - writeAreaModificationStart)];
				if(writeAreaModificationEnd) {
					[_writeAreaBuffer didModifyRange:NSMakeRange(0, writeAreaModificationEnd)];
				}
			}

		}

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

	// Register the drawable's presentation, finalise and commit.
	[commandBuffer presentDrawable:view.currentDrawable];
	[commandBuffer commit];
}

-  (Outputs::Display::ScanTarget *)scanTarget {
	return &_scanTarget;
}

@end
