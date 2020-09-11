//
//  ScanTarget.m
//  Clock Signal
//
//  Created by Thomas Harte on 02/08/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#import "CSScanTarget.h"

#import <Metal/Metal.h>

#include <algorithm>
#include <atomic>
#include <cmath>

#include "BufferingScanTarget.hpp"
#include "FIRFilter.hpp"

/*

	RGB and composite monochrome
	----------------------------

	Source data is converted to 32bpp RGB or to composite directly from its input, at output resolution.
	Gamma correction is applied unless the inputs are 1bpp (e.g. Macintosh-style black/white, TTL-style RGB).

	S-Video
	-------

	Source data is pasted together with a common clock in the composition buffer. Colour phase is baked in
	at this point. Format within the composition buffer is:

		.r = luminance
		.g = 0.5 + 0.5 * chrominance * cos(phase)
		.b = 0.5 + 0.5 * chrominance * sin(phase)

	Contents of the composition buffer are then drawn into the finalised line texture; at this point a suitable
	low-filter is applied to the two chrominance channels, colours are converted to RGB and gamma corrected.

	Contents from the finalised line texture are then painted to the display.

	Composite colour
	----------------

	Source data is pasted together with a common clock in the composition buffer. Colour phase and amplitude are
	recorded at this point. Format within the composition buffer is:

		.r = composite value
		.g = 0.5 + 0.5 * cos(phase)
		.b = 0.5 + 0.5 * sin(phase)
		.a = amplitude

	[aside: upfront calculation of cos/sin is just because it'll need to be calculated at this precision anyway,
	and doing it here avoids having to do unit<->radian conversions on phase alone]

	Contents of the composition buffer are transferred to the separated-luma buffer, subject to a low-paass filter
	that has sought to separate luminance and chrominance, and with phase and amplitude now baked into the latter:

		.r = luminance
		.g = 0.5 + 0.5 * chrominance * cos(phase)
		.b = 0.5 + 0.5 * chrominance * sin(phase)

	The process now continues as per the corresponding S-Video steps.

	NOTES
	-----

		1)	for many of the input pixel formats it would be possible to do the trigonometric side of things at
			arbitrary precision. Since it would always be necessary to support fixed-precision processing because
			of the directly-sampled input formats, I've used fixed throughout to reduce the number of permutations
			and combinations of code I need to support. The precision is always selected to be at least four times
			the colour clock.

		2)	I experimented with skipping the separated-luma buffer for composite colour based on the observation that
			just multiplying the raw signal by sin and cos and then filtering well below the colour subcarrier frequency
			should be sufficient. It wasn't in practice because the bits of luminance that don't quite separate are then
			of such massive amplitude that you get huge bands of bright colour in place of the usual chroma dots.

		3)	I also initially didn't want to have a finalied-line texture, but processing costs changed my mind on that.
			If you accept that output will be fixed precision, anyway. In that case, processing for a typical NTSC frame
			in its original resolution means applying filtering (i.e. at least 15 samples per pixel) likely between
			218,400 and 273,000 times per output frame, then upscaling from there at 1 sample per pixel. Count the second
			sample twice for the original store and you're talking between 16*218,400 = 3,494,400 to 16*273,000 = 4,368,000
			total pixel accesses. Though that's not a perfect way to measure cost, roll with it.

			On my 4k monitor, doing it at actual output resolution would instead cost 3840*2160*15 = 124,416,000 total
			accesses. Which doesn't necessarily mean "more than 28 times as much", but does mean "a lot more".

			(going direct-to-display for composite monochrome means evaluating sin/cos a lot more often than it might
			with more buffering in between, but that doesn't provisionally seem to be as much of a bottleneck)
*/

namespace {

/// Provides a container for __fp16 versions of tightly-packed single-precision plain old data with a copy assignment constructor.
template <typename NaturalType> struct HalfConverter {
	__fp16 elements[sizeof(NaturalType) / sizeof(float)];

	void operator =(const NaturalType &rhs) {
		const float *floatRHS = reinterpret_cast<const float *>(&rhs);
		for(size_t c = 0; c < sizeof(elements) / sizeof(*elements); ++c) {
			elements[c] = __fp16(floatRHS[c]);
		}
	}
};

// Tracks the Uniforms struct declared in ScanTarget.metal; see there for field definitions.
//
// __fp16 is a Clang-specific type which I'm using as equivalent to a Metal half, i.e. an IEEE 754 binary16.
struct Uniforms {
	int32_t scale[2];
	float cyclesMultiplier;
	float lineWidth;

	simd::float3x3 sourcetoDisplay;

	HalfConverter<simd::float3x3> toRGB;
	HalfConverter<simd::float3x3> fromRGB;

	HalfConverter<simd::float3> chromaKernel[8];
	__fp16 lumaKernel[8];

	__fp16 outputAlpha;
	__fp16 outputGamma;
	__fp16 outputMultiplier;
};

constexpr size_t NumBufferedLines = 500;
constexpr size_t NumBufferedScans = NumBufferedLines * 4;

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

/// @returns the proper 1d kernel to apply a box filter around a certain point a pixel density of @c radiansPerPixel and applying an
///		angular limit of @c cutoff. The values returned will be the first eight of a fifteen-point filter that is symmetrical around its centre.
std::array<float, 8> boxCoefficients(float radiansPerPixel, float cutoff) {
	std::array<float, 8> filter;
	float total = 0.0f;

	for(size_t c = 0; c < 8; ++c) {
		// This coefficient occupies the angular window [6.5-c, 7.5-c]*radiansPerPixel.
		const float startAngle = (6.5f - float(c)) * radiansPerPixel;
		const float endAngle = (7.5f - float(c)) * radiansPerPixel;

		float coefficient = 0.0f;
		if(endAngle < cutoff) {
			coefficient = 1.0f;
		} else if(startAngle >= cutoff) {
			coefficient = 0.0f;
		} else {
			coefficient = (cutoff - startAngle) / radiansPerPixel;
		}
		total += 2.0f * coefficient;	// All but the centre coefficient will be used twice.
		filter[c] = coefficient;
	}
	total = total - filter[7];			// As per above; ensure the centre coefficient is counted only once.

	for(size_t c = 0; c < 8; ++c) {
		filter[c] /= total;
	}

	return filter;
}

}

using BufferingScanTarget = Outputs::Display::BufferingScanTarget;

@implementation CSScanTarget {
	// The command queue for the device in use.
	id<MTLCommandQueue> _commandQueue;

	// Pipelines.
	id<MTLRenderPipelineState> _composePipeline;	// For rendering to the composition texture.
	id<MTLRenderPipelineState> _outputPipeline;		// For drawing to the frame buffer.
	id<MTLRenderPipelineState> _copyPipeline;		// For copying the frame buffer to the visible surface.
	id<MTLRenderPipelineState> _clearPipeline;		// For applying additional inter-frame clearing (cf. the stencil).

	// Buffers.
	id<MTLBuffer> _uniformsBuffer;	// A static buffer, containing a copy of the Uniforms struct.
	id<MTLBuffer> _scansBuffer;		// A dynamic buffer, into which the CPU writes Scans for later display.
	id<MTLBuffer> _linesBuffer;		// A dynamic buffer, into which the CPU writes Lines for later display.

	// Textures: the write area.
	//
	// The write area receives fragments of output from the emulated machine.
	// So it is written by the CPU and read by the GPU.
	id<MTLTexture> _writeAreaTexture;
	id<MTLBuffer> _writeAreaBuffer;		// The storage underlying the write-area texture.
	size_t _bytesPerInputPixel;			// Determines per-pixel sizing within the write-area texture.
	size_t _totalTextureBytes;			// Holds the total size of the write-area texture.

	// Textures: the frame buffer.
	//
	// When inter-frame blending is in use, the frame buffer contains the most recent output.
	// Metal isn't really set up for single-buffered output, so this acts as if it were that
	// single buffer. This texture is complete 2d data, copied directly to the display.
	id<MTLTexture> _frameBuffer;
	MTLRenderPassDescriptor *_frameBufferRenderPass;	// The render pass for _drawing to_ the frame buffer.
	BOOL _dontClearFrameBuffer;

	// Textures: the stencil.
	//
	// Scan targets recceive scans, not full frames. Those scans may not cover the entire display,
	// either because unlit areas have been omitted or because a sync discrepancy means that the full
	// potential vertical or horizontal width of the display isn't used momentarily.
	//
	// In order to manage inter-frame blending correctly in those cases, a stencil is attached to the
	// frame buffer so that a clearing step can darken any pixels that weren't naturally painted during
	// any frame.
	id<MTLTexture> _frameBufferStencil;
	id<MTLDepthStencilState> _drawStencilState;		// Always draws, sets stencil to 1.
	id<MTLDepthStencilState> _clearStencilState;	// Draws only where stencil is 0, clears all to 0.

	// Textures: the composition texture.
	//
	// If additional temporal processing is required (i.e. for S-Video and colour composite output),
	// fragments from the write-area texture are assembled into the composition texture, where they
	// properly adjoin their neighbours and everything is converted to a common clock.
	id<MTLTexture> _compositionTexture;
	MTLRenderPassDescriptor *_compositionRenderPass;	// The render pass for _drawing to_ the composition buffer.

	enum class Pipeline {
		/// Scans are painted directly to the frame buffer.
		DirectToDisplay,
		/// Scans are painted to the composition buffer, which is processed to the finalised line buffer,
		/// from which lines are painted to the frame buffer.
		SVideo,
		/// Scans are painted to the composition buffer, which is processed to the separated luma buffer and then the finalised line buffer,
		/// from which lines are painted to the frame buffer.
		CompositeColour

		// TODO: decide what to do for downard-scaled direct-to-display. Obvious options are to include lowpass
		// filtering into the scan outputter and contine hoping that the vertical takes care of itself, or maybe
		// to stick with DirectToDisplay but with a minimum size for the frame buffer and apply filtering from
		// there to the screen.
	};
	Pipeline _pipeline;

	// Textures: additional storage used when processing S-Video and composite colour input.
	id<MTLTexture> _finalisedLineTexture;
	id<MTLComputePipelineState> _finalisedLineState;
	id<MTLTexture> _separatedLumaTexture;
	id<MTLComputePipelineState> _separatedLumaState;
	NSUInteger _lineBufferPixelsPerLine;

	size_t _lineOffsetBuffer;
	id<MTLBuffer> _lineOffsetBuffers[NumBufferedLines];	// Allocating NumBufferedLines buffers ensures these can't possibly be exhausted;
														// for this list to be exhausted there'd have to be more draw calls in flight than
														// there are lines for them to operate upon.

	// The scan target in C++-world terms and the non-GPU storage for it.
	BufferingScanTarget _scanTarget;
	BufferingScanTarget::LineMetadata _lineMetadataBuffer[NumBufferedLines];
	std::atomic_flag _isDrawing;

	// Additional pipeline information.
	size_t _lumaKernelSize;
	size_t _chromaKernelSize;

	// The output view.
	__weak MTKView *_view;
}

- (nonnull instancetype)initWithView:(nonnull MTKView *)view {
	self = [super init];
	if(self) {
		_view = view;
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

		// Allocate a large number of single-int buffers, for supplying offsets to the compute shaders.
		// There's a ridiculous amount of overhead in this, but it avoids allocations during drawing,
		// and a single int per instance is all I need.
		for(size_t c = 0; c < NumBufferedLines; ++c) {
			_lineOffsetBuffers[c] = [_view.device newBufferWithLength:sizeof(int) options:SharedResourceOptionsStandard];
		}

		// Ensure the is-drawing flag is initially clear.
		_isDrawing.clear();

		// Set initial aspect-ratio multiplier and generate buffers.
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
	[self setAspectRatio];

	@synchronized(self) {
		[self updateSizeBuffersToSize:size];
	}
}

- (void)updateSizeBuffersToSize:(CGSize)size {
	const NSUInteger frameBufferWidth = NSUInteger(size.width * _view.layer.contentsScale);
	const NSUInteger frameBufferHeight = NSUInteger(size.height * _view.layer.contentsScale);

	// Generate a framebuffer and a stencil.
	MTLTextureDescriptor *const textureDescriptor = [MTLTextureDescriptor
		texture2DDescriptorWithPixelFormat:_view.colorPixelFormat
		width:frameBufferWidth
		height:frameBufferHeight
		mipmapped:NO];
	textureDescriptor.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
	textureDescriptor.resourceOptions = MTLResourceStorageModePrivate;
	id<MTLTexture> _oldFrameBuffer = _frameBuffer;
	_frameBuffer = [_view.device newTextureWithDescriptor:textureDescriptor];

	MTLTextureDescriptor *const stencilTextureDescriptor = [MTLTextureDescriptor
		texture2DDescriptorWithPixelFormat:MTLPixelFormatStencil8
		width:frameBufferWidth
		height:frameBufferHeight
		mipmapped:NO];
	stencilTextureDescriptor.usage = MTLTextureUsageRenderTarget;
	stencilTextureDescriptor.resourceOptions = MTLResourceStorageModePrivate;
	_frameBufferStencil = [_view.device newTextureWithDescriptor:stencilTextureDescriptor];

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
	_drawStencilState = [_view.device newDepthStencilStateWithDescriptor:depthStencilDescriptor];

	// Draw from _oldFrameBuffer to _frameBuffer.
	if(_oldFrameBuffer) {
		MTLRenderPassDescriptor *const resizeFrameBufferDescriptor = [[MTLRenderPassDescriptor alloc] init];
		resizeFrameBufferDescriptor.colorAttachments[0].texture = _frameBuffer;
		resizeFrameBufferDescriptor.colorAttachments[0].loadAction = MTLLoadActionDontCare;
		resizeFrameBufferDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;

		id<MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];
		id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:resizeFrameBufferDescriptor];

		[encoder setRenderPipelineState:_copyPipeline];
		[encoder setVertexTexture:_oldFrameBuffer atIndex:0];
		[encoder setFragmentTexture:_oldFrameBuffer atIndex:0];

		[encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
		[encoder endEncoding];
		[commandBuffer commit];

		// Don't clear the framebuffer at the end of this frame.
		_dontClearFrameBuffer = YES;
	}
}

- (BOOL)shouldApplyGamma {
	return fabsf(float(uniforms()->outputGamma) - 1.0f) > 0.01f;
}

- (void)updateModalBuffers {
	// Build a descriptor for any intermediate line texture.
	MTLTextureDescriptor *const lineTextureDescriptor = [MTLTextureDescriptor
		texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
		width:2048		// This 'should do'.
		height:NumBufferedLines
		mipmapped:NO];
	lineTextureDescriptor.resourceOptions = MTLResourceStorageModePrivate;

	if(_pipeline == Pipeline::DirectToDisplay) {
		// Buffers are not required when outputting direct to display; so if this isn't that then release anything
		// currently being held and return.
		_finalisedLineTexture = nil;
		_finalisedLineState = nil;
		_separatedLumaTexture = nil;
		_separatedLumaState = nil;
		_compositionTexture = nil;
		_compositionRenderPass = nil;
		return;
	}

	// Create a composition texture if one does not yet exist.
	if(!_compositionTexture) {
		lineTextureDescriptor.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
		_compositionTexture = [_view.device newTextureWithDescriptor:lineTextureDescriptor];
	}

	// Grab the shader library.
	id<MTLLibrary> library = [_view.device newDefaultLibrary];
	lineTextureDescriptor.usage = MTLTextureUsageShaderWrite | MTLTextureUsageShaderRead;

	// The finalised texture will definitely exist, and may or may not require a gamma conversion when written to.
	if(!_finalisedLineTexture) {
		_finalisedLineTexture = [_view.device newTextureWithDescriptor:lineTextureDescriptor];

		NSString *const kernelFunction = [self shouldApplyGamma] ? @"filterChromaKernelWithGamma" : @"filterChromaKernelNoGamma";
		_finalisedLineState = [_view.device newComputePipelineStateWithFunction:[library newFunctionWithName:kernelFunction] error:nil];

		// Ensure finalised line texture is initially clear.
		id<MTLComputePipelineState> clearPipeline = [_view.device newComputePipelineStateWithFunction:[library newFunctionWithName:@"clearKernel"] error:nil];
		id<MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];
		id<MTLComputeCommandEncoder> computeEncoder = [commandBuffer computeCommandEncoder];

		[self dispatchComputeCommandEncoder:computeEncoder pipelineState:clearPipeline width:lineTextureDescriptor.width height:lineTextureDescriptor.height offsetBuffer:[self bufferForOffset:0]];

		[computeEncoder endEncoding];
		[commandBuffer commit];
	}

	// A luma separation texture will exist only for composite colour.
	if(_pipeline == Pipeline::CompositeColour) {
		if(!_separatedLumaTexture) {
			_separatedLumaTexture = [_view.device newTextureWithDescriptor:lineTextureDescriptor];

			NSString *kernelFunction;
			switch(_lumaKernelSize) {
				default:	kernelFunction = @"separateLumaKernel15";	break;
				case 9:		kernelFunction = @"separateLumaKernel9";	break;
				case 7:		kernelFunction = @"separateLumaKernel7";	break;
				case 1:
				case 3:
				case 5:		kernelFunction = @"separateLumaKernel5";	break;
			}

			_separatedLumaState = [_view.device newComputePipelineStateWithFunction:[library newFunctionWithName:kernelFunction] error:nil];
		}
	} else {
		_separatedLumaTexture = nil;
	}
}

- (void)setAspectRatio {
	const auto modals = _scanTarget.modals();
	const auto viewAspectRatio = (_view.bounds.size.width / _view.bounds.size.height);
	simd::float3x3 sourceToDisplay{1.0f};

	// The starting coordinate space is [0, 1].

	// Move the centre of the cropping rectangle to the centre of the display.
	{
		simd::float3x3 recentre{1.0f};
		recentre.columns[2][0] = 0.5f - (modals.visible_area.origin.x + modals.visible_area.size.width * 0.5f);
		recentre.columns[2][1] = 0.5f - (modals.visible_area.origin.y + modals.visible_area.size.height * 0.5f);
		sourceToDisplay = recentre * sourceToDisplay;
	}

	// Convert from the internal [0, 1] to centred [-1, 1] (i.e. Metal's eye coordinates, though also appropriate
	// for the zooming step that follows).
	{
		simd::float3x3 convertToEye;
		convertToEye.columns[0][0] = 2.0f;
		convertToEye.columns[1][1] = -2.0f;
		convertToEye.columns[2][0] = -1.0f;
		convertToEye.columns[2][1] = 1.0f;
		convertToEye.columns[2][2] = 1.0f;
		sourceToDisplay = convertToEye * sourceToDisplay;
	}

	// Determine the correct zoom level. This is a combination of (i) the necessary horizontal stretch to produce a proper
	// aspect ratio; and (ii) the necessary zoom from there to either fit the visible area width or height as per a decision
	// on letterboxing or pillarboxing.
	const float aspectRatioStretch = float(modals.aspect_ratio / viewAspectRatio);
	const float fitWidthZoom = 1.0f / (float(modals.visible_area.size.width) * aspectRatioStretch);
	const float fitHeightZoom = 1.0f / float(modals.visible_area.size.height);
	const float zoom = std::min(fitWidthZoom, fitHeightZoom);

	// Convert from there to the proper aspect ratio by stretching or compressing width.
	// After this the output is exactly centred, filling the vertical space and being as wide or slender as it likes.
	{
		simd::float3x3 applyAspectRatio{1.0f};
		applyAspectRatio.columns[0][0] = aspectRatioStretch * zoom;
		applyAspectRatio.columns[1][1] = zoom;
		sourceToDisplay = applyAspectRatio * sourceToDisplay;
	}

	// Store.
	uniforms()->sourcetoDisplay = sourceToDisplay;
}

- (void)setModals:(const Outputs::Display::ScanTarget::Modals &)modals {
	//
	// Populate uniforms.
	//
	uniforms()->scale[0] = modals.output_scale.x;
	uniforms()->scale[1] = modals.output_scale.y;
	uniforms()->lineWidth = 1.05f / modals.expected_vertical_lines;
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

	// This is fixed for now; consider making it a function of frame rate and/or of whether frame syncing
	// is ongoing (which would require a way to signal that to this scan target).
	uniforms()->outputAlpha = __fp16(0.64f);
	uniforms()->outputMultiplier = __fp16(modals.brightness);

	const float displayGamma = 2.2f;	// This is assumed.
	uniforms()->outputGamma = __fp16(displayGamma / modals.intended_gamma);



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

	// Occasions when the composition buffer isn't required are slender: the output must be neither RGB nor composite monochrome.
	const bool isComposition =
		modals.display_type != Outputs::Display::DisplayType::RGB &&
		modals.display_type != Outputs::Display::DisplayType::CompositeMonochrome;
	const bool isSVideoOutput = modals.display_type == Outputs::Display::DisplayType::SVideo;

	if(!isComposition) {
		_pipeline = Pipeline::DirectToDisplay;
	} else {
		_pipeline = isSVideoOutput ? Pipeline::SVideo : Pipeline::CompositeColour;
	}

	struct FragmentSamplerDictionary {
		/// Fragment shader that outputs to the composition buffer for composite processing.
		NSString *const compositionComposite;
		/// Fragment shader that outputs to the composition buffer for S-Video processing.
		NSString *const compositionSVideo;

		/// Fragment shader that outputs directly as monochrome composite.
		NSString *const directComposite;
		/// Fragment shader that outputs directly as monochrome composite, with gamma correction.
		NSString *const directCompositeWithGamma;
		/// Fragment shader that outputs directly as RGB.
		NSString *const directRGB;
		/// Fragment shader that outputs directly as RGB, with gamma correction.
		NSString *const directRGBWithGamma;
	};
	const FragmentSamplerDictionary samplerDictionary[8] = {
		// Composite formats.
		{@"compositeSampleLuminance1", 				nil,	@"sampleLuminance1",				@"sampleLuminance1",						@"sampleLuminance1",				@"sampleLuminance1"},
		{@"compositeSampleLuminance8", 				nil,	@"sampleLuminance8", 				@"sampleLuminance8WithGamma",				@"sampleLuminance8", 				@"sampleLuminance8WithGamma"},
		{@"compositeSamplePhaseLinkedLuminance8", 	nil,	@"samplePhaseLinkedLuminance8",		@"samplePhaseLinkedLuminance8WithGamma",	@"samplePhaseLinkedLuminance8",		@"samplePhaseLinkedLuminance8WithGamma"},

		// S-Video formats.
		{@"compositeSampleLuminance8Phase8", @"sampleLuminance8Phase8", @"directCompositeSampleLuminance8Phase8", @"directCompositeSampleLuminance8Phase8WithGamma", @"directCompositeSampleLuminance8Phase8", @"directCompositeSampleLuminance8Phase8WithGamma"},

		// RGB formats.
		{@"compositeSampleRed1Green1Blue1", @"svideoSampleRed1Green1Blue1", @"directCompositeSampleRed1Green1Blue1", @"directCompositeSampleRed1Green1Blue1WithGamma", @"sampleRed1Green1Blue1", @"sampleRed1Green1Blue1"},
		{@"compositeSampleRed2Green2Blue2", @"svideoSampleRed2Green2Blue2", @"directCompositeSampleRed2Green2Blue2", @"directCompositeSampleRed2Green2Blue2WithGamma", @"sampleRed2Green2Blue2", @"sampleRed2Green2Blue2WithGamma"},
		{@"compositeSampleRed4Green4Blue4", @"svideoSampleRed4Green4Blue4", @"directCompositeSampleRed4Green4Blue4", @"directCompositeSampleRed4Green4Blue4WithGamma", @"sampleRed4Green4Blue4", @"sampleRed4Green4Blue4WithGamma"},
		{@"compositeSampleRed8Green8Blue8", @"svideoSampleRed8Green8Blue8", @"directCompositeSampleRed8Green8Blue8", @"directCompositeSampleRed8Green8Blue8WithGamma", @"sampleRed8Green8Blue8", @"sampleRed8Green8Blue8WithGamma"},
	};

#ifndef NDEBUG
	// Do a quick check that all the shaders named above are defined in the Metal code. I don't think this is possible at compile time.
	for(int c = 0; c < 8; ++c) {
#define Test(x)	if(samplerDictionary[c].x)	assert([library newFunctionWithName:samplerDictionary[c].x]);
		Test(compositionComposite);
		Test(compositionSVideo);
		Test(directComposite);
		Test(directCompositeWithGamma);
		Test(directRGB);
		Test(directRGBWithGamma);
#undef Test
	}
#endif

	uniforms()->cyclesMultiplier = 1.0f;
	if(_pipeline != Pipeline::DirectToDisplay) {
		// Pick a suitable cycle multiplier.
		const float minimumSize = 4.0f * float(modals.colour_cycle_numerator) / float(modals.colour_cycle_denominator);
		while(uniforms()->cyclesMultiplier * modals.cycles_per_line < minimumSize) {
			uniforms()->cyclesMultiplier += 1.0f;

			if(uniforms()->cyclesMultiplier * modals.cycles_per_line > 2048) {
				uniforms()->cyclesMultiplier -= 1.0f;
				break;
			}
		}

		// Create suitable filters.
		_lineBufferPixelsPerLine = NSUInteger(modals.cycles_per_line) * NSUInteger(uniforms()->cyclesMultiplier);
		const float colourCyclesPerLine = float(modals.colour_cycle_numerator) / float(modals.colour_cycle_denominator);

		// Compute radians per pixel.
		const float radiansPerPixel = (colourCyclesPerLine * 3.141592654f * 2.0f) / float(_lineBufferPixelsPerLine);

		// Generate the chrominance filter.
		{
			simd::float3 firCoefficients[8];
			const auto chromaCoefficients = boxCoefficients(radiansPerPixel, 3.141592654f);
			_chromaKernelSize = 15;
			for(size_t c = 0; c < 8; ++c) {
				firCoefficients[c].y = firCoefficients[c].z = (isSVideoOutput ? 2.0f : 1.0f) * chromaCoefficients[c];
				firCoefficients[c].x = 0.0f;
				if(fabsf(chromaCoefficients[c]) < 0.01f) {
					_chromaKernelSize -= 2;
				}
			}
			firCoefficients[7].x = 1.0f;

			// Luminance will be very soft as a result of the separation phase; apply a sharpen filter to try to undo that.
			// This is applied separately because the first composite processing step is going to select between the nominal
			// chroma and luma parts to take the place of luminance depending on whether a colour burst was found, and high-pass
			// filtering the chrominance channel would be visually detrimental.
			//
			// The low cut off ['Hz' but per line, not per second] is somewhat arbitrary.
			if(!isSVideoOutput) {
				SignalProcessing::FIRFilter sharpenFilter(15, float(_lineBufferPixelsPerLine), 40.0f, colourCyclesPerLine);
				const auto sharpen = sharpenFilter.get_coefficients();
				size_t sharpenFilterSize = 15;
				bool isStart = true;
				for(size_t c = 0; c < 8; ++c) {
					firCoefficients[c].x = sharpen[c];
					if(fabsf(sharpen[c]) > 0.01f) isStart = false;
					if(isStart) sharpenFilterSize -= 2;
				}
				_chromaKernelSize = std::max(_chromaKernelSize, sharpenFilterSize);
			}

			// Convert to half-size floats.
			for(size_t c = 0; c < 8; ++c) {
				uniforms()->chromaKernel[c] = firCoefficients[c];
			}
		}

		// Generate the luminance separation filter and determine its required size.
		{
			auto *const filter = uniforms()->lumaKernel;
			const auto coefficients = boxCoefficients(radiansPerPixel, 3.141592654f);
			_lumaKernelSize = 15;
			for(size_t c = 0; c < 8; ++c) {
				filter[c] = __fp16(coefficients[c]);
				if(fabsf(coefficients[c]) < 0.01f) {
					_lumaKernelSize -= 2;
				}
			}
		}
	}

	// Update intermediate storage.
	[self updateModalBuffers];

	if(_pipeline != Pipeline::DirectToDisplay) {
		// Create the composition render pass.
 		pipelineDescriptor.colorAttachments[0].pixelFormat = _compositionTexture.pixelFormat;
		pipelineDescriptor.vertexFunction = [library newFunctionWithName:@"scanToComposition"];
		pipelineDescriptor.fragmentFunction =
			[library newFunctionWithName:isSVideoOutput ? samplerDictionary[int(modals.input_data_type)].compositionSVideo : samplerDictionary[int(modals.input_data_type)].compositionComposite];

		_composePipeline = [_view.device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:nil];

		_compositionRenderPass = [[MTLRenderPassDescriptor alloc] init];
		_compositionRenderPass.colorAttachments[0].texture = _compositionTexture;
		_compositionRenderPass.colorAttachments[0].loadAction = MTLLoadActionClear;
		_compositionRenderPass.colorAttachments[0].storeAction = MTLStoreActionStore;
		_compositionRenderPass.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.5, 0.5, 0.3);
	}

	// Build the output pipeline.
	pipelineDescriptor.colorAttachments[0].pixelFormat = _view.colorPixelFormat;
	pipelineDescriptor.vertexFunction = [library newFunctionWithName:_pipeline == Pipeline::DirectToDisplay ? @"scanToDisplay" : @"lineToDisplay"];

	if(_pipeline != Pipeline::DirectToDisplay) {
		pipelineDescriptor.fragmentFunction = [library newFunctionWithName:@"interpolateFragment"];
	} else {
		const bool isRGBOutput = modals.display_type == Outputs::Display::DisplayType::RGB;

		NSString *shaderName;
		if(isRGBOutput) {
			shaderName = [self shouldApplyGamma] ? samplerDictionary[int(modals.input_data_type)].directRGBWithGamma : samplerDictionary[int(modals.input_data_type)].directRGB;
		} else {
			shaderName = [self shouldApplyGamma] ? samplerDictionary[int(modals.input_data_type)].directCompositeWithGamma : samplerDictionary[int(modals.input_data_type)].directComposite;
		}
		pipelineDescriptor.fragmentFunction = [library newFunctionWithName:shaderName];
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
	if(start == end) return;

	// Generate a command encoder for the view.
	id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:_frameBufferRenderPass];

	// Final output. Could be scans or lines.
	[encoder setRenderPipelineState:_outputPipeline];

	if(_pipeline != Pipeline::DirectToDisplay) {
		[encoder setFragmentTexture:_finalisedLineTexture atIndex:0];
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
	RangePerform(start, end, (_pipeline != Pipeline::DirectToDisplay ? NumBufferedLines : NumBufferedScans), OutputStrips);
#undef OutputStrips

	// Complete encoding.
	[encoder endEncoding];
}

- (void)outputFrameCleanerToCommandBuffer:(id<MTLCommandBuffer>)commandBuffer {
	// Generate a command encoder for the view.
	id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:_frameBufferRenderPass];

	[encoder setRenderPipelineState:_clearPipeline];
	[encoder setDepthStencilState:_clearStencilState];
	[encoder setStencilReferenceValue:0];

	[encoder setVertexTexture:_frameBuffer atIndex:0];
	[encoder setFragmentTexture:_frameBuffer atIndex:0];
	[encoder setFragmentBuffer:_uniformsBuffer offset:0 atIndex:0];

	[encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
	[encoder endEncoding];
}

- (void)composeOutputArea:(const BufferingScanTarget::OutputArea &)outputArea commandBuffer:(id<MTLCommandBuffer>)commandBuffer {
	// Output all scans to the composition buffer.
	const id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:_compositionRenderPass];
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
}

- (id<MTLBuffer>)bufferForOffset:(size_t)offset {
	// Store and apply the offset.
	const auto buffer = _lineOffsetBuffers[_lineOffsetBuffer];
	*(reinterpret_cast<int *>(_lineOffsetBuffers[_lineOffsetBuffer].contents)) = int(offset);
	_lineOffsetBuffer = (_lineOffsetBuffer + 1) % NumBufferedLines;
	return buffer;
}

- (void)dispatchComputeCommandEncoder:(id<MTLComputeCommandEncoder>)encoder pipelineState:(id<MTLComputePipelineState>)pipelineState width:(NSUInteger)width height:(NSUInteger)height offsetBuffer:(id<MTLBuffer>)offsetBuffer {
	[encoder setBuffer:offsetBuffer offset:0 atIndex:1];

	// This follows the recommendations at https://developer.apple.com/documentation/metal/calculating_threadgroup_and_grid_sizes ;
	// I currently have no independent opinion whatsoever.
	const MTLSize threadsPerThreadgroup = MTLSizeMake(
		pipelineState.threadExecutionWidth,
		pipelineState.maxTotalThreadsPerThreadgroup / pipelineState.threadExecutionWidth,
		1
	);
	const MTLSize threadsPerGrid = MTLSizeMake(width, height, 1);

	// Set the pipeline state and dispatch the drawing. Which may slightly overdraw.
	[encoder setComputePipelineState:pipelineState];
	[encoder dispatchThreads:threadsPerGrid threadsPerThreadgroup:threadsPerThreadgroup];
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

		if(outputArea.end.line != outputArea.start.line) {

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

			switch(_pipeline) {
				case Pipeline::DirectToDisplay: {
					// Output scans directly, broken up by frame.
					size_t line = outputArea.start.line;
					size_t scan = outputArea.start.scan;
					while(line != outputArea.end.line) {
						if(_lineMetadataBuffer[line].is_first_in_frame) {
							[self outputFrom:scan to:_lineMetadataBuffer[line].first_scan commandBuffer:commandBuffer];
							scan = _lineMetadataBuffer[line].first_scan;

							if(_lineMetadataBuffer[line].previous_frame_was_complete && !_dontClearFrameBuffer) {
								[self outputFrameCleanerToCommandBuffer:commandBuffer];
							}
							_dontClearFrameBuffer = NO;
						}
						line = (line + 1) % NumBufferedLines;
					}
					[self outputFrom:scan to:outputArea.end.scan commandBuffer:commandBuffer];
				} break;

				case Pipeline::CompositeColour:
				case Pipeline::SVideo: {
					// Build the composition buffer.
					[self composeOutputArea:outputArea commandBuffer:commandBuffer];

					if(_pipeline == Pipeline::SVideo) {
						// Filter from composition to the finalised line texture.
						id<MTLComputeCommandEncoder> computeEncoder = [commandBuffer computeCommandEncoder];
						[computeEncoder setTexture:_compositionTexture atIndex:0];
						[computeEncoder setTexture:_finalisedLineTexture atIndex:1];
						[computeEncoder setBuffer:_uniformsBuffer offset:0 atIndex:0];

						if(outputArea.end.line > outputArea.start.line) {
							[self dispatchComputeCommandEncoder:computeEncoder pipelineState:_finalisedLineState width:_lineBufferPixelsPerLine height:outputArea.end.line - outputArea.start.line offsetBuffer:[self bufferForOffset:outputArea.start.line]];
						} else {
							[self dispatchComputeCommandEncoder:computeEncoder pipelineState:_finalisedLineState width:_lineBufferPixelsPerLine height:NumBufferedLines - outputArea.start.line offsetBuffer:[self bufferForOffset:outputArea.start.line]];
							if(outputArea.end.line) {
								[self dispatchComputeCommandEncoder:computeEncoder pipelineState:_finalisedLineState width:_lineBufferPixelsPerLine height:outputArea.end.line offsetBuffer:[self bufferForOffset:0]];
							}
						}

						[computeEncoder endEncoding];
					} else {
						// Separate luminance.
						id<MTLComputeCommandEncoder> computeEncoder = [commandBuffer computeCommandEncoder];
						[computeEncoder setTexture:_compositionTexture atIndex:0];
						[computeEncoder setTexture:_separatedLumaTexture atIndex:1];
						[computeEncoder setBuffer:_uniformsBuffer offset:0 atIndex:0];

						__unsafe_unretained id<MTLBuffer> offsetBuffers[2] = {nil, nil};
						offsetBuffers[0] = [self bufferForOffset:outputArea.start.line];

						if(outputArea.end.line > outputArea.start.line) {
							[self dispatchComputeCommandEncoder:computeEncoder pipelineState:_separatedLumaState width:_lineBufferPixelsPerLine height:outputArea.end.line - outputArea.start.line offsetBuffer:offsetBuffers[0]];
						} else {
							[self dispatchComputeCommandEncoder:computeEncoder pipelineState:_separatedLumaState width:_lineBufferPixelsPerLine height:NumBufferedLines - outputArea.start.line offsetBuffer:offsetBuffers[0]];
							if(outputArea.end.line) {
								offsetBuffers[1] = [self bufferForOffset:0];
								[self dispatchComputeCommandEncoder:computeEncoder pipelineState:_separatedLumaState width:_lineBufferPixelsPerLine height:outputArea.end.line offsetBuffer:offsetBuffers[1]];
							}
						}

						// Filter resulting chrominance.
						[computeEncoder setTexture:_separatedLumaTexture atIndex:0];
						[computeEncoder setTexture:_finalisedLineTexture atIndex:1];
						[computeEncoder setBuffer:_uniformsBuffer offset:0 atIndex:0];

						if(outputArea.end.line > outputArea.start.line) {
							[self dispatchComputeCommandEncoder:computeEncoder pipelineState:_finalisedLineState width:_lineBufferPixelsPerLine height:outputArea.end.line - outputArea.start.line offsetBuffer:offsetBuffers[0]];
						} else {
							[self dispatchComputeCommandEncoder:computeEncoder pipelineState:_finalisedLineState width:_lineBufferPixelsPerLine height:NumBufferedLines - outputArea.start.line offsetBuffer:offsetBuffers[0]];
							if(outputArea.end.line) {
								[self dispatchComputeCommandEncoder:computeEncoder pipelineState:_finalisedLineState width:_lineBufferPixelsPerLine height:outputArea.end.line offsetBuffer:offsetBuffers[1]];
							}
						}

						[computeEncoder endEncoding];
					}

					// Output lines, broken up by frame.
					size_t startLine = outputArea.start.line;
					size_t line = outputArea.start.line;
					while(line != outputArea.end.line) {
						if(_lineMetadataBuffer[line].is_first_in_frame) {
							[self outputFrom:startLine to:line commandBuffer:commandBuffer];
							startLine = line;

							if(_lineMetadataBuffer[line].previous_frame_was_complete && !_dontClearFrameBuffer) {
								[self outputFrameCleanerToCommandBuffer:commandBuffer];
							}
							_dontClearFrameBuffer = NO;
						}
						line = (line + 1) % NumBufferedLines;
					}
					[self outputFrom:startLine to:outputArea.end.line commandBuffer:commandBuffer];
				} break;
			}

			// Add a callback to update the scan target buffer and commit the drawing.
			[commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> _Nonnull) {
				self->_scanTarget.complete_output_area(outputArea);
			}];
			[commandBuffer commit];
		} else {
			// There was no work, but to be contractually correct, remember to announce completion,
			// and do it after finishing an empty command queue, as a cheap way to ensure this doen't
			// front run any actual processing. TODO: can I do a better job of that?
			id<MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];
			[commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> _Nonnull) {
				self->_scanTarget.complete_output_area(outputArea);
			}];
			[commandBuffer commit];

			// TODO: reenable these and work out how on earth the Master System + Alex Kidd (US) is managing
			// to provide write_area_y = 0, start_x = 0, end_x = 1.
//			assert(outputArea.end.line == outputArea.start.line);
//			assert(outputArea.end.scan == outputArea.start.scan);
//			assert(outputArea.end.write_area_y == outputArea.start.write_area_y);
//			assert(outputArea.end.write_area_x == outputArea.start.write_area_x);
		}
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
