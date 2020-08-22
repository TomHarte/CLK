//
//  ScanTarget.m
//  Clock Signal
//
//  Created by Thomas Harte on 02/08/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#import "CSScanTarget.h"

#import <Metal/Metal.h>

#include <atomic>

#include "BufferingScanTarget.hpp"
#include "FIRFilter.hpp"

namespace {

struct Uniforms {
	int32_t scale[2];
	float lineWidth;
	float aspectRatioMultiplier;
	simd::float3x3 toRGB;
	simd::float3x3 fromRGB;
	float zoom;
	simd::float2 offset;
	simd::float3 firCoefficients[8];
};

constexpr size_t NumBufferedScans = 2048;
constexpr size_t NumBufferedLines = 2048;

/// The shared resource options this app would most favour; applied as widely as possible.
constexpr MTLResourceOptions SharedResourceOptionsStandard = MTLResourceCPUCacheModeWriteCombined | MTLResourceStorageModeShared;

/// The shared resource options used for the write-area texture; on macOS it can't be MTLResourceStorageModeShared so this is a carve-out.
constexpr MTLResourceOptions SharedResourceOptionsTexture = MTLResourceCPUCacheModeWriteCombined | MTLResourceStorageModeManaged;

#define uniforms() reinterpret_cast<Uniforms *>(_uniformsBuffer.contents)

#define RangePerform(start, end, size, func)	\
	if(start != end) {	\
		if(start < end) {	\
			func(start, end-start);	\
		} else {	\
			func(start, size-start);	\
			if(end) {	\
				func(0, end);	\
			}	\
		}	\
	}

}

using BufferingScanTarget = Outputs::Display::BufferingScanTarget;

@implementation CSScanTarget {
	id<MTLCommandQueue> _commandQueue;

	id<MTLFunction> _vertexShader;
	id<MTLFunction> _fragmentShader;

	id<MTLRenderPipelineState> _composePipeline;
	id<MTLRenderPipelineState> _outputPipeline;
	id<MTLRenderPipelineState> _copyPipeline;
	id<MTLRenderPipelineState> _clearPipeline;

	// Buffers.
	id<MTLBuffer> _uniformsBuffer;

	id<MTLBuffer> _scansBuffer;
	id<MTLBuffer> _linesBuffer;
	id<MTLBuffer> _writeAreaBuffer;

	// Textures.
	id<MTLTexture> _writeAreaTexture;
	size_t _bytesPerInputPixel;
	size_t _totalTextureBytes;

	id<MTLTexture> _frameBuffer;
	MTLRenderPassDescriptor *_frameBufferRenderPass;

	id<MTLTexture> _compositionTexture;
	MTLRenderPassDescriptor *_compositionRenderPass;

	// The stencil buffer and the two ways it's used.
	id<MTLTexture> _frameBufferStencil;
	id<MTLDepthStencilState> _drawStencilState;		// Always draws, sets stencil to 1.
	id<MTLDepthStencilState> _clearStencilState;	// Draws only where stencil is 0, clears all to 0.

	// The composition pipeline, and whether it's in use.
	BOOL _isUsingCompositionPipeline;

	// The scan target in C++-world terms and the non-GPU storage for it.
	BufferingScanTarget _scanTarget;
	BufferingScanTarget::LineMetadata _lineMetadataBuffer[NumBufferedLines];
	std::atomic_flag _isDrawing;

	// The output view.
	__weak MTKView *_view;
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
		_view = view;
		[self mtkView:view drawableSizeWillChange:view.drawableSize];

		// Generate copy and clear pipelines.
		id<MTLLibrary> library = [_view.device newDefaultLibrary];
		MTLRenderPipelineDescriptor *const pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
		pipelineDescriptor.colorAttachments[0].pixelFormat = _view.colorPixelFormat;
		pipelineDescriptor.vertexFunction = [library newFunctionWithName:@"copyVertex"];
		pipelineDescriptor.fragmentFunction = [library newFunctionWithName:@"copyFragment"];
		_copyPipeline = [_view.device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:nil];

		pipelineDescriptor.fragmentFunction = [library newFunctionWithName:@"clearFragment"];
		pipelineDescriptor.stencilAttachmentPixelFormat = MTLPixelFormatStencil8;
		_clearPipeline = [_view.device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:nil];

		// Clear stencil: always write the reference value (of 0), but draw only where the stencil already
		// had that value.
		MTLDepthStencilDescriptor *depthStencilDescriptor = [[MTLDepthStencilDescriptor alloc] init];
		depthStencilDescriptor.frontFaceStencil.stencilCompareFunction = MTLCompareFunctionEqual;
		depthStencilDescriptor.frontFaceStencil.depthStencilPassOperation = MTLStencilOperationReplace;
		depthStencilDescriptor.frontFaceStencil.stencilFailureOperation = MTLStencilOperationReplace;
		_clearStencilState = [view.device newDepthStencilStateWithDescriptor:depthStencilDescriptor];

		// Create a composition texture up front.
		MTLTextureDescriptor *const textureDescriptor = [MTLTextureDescriptor
			texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
			width:2048		// This 'should do'.
			height:NumBufferedLines
			mipmapped:NO];
		textureDescriptor.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
		textureDescriptor.resourceOptions = MTLResourceStorageModePrivate;
		_compositionTexture = [view.device newTextureWithDescriptor:textureDescriptor];

		// Ensure the is-drawing flag is initially clear.
		_isDrawing.clear();
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
	[self setAspectRatio];

	// TODO: consider multisampling here? But it seems like you'd need another level of indirection
	// in order to maintain an ongoing buffer that supersamples only at the end.

	@synchronized(self) {
		// Generate a framebuffer and a stencil.
		MTLTextureDescriptor *const textureDescriptor = [MTLTextureDescriptor
			texture2DDescriptorWithPixelFormat:view.colorPixelFormat
			width:NSUInteger(size.width * view.layer.contentsScale)
			height:NSUInteger(size.height * view.layer.contentsScale)
			mipmapped:NO];
		textureDescriptor.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
		textureDescriptor.resourceOptions = MTLResourceStorageModePrivate;
		_frameBuffer = [view.device newTextureWithDescriptor:textureDescriptor];

		MTLTextureDescriptor *const stencilTextureDescriptor = [MTLTextureDescriptor
			texture2DDescriptorWithPixelFormat:MTLPixelFormatStencil8
			width:NSUInteger(size.width * view.layer.contentsScale)
			height:NSUInteger(size.height * view.layer.contentsScale)
			mipmapped:NO];
		stencilTextureDescriptor.usage = MTLTextureUsageRenderTarget;
		stencilTextureDescriptor.resourceOptions = MTLResourceStorageModePrivate;
		_frameBufferStencil = [view.device newTextureWithDescriptor:stencilTextureDescriptor];

		// Generate a render pass with that framebuffer and stencil.
		_frameBufferRenderPass = [[MTLRenderPassDescriptor alloc] init];
		_frameBufferRenderPass.colorAttachments[0].texture = _frameBuffer;
		_frameBufferRenderPass.colorAttachments[0].loadAction = MTLLoadActionLoad;
		_frameBufferRenderPass.colorAttachments[0].storeAction = MTLStoreActionStore;

		_frameBufferRenderPass.stencilAttachment.clearStencil = 0;
		_frameBufferRenderPass.stencilAttachment.texture = _frameBufferStencil;
		_frameBufferRenderPass.stencilAttachment.loadAction = MTLLoadActionLoad;
		_frameBufferRenderPass.stencilAttachment.storeAction = MTLStoreActionStore;

		// Establish intended stencil useage; it's only to track which pixels haven't been painted
		// at all at the end of every frame. So: always paint, and replace the stored stencil value
		// (which is seeded as 0) with the nominated one (a 1).
		MTLDepthStencilDescriptor *depthStencilDescriptor = [[MTLDepthStencilDescriptor alloc] init];
		depthStencilDescriptor.frontFaceStencil.stencilCompareFunction = MTLCompareFunctionAlways;
		depthStencilDescriptor.frontFaceStencil.depthStencilPassOperation = MTLStencilOperationReplace;
		_drawStencilState = [view.device newDepthStencilStateWithDescriptor:depthStencilDescriptor];

		// TODO: old framebuffer should be resized onto the new one.
	}
}

- (void)setAspectRatio {
	const auto modals = _scanTarget.modals();
	const auto viewAspectRatio = (_view.bounds.size.width / _view.bounds.size.height);

	// Set the aspect ratio multiplier.
	uniforms()->aspectRatioMultiplier = float(modals.aspect_ratio / viewAspectRatio);

	// Also work out the proper zoom.
	const double fitWidthZoom = (viewAspectRatio / modals.aspect_ratio) / modals.visible_area.size.width;
	const double fitHeightZoom = 1.0 / modals.visible_area.size.height;
	uniforms()->zoom = float(std::min(fitWidthZoom, fitHeightZoom));

	// Store the offset.
	uniforms()->offset.x = -modals.visible_area.origin.x;
	uniforms()->offset.y = -modals.visible_area.origin.y;
}

- (void)setModals:(const Outputs::Display::ScanTarget::Modals &)modals {
	//
	// Populate uniforms.
	//
	uniforms()->scale[0] = modals.output_scale.x;
	uniforms()->scale[1] = modals.output_scale.y;
	uniforms()->lineWidth = 1.05f / modals.expected_vertical_lines;	// TODO: return to 1.0 (or slightly more), once happy.
	[self setAspectRatio];

	const auto toRGB = to_rgb_matrix(modals.composite_colour_space);
	uniforms()->toRGB = simd::float3x3(
		simd::float3{toRGB[0], toRGB[1], toRGB[2]},
		simd::float3{toRGB[3], toRGB[4], toRGB[5]},
		simd::float3{toRGB[6], toRGB[7], toRGB[8]}
	);

	const auto fromRGB = from_rgb_matrix(modals.composite_colour_space);
	uniforms()->fromRGB = simd::float3x3(
		simd::float3{fromRGB[0], fromRGB[1], fromRGB[2]},
		simd::float3{fromRGB[3], fromRGB[4], fromRGB[5]},
		simd::float3{fromRGB[6], fromRGB[7], fromRGB[8]}
	);



	//
	// Generate input texture.
	//
	MTLPixelFormat pixelFormat;
	_bytesPerInputPixel = size_for_data_type(modals.input_data_type);
	if(data_type_is_normalised(modals.input_data_type)) {
		switch(_bytesPerInputPixel) {
			default:
			case 1: pixelFormat = MTLPixelFormatR8Unorm;	break;
			case 2: pixelFormat = MTLPixelFormatRG8Unorm;	break;
			case 4: pixelFormat = MTLPixelFormatRGBA8Unorm;	break;
		}
	} else {
		switch(_bytesPerInputPixel) {
			default:
			case 1: pixelFormat = MTLPixelFormatR8Uint;		break;
			case 2: pixelFormat = MTLPixelFormatRG8Uint;	break;
			case 4: pixelFormat = MTLPixelFormatRGBA8Uint;	break;
		}
	}
	MTLTextureDescriptor *const textureDescriptor = [MTLTextureDescriptor
		texture2DDescriptorWithPixelFormat:pixelFormat
		width:BufferingScanTarget::WriteAreaWidth
		height:BufferingScanTarget::WriteAreaHeight
		mipmapped:NO];
	textureDescriptor.resourceOptions = SharedResourceOptionsTexture;
	if(@available(macOS 10.14, *)) {
		textureDescriptor.allowGPUOptimizedContents = NO;
	}

	// TODO: the call below is the only reason why this project now requires macOS 10.13; is it all that helpful versus just uploading each frame?
	const NSUInteger bytesPerRow = BufferingScanTarget::WriteAreaWidth * _bytesPerInputPixel;
	_writeAreaTexture = [_writeAreaBuffer
		newTextureWithDescriptor:textureDescriptor
		offset:0
		bytesPerRow:bytesPerRow];
	_totalTextureBytes = bytesPerRow * BufferingScanTarget::WriteAreaHeight;



	//
	// Generate scan pipeline.
	//
	id<MTLLibrary> library = [_view.device newDefaultLibrary];
	MTLRenderPipelineDescriptor *pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];

	// Occasions when the composition buffer isn't required are slender:
	//
	//	(i) input and output are both RGB; or
	//	(i) output is composite monochrome.
	_isUsingCompositionPipeline =
		(
			(natural_display_type_for_data_type(modals.input_data_type) != Outputs::Display::DisplayType::RGB) ||
			(modals.display_type != Outputs::Display::DisplayType::RGB)
		) && modals.display_type != Outputs::Display::DisplayType::CompositeMonochrome;

	struct FragmentSamplerDictionary {
		/// Fragment shader that outputs to the composition buffer for composite processing.
		NSString *const compositionComposite;
		/// Fragment shader that outputs to the composition buffer for S-Video processing.
		NSString *const compositionSVideo;

		/// Fragment shader that outputs directly as monochrome composite.
		NSString *const directComposite;
		/// Fragment shader that outputs directly as RGB.
		NSString *const directRGB;
	};

	// TODO: create fragment shaders to apply composite multiplication.
	// TODO: incorporate gamma correction into all direct outputters.
	const FragmentSamplerDictionary samplerDictionary[8] = {
		// Luminance1
		{@"sampleLuminance1", nullptr, @"sampleLuminance1", nullptr},
		{@"sampleLuminance8", nullptr, @"sampleLuminance8", nullptr},
		{@"samplePhaseLinkedLuminance8", nullptr, @"samplePhaseLinkedLuminance8", nullptr},
		{@"compositeSampleLuminance8Phase8", @"sampleLuminance8Phase8", @"compositeSampleLuminance8Phase8", nullptr},
		{@"compositeSampleRed1Green1Blue1", @"svideoSampleRed1Green1Blue1", @"compositeSampleRed1Green1Blue1", @"sampleRed1Green1Blue1"},
		{@"compositeSampleRed2Green2Blue2", @"svideoSampleRed2Green2Blue2", @"compositeSampleRed2Green2Blue2", @"sampleRed2Green2Blue2"},
		{@"compositeSampleRed4Green4Blue4", @"svideoSampleRed4Green4Blue4", @"compositeSampleRed4Green4Blue4", @"sampleRed4Green4Blue4"},
		{@"compositeSampleRed8Green8Blue8", @"svideoSampleRed8Green8Blue8", @"compositeSampleRed8Green8Blue8", @"sampleRed8Green8Blue8"},
	};

#ifndef NDEBUG
	// Do a quick check of the names entered above. I don't think this is possible at compile time.
	for(int c = 0; c < 8; ++c) {
		if(samplerDictionary[c].compositionComposite)	assert([library newFunctionWithName:samplerDictionary[c].compositionComposite]);
		if(samplerDictionary[c].compositionSVideo)		assert([library newFunctionWithName:samplerDictionary[c].compositionSVideo]);
		if(samplerDictionary[c].directComposite)		assert([library newFunctionWithName:samplerDictionary[c].directComposite]);
		if(samplerDictionary[c].directRGB)				assert([library newFunctionWithName:samplerDictionary[c].directRGB]);
	}
#endif

	// Build the composition pipeline if one is in use.
	const bool isSVideoOutput = modals.display_type == Outputs::Display::DisplayType::SVideo;
	if(_isUsingCompositionPipeline) {
		pipelineDescriptor.colorAttachments[0].pixelFormat = _compositionTexture.pixelFormat;
		pipelineDescriptor.vertexFunction = [library newFunctionWithName:@"scanToComposition"];
		pipelineDescriptor.fragmentFunction =
			[library newFunctionWithName:isSVideoOutput ? samplerDictionary[int(modals.input_data_type)].compositionSVideo : samplerDictionary[int(modals.input_data_type)].compositionComposite];

		_composePipeline = [_view.device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:nil];

		_compositionRenderPass = [[MTLRenderPassDescriptor alloc] init];
		_compositionRenderPass.colorAttachments[0].texture = _compositionTexture;
		_compositionRenderPass.colorAttachments[0].loadAction = MTLLoadActionClear;
		_compositionRenderPass.colorAttachments[0].storeAction = MTLStoreActionStore;

		// TODO: set proper clear colour for S-Video (and fragment function, below).

		simd::float3 *const firCoefficients = uniforms()->firCoefficients;
		const float cyclesPerLine = float(modals.cycles_per_line);
		const float colourCyclesPerLine = float(modals.colour_cycle_numerator) / float(modals.colour_cycle_denominator);

		if(isSVideoOutput) {
			// In S-Video, don't filter luminance.
			for(size_t c = 0; c < 7; ++c) {
				firCoefficients[c].x = 0.0f;
			}
			firCoefficients[7].x = 1.0f;
		} else {
			// In composite, filter luminance gently.
			SignalProcessing::FIRFilter luminancefilter(15, cyclesPerLine, 0.0f, colourCyclesPerLine * 0.75f);
			const auto calculatedCoefficients = luminancefilter.get_coefficients();
			for(size_t c = 0; c < 8; ++c) {
				firCoefficients[c].x = calculatedCoefficients[c];
			}
		}

		// Whether S-Video or composite, apply the same relatively strong filter to colour channels.
		SignalProcessing::FIRFilter chrominancefilter(15, cyclesPerLine, 0.0f, colourCyclesPerLine * 0.125f);
		const auto calculatedCoefficients = chrominancefilter.get_coefficients();
		for(size_t c = 0; c < 8; ++c) {
			firCoefficients[c].y = firCoefficients[c].z = calculatedCoefficients[c];
		}
	}

	// Build the output pipeline.
	pipelineDescriptor.colorAttachments[0].pixelFormat = _view.colorPixelFormat;
	pipelineDescriptor.vertexFunction = [library newFunctionWithName:_isUsingCompositionPipeline ? @"lineToDisplay" : @"scanToDisplay"];

	if(_isUsingCompositionPipeline) {
		pipelineDescriptor.fragmentFunction = [library newFunctionWithName:@"filterFragment"];
	} else {
		const bool isRGBOutput = modals.display_type == Outputs::Display::DisplayType::RGB;
		pipelineDescriptor.fragmentFunction =
			[library newFunctionWithName:isRGBOutput ? samplerDictionary[int(modals.input_data_type)].directRGB : samplerDictionary[int(modals.input_data_type)].directComposite];
	}

	// Enable blending.
	pipelineDescriptor.colorAttachments[0].blendingEnabled = YES;
	pipelineDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
	pipelineDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

	// Set stencil format.
	pipelineDescriptor.stencilAttachmentPixelFormat = MTLPixelFormatStencil8;

	// Finish.
	_outputPipeline = [_view.device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:nil];
}

- (void)outputFrom:(size_t)start to:(size_t)end commandBuffer:(id<MTLCommandBuffer>)commandBuffer {
	// Generate a command encoder for the view.
	id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:_frameBufferRenderPass];

	// Final output. Could be scans or lines.
	[encoder setRenderPipelineState:_outputPipeline];

	if(_isUsingCompositionPipeline) {
		[encoder setFragmentTexture:_compositionTexture atIndex:0];
		[encoder setVertexBuffer:_linesBuffer offset:0 atIndex:0];
	} else {
		[encoder setFragmentTexture:_writeAreaTexture atIndex:0];
		[encoder setVertexBuffer:_scansBuffer offset:0 atIndex:0];
	}
	[encoder setVertexBuffer:_uniformsBuffer offset:0 atIndex:1];
	[encoder setFragmentBuffer:_uniformsBuffer offset:0 atIndex:0];

	[encoder setDepthStencilState:_drawStencilState];
	[encoder setStencilReferenceValue:1];
#ifndef NDEBUG
	// Quick aid for debugging: the stencil test is predicated on front-facing pixels, so make sure they're
	// being generated.
	[encoder setCullMode:MTLCullModeBack];
#endif

#define OutputStrips(start, size)	[encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4 instanceCount:size baseInstance:start]
	RangePerform(start, end, (_isUsingCompositionPipeline ? NumBufferedLines : NumBufferedScans), OutputStrips);
#undef OutputStrips

	// Complete encoding.
	[encoder endEncoding];
}

- (void)outputFrameCleanerToCommandBuffer:(id<MTLCommandBuffer>)commandBuffer {
	// Generate a command encoder for the view.
	id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:_frameBufferRenderPass];

	// Drawing. Just scans.
	[encoder setRenderPipelineState:_clearPipeline];
	[encoder setDepthStencilState:_clearStencilState];
	[encoder setStencilReferenceValue:0];

	[encoder setVertexTexture:_frameBuffer atIndex:0];
	[encoder setFragmentTexture:_frameBuffer atIndex:0];

	[encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
	[encoder endEncoding];
}

- (void)updateFrameBuffer {
	// TODO: rethink BufferingScanTarget::perform. Is it now really just for guarding the modals?
	_scanTarget.perform([=] {
		const Outputs::Display::ScanTarget::Modals *const newModals = _scanTarget.new_modals();
		if(newModals) {
			[self setModals:*newModals];
		}
	});

	@synchronized(self) {
		if(!_frameBufferRenderPass) return;

		const auto outputArea = _scanTarget.get_output_area();

		// Ensure texture changes are noted.
		const auto writeAreaModificationStart = size_t(outputArea.start.write_area_x + outputArea.start.write_area_y * 2048) * _bytesPerInputPixel;
		const auto writeAreaModificationEnd = size_t(outputArea.end.write_area_x + outputArea.end.write_area_y * 2048) * _bytesPerInputPixel;
#define FlushRegion(start, size)	[_writeAreaBuffer didModifyRange:NSMakeRange(start, size)]
		RangePerform(writeAreaModificationStart, writeAreaModificationEnd, _totalTextureBytes, FlushRegion);
#undef FlushRegion

		// Obtain a source for render command encoders.
		id<MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];

		//
		// Drawing algorithm used below, in broad terms:
		//
		// Maintain a persistent buffer of current CRT state.
		//
		// During each frame, paint to the persistent buffer anything new. Update a stencil buffer to track
		// every pixel so-far touched.
		//
		// At the end of the frame, draw a 'frame cleaner', which is a whole-screen rect that paints over
		// only those areas that the stencil buffer indicates weren't painted this frame.
		//
		// Hence every pixel is touched every frame, regardless of the machine's output.
		//

		if(_isUsingCompositionPipeline) {
			// Output all scans to the composition buffer.
			id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:_compositionRenderPass];
			[encoder setRenderPipelineState:_composePipeline];

			[encoder setVertexBuffer:_scansBuffer offset:0 atIndex:0];
			[encoder setVertexBuffer:_uniformsBuffer offset:0 atIndex:1];
			[encoder setVertexTexture:_compositionTexture atIndex:0];

			[encoder setFragmentBuffer:_uniformsBuffer offset:0 atIndex:0];
			[encoder setFragmentTexture:_writeAreaTexture atIndex:0];

#define OutputScans(start, size)	[encoder drawPrimitives:MTLPrimitiveTypeLine vertexStart:0 vertexCount:2 instanceCount:size baseInstance:start]
			RangePerform(outputArea.start.scan, outputArea.end.scan, NumBufferedScans, OutputScans);
#undef OutputScans
			[encoder endEncoding];

			// Output lines, broken up by frame.
			size_t startLine = outputArea.start.line;
			size_t line = outputArea.start.line;
			while(line != outputArea.end.line) {
				if(_lineMetadataBuffer[line].is_first_in_frame && _lineMetadataBuffer[line].previous_frame_was_complete) {
					[self outputFrom:startLine to:line commandBuffer:commandBuffer];
					[self outputFrameCleanerToCommandBuffer:commandBuffer];
					startLine = line;
				}
				line = (line + 1) % NumBufferedLines;
			}
			[self outputFrom:startLine to:outputArea.end.line commandBuffer:commandBuffer];
		} else {
			// Output scans directly, broken up by frame.
			size_t line = outputArea.start.line;
			size_t scan = outputArea.start.scan;
			while(line != outputArea.end.line) {
				if(_lineMetadataBuffer[line].is_first_in_frame && _lineMetadataBuffer[line].previous_frame_was_complete) {
					[self outputFrom:scan to:_lineMetadataBuffer[line].first_scan commandBuffer:commandBuffer];
					[self outputFrameCleanerToCommandBuffer:commandBuffer];
					scan = _lineMetadataBuffer[line].first_scan;
				}
				line = (line + 1) % NumBufferedLines;
			}
			[self outputFrom:scan to:outputArea.end.scan commandBuffer:commandBuffer];
		}

		// Add a callback to update the scan target buffer and commit the drawing.
		[commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> _Nonnull) {
			self->_scanTarget.complete_output_area(outputArea);
		}];
		[commandBuffer commit];
	}
}

/*!
 @method drawInMTKView:
 @abstract Called on the delegate when it is asked to render into the view
 @discussion Called on the delegate when it is asked to render into the view
 */
- (void)drawInMTKView:(nonnull MTKView *)view {
	if(_isDrawing.test_and_set()) {
		return;
	}

	// Schedule a copy from the current framebuffer to the view; blitting is unavailable as the target is a framebuffer texture.
	id<MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];

	// Every pixel will be drawn, so don't clear or reload.
	view.currentRenderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionDontCare;
	id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:view.currentRenderPassDescriptor];

	[encoder setRenderPipelineState:_copyPipeline];
	[encoder setVertexTexture:_frameBuffer atIndex:0];
	[encoder setFragmentTexture:_frameBuffer atIndex:0];

	[encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
	[encoder endEncoding];

	[commandBuffer presentDrawable:view.currentDrawable];
	[commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> _Nonnull) {
		self->_isDrawing.clear();
	}];
	[commandBuffer commit];
}

-  (Outputs::Display::ScanTarget *)scanTarget {
	return &_scanTarget;
}

@end
