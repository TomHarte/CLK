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
#include <concepts>
#include <optional>

#include "BufferingScanTarget.hpp"
#include "FilterGenerator.hpp"
#include "Numeric/CircularCounter.hpp"

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
	low-pass filter is applied to the two chrominance channels, colours are converted to RGB and gamma corrected.

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

	Contents of the composition buffer are transferred to the separated-luma buffer, subject to a low-pass filter
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
			218,400 and 273,000 times per output frame, then upscaling from there at 1 sample per pixel. Count the
			second sample twice for the original store and you're talking between 16*218,400 = 3,494,400 to
			16*273,000 = 4,368,000 total pixel accesses. Though that's not a perfect way to measure cost, roll with it.

			On my 4k monitor, doing it at actual output resolution would instead cost 3840*2160*15 = 124,416,000 total
			accesses. Which doesn't necessarily mean "more than 28 times as much", but does mean "a lot more".

			(going direct-to-display for composite monochrome means evaluating sin/cos a lot more often than it might
			with more buffering in between, but that doesn't provisionally seem to be as much of a bottleneck)
*/

namespace {
constexpr auto BufferWidth = Outputs::Display::FilterGenerator::SuggestedBufferWidth;
constexpr size_t NumBufferedLines = 500;
constexpr size_t NumBufferedScans = NumBufferedLines * 4;

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

	HalfConverter<simd::float3> chromaKernel[16];
	HalfConverter<simd::float2> lumaKernel[16];

	__fp16 outputAlpha;
	__fp16 outputGamma;
	__fp16 outputMultiplier;
};

// Kernel sizes above and in the shaders themselves assume a maximum filter kernel size.
static_assert(Outputs::Display::FilterGenerator::MaxKernelSize <= 31);

/// The shared resource options this app would most favour; applied as widely as possible.
constexpr MTLResourceOptions SharedResourceOptionsStandard =
	MTLResourceCPUCacheModeWriteCombined | MTLResourceStorageModeShared;

/// The shared resource options used for the write-area texture; on macOS it can't be MTLResourceStorageModeShared so this is a carve-out.
constexpr MTLResourceOptions SharedResourceOptionsTexture =
	MTLResourceCPUCacheModeWriteCombined | MTLResourceStorageModeManaged;

template <typename FuncT>
requires std::invocable<FuncT, size_t, size_t>
void range_perform(
	const size_t start,
	const size_t end,
	const size_t size,
	const FuncT &&func
) {
	if(start == end) return;
	if(start < end) {
		func(start, end - start);
		return;
	}

	func(start, size - start);
	if(end) func(0, end);
}

}

using BufferingScanTarget = Outputs::Display::BufferingScanTarget;

@implementation CSScanTarget {
	// The command queue for the device in use.
	id<MTLCommandQueue> _commandQueue;

	// Pipelines.
	id<MTLRenderPipelineState> _composePipeline;		// Renders to the composition texture.
	id<MTLRenderPipelineState> _outputPipeline;			// Draws to the frame buffer.
	id<MTLRenderPipelineState> _copyPipeline;			// Copies from one texture to another.
	id<MTLRenderPipelineState> _supersamplePipeline;	// Resamples from one texture to one that is 1/4 as large.
	id<MTLRenderPipelineState> _clearPipeline;			// Applies additional inter-frame clearing (cf. the stencil).

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
	// Scan targets receive scans, not full frames. Those scans may not cover the entire display,
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

		// TODO: decide what to do for downward-scaled direct-to-display. Obvious options are to include lowpass
		// filtering into the scan outputter and continue hoping that the vertical takes care of itself, or maybe
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

	Numeric::CircularCounter<size_t, NumBufferedLines> _lineOffsetBuffer;
	id<MTLBuffer> _lineOffsetBuffers[NumBufferedLines];	// Allocating NumBufferedLines buffers ensures these can't
														// possibly be exhausted; for this list to be exhausted there'd
														// have to be more draw calls in flight than there are lines for
														// them to operate upon.

	// The scan target in C++-world terms and the non-GPU storage for it.
	BufferingScanTarget _scanTarget;
	std::atomic_flag _isDrawing;

	// Additional pipeline information.
	std::atomic<bool> _isUsingSupersampling;

	// The output view and its aspect ratio.
	__weak MTKView *_view;
	CGFloat _viewAspectRatio;	// To avoid accessing .bounds away from the main thread.

	// Previously set modals, to avoid unnecessary buffer churn.
	std::optional<Outputs::Display::ScanTarget::Modals> _priorModals;
}

- (Uniforms *)uniforms {
	return reinterpret_cast<Uniforms *>(_uniformsBuffer.contents);
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
		_scanTarget.set_line_buffer(
			reinterpret_cast<BufferingScanTarget::Line *>(_linesBuffer.contents),
			NumBufferedLines
		);
		_scanTarget.set_scan_buffer(
			reinterpret_cast<BufferingScanTarget::Scan *>(_scansBuffer.contents),
			NumBufferedScans
		);

		// Generate copy and clear pipelines.
		id<MTLLibrary> library = [_view.device newDefaultLibrary];
		MTLRenderPipelineDescriptor *const pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
		pipelineDescriptor.colorAttachments[0].pixelFormat = _view.colorPixelFormat;
		pipelineDescriptor.vertexFunction = [library newFunctionWithName:@"copyVertex"];
		pipelineDescriptor.fragmentFunction = [library newFunctionWithName:@"copyFragment"];
		_copyPipeline = [_view.device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:nil];

		pipelineDescriptor.fragmentFunction = [library newFunctionWithName:@"interpolateFragment"];
		_supersamplePipeline = [_view.device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:nil];

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
			_lineOffsetBuffers[c] =
				[_view.device newBufferWithLength:sizeof(int) options:SharedResourceOptionsStandard];
		}

		// Ensure the is-drawing flag is initially clear.
		_isDrawing.clear();

		// Set initial aspect-ratio multiplier and generate buffers.
		[self mtkView:view drawableSizeWillChange:view.drawableSize];
	}

	return self;
}

- (void)mtkView:(nonnull MTKView *)view drawableSizeWillChange:(CGSize)size {
	_viewAspectRatio = size.width / size.height;
	[self setAspectRatio];

	@synchronized(self) {
		// Always [re]try multisampling upon a resize.
		_scanTarget.display_metrics_.announce_did_resize();
		_isUsingSupersampling = true;
		[self updateSizeBuffersToSize:size];
	}
}

- (void)updateSizeBuffers {
	@synchronized(self) {
		[self updateSizeBuffersToSize:_view.drawableSize];
	}
}

- (id<MTLCommandBuffer>)copyTexture:(id<MTLTexture>)source to:(id<MTLTexture>)destination {
	MTLRenderPassDescriptor *const copyTextureDescriptor = [[MTLRenderPassDescriptor alloc] init];
	copyTextureDescriptor.colorAttachments[0].texture = destination;
	copyTextureDescriptor.colorAttachments[0].loadAction = MTLLoadActionDontCare;
	copyTextureDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;

	id<MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];
	id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:copyTextureDescriptor];

	[encoder setRenderPipelineState:_copyPipeline];
	[encoder setVertexTexture:source atIndex:0];
	[encoder setFragmentTexture:source atIndex:0];

	[encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
	[encoder endEncoding];
	encoder = nil;
	[commandBuffer commit];

	return commandBuffer;
}

- (void)updateSizeBuffersToSize:(CGSize)size {
	// Anecdotally, the size provided here, which ultimately is from _view.drawableSize,
	// already factors in Retina-style scaling.
	//
	// 16384 has been the maximum texture size in all Mac versions of Metal so far, and
	// I haven't yet found a way to query it dynamically. So it's hard-coded.
	const NSUInteger frameBufferWidth = MIN(NSUInteger(size.width) * (_isUsingSupersampling ? 2 : 1), 16384);
	const NSUInteger frameBufferHeight = MIN(NSUInteger(size.height) * (_isUsingSupersampling ? 2 : 1), 16384);

	// Generate a framebuffer and a stencil.
	MTLTextureDescriptor *const textureDescriptor = [MTLTextureDescriptor
		texture2DDescriptorWithPixelFormat:_view.colorPixelFormat
		width:frameBufferWidth
		height:frameBufferHeight
		mipmapped:NO];
	textureDescriptor.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
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

	// Draw from _oldFrameBuffer to _frameBuffer; otherwise clear the new framebuffer.
	if(_oldFrameBuffer) {
		[self copyTexture:_oldFrameBuffer to:_frameBuffer];
	} else {
		// TODO: this use of clearTexture is the only reason _frameBuffer has a marked usage of
		// MTLTextureUsageShaderWrite; it'd probably be smarter to blank it with geometry rather than potentially
		// complicating its storage further?
		[self clearTexture:_frameBuffer];
	}

	// Don't clear the framebuffer at the end of this frame.
	_dontClearFrameBuffer = YES;
}

- (BOOL)shouldApplyGamma {
	return fabsf(float(self.uniforms->outputGamma) - 1.0f) > 0.01f;
}

- (void)clearTexture:(id<MTLTexture>)texture {
	id<MTLLibrary> library = [_view.device newDefaultLibrary];

	// Ensure finalised line texture is initially clear.
	id<MTLComputePipelineState> clearPipeline =
		[_view.device newComputePipelineStateWithFunction:[library newFunctionWithName:@"clearKernel"] error:nil];
	id<MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];
	id<MTLComputeCommandEncoder> computeEncoder = [commandBuffer computeCommandEncoder];

	[computeEncoder setTexture:texture atIndex:0];
	[self
		dispatchComputeCommandEncoder:computeEncoder
		pipelineState:clearPipeline
		width:texture.width
		height:texture.height
		offsetBuffer:[self bufferForOffset:0]
	];

	[computeEncoder endEncoding];
	[commandBuffer commit];
}

- (void)updateModalBuffers {
	// Build a descriptor for any intermediate line texture.
	MTLTextureDescriptor *const lineTextureDescriptor = [MTLTextureDescriptor
		texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
		width:BufferWidth
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
		[self clearTexture:_finalisedLineTexture];

		NSString *const kernelFunction =
			[self shouldApplyGamma] ? @"demodulateKernelWithGamma" : @"demodulateKernelNoGamma";
		_finalisedLineState =
			[_view.device newComputePipelineStateWithFunction:[library newFunctionWithName:kernelFunction] error:nil];
	}

	// A luma separation texture will exist only for composite colour.
	if(_pipeline == Pipeline::CompositeColour) {
		if(!_separatedLumaTexture) {
			_separatedLumaTexture = [_view.device newTextureWithDescriptor:lineTextureDescriptor];
			_separatedLumaState =
				[_view.device
					newComputePipelineStateWithFunction:[library newFunctionWithName:@"separateKernel"]
					error:nil];
		}
	} else {
		_separatedLumaTexture = nil;
	}
}

- (void)setAspectRatio {
	const auto transformation = aspect_ratio_transformation(_scanTarget.modals(), float(_viewAspectRatio));
	self.uniforms->sourcetoDisplay = simd_matrix_from_rows(
		simd_float3{transformation[0], transformation[3], transformation[6]},
		simd_float3{transformation[1], transformation[4], transformation[7]},
		simd_float3{transformation[2], transformation[5], transformation[8]}
	);
}

- (void)setModals:(const Outputs::Display::ScanTarget::Modals &)modals {
	//
	// Populate uniforms.
	//
	self.uniforms->scale[0] = modals.output_scale.x;
	self.uniforms->scale[1] = modals.output_scale.y;
	self.uniforms->lineWidth = 1.05f / modals.expected_vertical_lines;
	[self setAspectRatio];

	const auto toRGB = to_rgb_matrix(modals.composite_colour_space);
	self.uniforms->toRGB = simd::float3x3(
		simd::float3{toRGB[0], toRGB[1], toRGB[2]},
		simd::float3{toRGB[3], toRGB[4], toRGB[5]},
		simd::float3{toRGB[6], toRGB[7], toRGB[8]}
	);

	const auto fromRGB = from_rgb_matrix(modals.composite_colour_space);
	self.uniforms->fromRGB = simd::float3x3(
		simd::float3{fromRGB[0], fromRGB[1], fromRGB[2]},
		simd::float3{fromRGB[3], fromRGB[4], fromRGB[5]},
		simd::float3{fromRGB[6], fromRGB[7], fromRGB[8]}
	);

	// This is fixed for now; consider making it a function of frame rate and/or of whether frame syncing
	// is ongoing (which would require a way to signal that to this scan target).
	self.uniforms->outputAlpha = __fp16(0.64f);
	self.uniforms->outputMultiplier = __fp16(modals.brightness);

	const float displayGamma = 2.2f;	// This is assumed.
	self.uniforms->outputGamma = __fp16(displayGamma / modals.intended_gamma);

	if(
		!_priorModals ||
		_priorModals->display_type != modals.display_type ||
		_priorModals->input_data_type != modals.input_data_type
	) {
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

		// TODO: the call below is the only reason why this project now requires macOS 10.13;
		// is it all that helpful versus just uploading each frame?
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

		// Occasions when the composition buffer isn't required are slender: the output must be neither RGB
		// nor composite monochrome.
		const bool isComposition =
			modals.display_type != Outputs::Display::DisplayType::RGB &&
			modals.display_type != Outputs::Display::DisplayType::CompositeMonochrome;
		const bool isSVideoOutput = modals.display_type == Outputs::Display::DisplayType::SVideo;

		if(!isComposition) {
			_pipeline = Pipeline::DirectToDisplay;
		} else {
			_pipeline = isSVideoOutput ? Pipeline::SVideo : Pipeline::CompositeColour;
		}

		float &cyclesMultiplier = self.uniforms->cyclesMultiplier;
		if(_pipeline != Pipeline::DirectToDisplay) {
			cyclesMultiplier =
				Outputs::Display::FilterGenerator::suggested_sample_multiplier(
					float(modals.colour_cycle_numerator) / float(modals.colour_cycle_denominator),
					modals.cycles_per_line
				);

			// Create suitable filters.
			_lineBufferPixelsPerLine = NSUInteger(modals.cycles_per_line * cyclesMultiplier);
			const float colourCyclesPerLine =
				float(modals.colour_cycle_numerator) / float(modals.colour_cycle_denominator);
			using DecodingPath = Outputs::Display::FilterGenerator::DecodingPath;

			Outputs::Display::FilterGenerator generator(
				_lineBufferPixelsPerLine,
				colourCyclesPerLine,
				isSVideoOutput ? DecodingPath::SVideo : DecodingPath::Composite
			);

			const auto separation = generator.separation_filter();
			using Coefficients2 = std::array<simd::float2, 31>;
			Coefficients2 separation_multiplexed{};
			separation.luma.copy_to<Coefficients2::iterator>(
				separation_multiplexed.begin(),
				separation_multiplexed.end(),
				[](const auto destination, const float value) {
					destination->x = value;
				}
			);
			separation.chroma.copy_to<Coefficients2::iterator>(
				separation_multiplexed.begin(),
				separation_multiplexed.end(),
				[](const auto destination, const float value) {
					destination->y = value;
				}
			);
			for(size_t c = 0; c < 16; ++c) {
				self.uniforms->lumaKernel[c] = separation_multiplexed[c];
			}

			const auto demodulation = generator.demouldation_filter();
			using Coefficients3 = std::array<simd::float3, 31>;
			Coefficients3 demodulation_multiplexed{};
			demodulation.luma.copy_to<Coefficients3::iterator>(
				demodulation_multiplexed.begin(),
				demodulation_multiplexed.end(),
				[](const auto destination, const float value) {
					destination->x = value;
				}
			);
			demodulation.chroma.copy_to<Coefficients3::iterator>(
				demodulation_multiplexed.begin(),
				demodulation_multiplexed.end(),
				[](const auto destination, const float value) {
					destination->y = destination->z = value;
				}
			);
			// Convert to half-size floats.
			for(size_t c = 0; c < 16; ++c) {
				self.uniforms->chromaKernel[c] = demodulation_multiplexed[c];
			}
		}

		// Update intermediate storage.
		[self updateModalBuffers];

		const auto fragment_function = [&](NSString *const prefix) {
			NSString *const functionName = [prefix stringByAppendingFormat:@"%s", name(modals.input_data_type)];
			id <MTLFunction> function = [library newFunctionWithName:functionName];
			assert(function);
			return function;
		};

		if(_pipeline != Pipeline::DirectToDisplay) {
			// Create the composition render pass.
			pipelineDescriptor.colorAttachments[0].pixelFormat = _compositionTexture.pixelFormat;
			pipelineDescriptor.vertexFunction = [library newFunctionWithName:@"scanToComposition"];
			pipelineDescriptor.fragmentFunction =
				fragment_function(isSVideoOutput ? @"internalSVideo" : @"internalComposite");

			_composePipeline = [_view.device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:nil];

			_compositionRenderPass = [[MTLRenderPassDescriptor alloc] init];
			_compositionRenderPass.colorAttachments[0].texture = _compositionTexture;
			_compositionRenderPass.colorAttachments[0].loadAction = MTLLoadActionClear;
			_compositionRenderPass.colorAttachments[0].storeAction = MTLStoreActionStore;
			_compositionRenderPass.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.5, 0.5, 0.3);
		}

		// Build the output pipeline.
		pipelineDescriptor.colorAttachments[0].pixelFormat = _view.colorPixelFormat;
		pipelineDescriptor.vertexFunction =
			[library newFunctionWithName:_pipeline == Pipeline::DirectToDisplay ? @"scanToDisplay" : @"lineToDisplay"];

		if(_pipeline != Pipeline::DirectToDisplay) {
			pipelineDescriptor.fragmentFunction = [library newFunctionWithName:@"interpolateFragment"];
		} else {
			const bool isRGBOutput = modals.display_type == Outputs::Display::DisplayType::RGB;
			pipelineDescriptor.fragmentFunction = fragment_function(
				[isRGBOutput ? @"outputRGB" : @"outputComposite"
					stringByAppendingString:self.shouldApplyGamma ? @"WithGamma" : @""]
			);
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
	_priorModals = modals;
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

	range_perform(
		start,
		end,
		_pipeline != Pipeline::DirectToDisplay ? NumBufferedLines : NumBufferedScans,
		[&](const size_t start, const size_t size) {
			[encoder
				drawPrimitives:MTLPrimitiveTypeTriangleStrip
				vertexStart:0
				vertexCount:4
				instanceCount:size
				baseInstance:start
			];
		});

	// Complete encoding.
	[encoder endEncoding];
	encoder = nil;
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
	encoder = nil;
}

- (void)
	composeOutputArea:(const BufferingScanTarget::OutputArea &)outputArea
	commandBuffer:(id<MTLCommandBuffer>)commandBuffer
{
	// Output all scans to the composition buffer.
	id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:_compositionRenderPass];
	[encoder setRenderPipelineState:_composePipeline];

	[encoder setVertexBuffer:_scansBuffer offset:0 atIndex:0];
	[encoder setVertexBuffer:_uniformsBuffer offset:0 atIndex:1];
	[encoder setVertexTexture:_compositionTexture atIndex:0];

	[encoder setFragmentBuffer:_uniformsBuffer offset:0 atIndex:0];
	[encoder setFragmentTexture:_writeAreaTexture atIndex:0];

	range_perform(
		outputArea.begin.scan,
		outputArea.end.scan,
		NumBufferedScans,
		[&](const size_t start, const size_t size) {
			[encoder
				drawPrimitives:MTLPrimitiveTypeLine
				vertexStart:0
				vertexCount:2
				instanceCount:size
				baseInstance:start
			];
		}
	);
	[encoder endEncoding];
	encoder = nil;
}

- (id<MTLBuffer>)bufferForOffset:(size_t)offset {
	// Store and apply the offset.
	const auto buffer = _lineOffsetBuffers[_lineOffsetBuffer];
	*(reinterpret_cast<int *>(_lineOffsetBuffers[_lineOffsetBuffer].contents)) = int(offset);
	++_lineOffsetBuffer;
	return buffer;
}

- (void)
	dispatchComputeCommandEncoder:(id<MTLComputeCommandEncoder>)encoder
	pipelineState:(id<MTLComputePipelineState>)pipelineState
	width:(NSUInteger)width
	height:(NSUInteger)height
	offsetBuffer:(id<MTLBuffer>)offsetBuffer
{
	[encoder setBuffer:offsetBuffer offset:0 atIndex:1];

	// Follows https://developer.apple.com/documentation/metal/calculating_threadgroup_and_grid_sizes ;
	// I have no independent opinion whatsoever.
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

- (void)drawInMTKView:(nonnull MTKView *)view {
	if(_isDrawing.test_and_set()) {
		_scanTarget.display_metrics_.announce_draw_status(false);
		return;
	}

	// Disable supersampling if performance requires it.
	if(_isUsingSupersampling && _scanTarget.display_metrics_.should_lower_resolution()) {
		_isUsingSupersampling = false;
		[self updateSizeBuffers];
	}

	// Schedule a copy from the current framebuffer to the view;
	// blitting is unavailable as the target is a framebuffer texture.
	id<MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];

	// Every pixel will be drawn, so don't clear or reload.
	view.currentRenderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionDontCare;
	id<MTLRenderCommandEncoder> encoder =
		[commandBuffer renderCommandEncoderWithDescriptor:view.currentRenderPassDescriptor];

	[encoder setRenderPipelineState:_isUsingSupersampling ? _supersamplePipeline : _copyPipeline];
	[encoder setVertexTexture:_frameBuffer atIndex:0];
	[encoder setFragmentTexture:_frameBuffer atIndex:0];

	[encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
	[encoder endEncoding];
	encoder = nil;

	[commandBuffer presentDrawable:view.currentDrawable];
	[commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> _Nonnull) {
		self->_isDrawing.clear();
		self->_scanTarget.display_metrics_.announce_draw_status(true);
	}];
	[commandBuffer commit];
}

// MARK: - Per-frame output.

- (void)updateFrameBuffer {
	if(_scanTarget.has_new_modals()) {
		// TODO: rethink BufferingScanTarget::perform. Is it now really just for guarding the modals?
		_scanTarget.perform([=] {
			const Outputs::Display::ScanTarget::Modals *const newModals = _scanTarget.new_modals();
			if(newModals) {
				[self setModals:*newModals];
			}
		});
	}

	@synchronized(self) {
		if(!_frameBufferRenderPass) return;

		const auto outputArea = _scanTarget.get_output_area();

		// Ensure texture changes are noted.
		const auto writeAreaModificationStart =
			size_t(outputArea.begin.write_area_x + outputArea.begin.write_area_y * BufferWidth)
				* _bytesPerInputPixel;
		const auto writeAreaModificationEnd =
			size_t(outputArea.end.write_area_x + outputArea.end.write_area_y * BufferWidth)
				* _bytesPerInputPixel;
		range_perform(
			writeAreaModificationStart,
			writeAreaModificationEnd,
			_totalTextureBytes,
			[&](const size_t start, const size_t size) {
				[_writeAreaBuffer didModifyRange:NSMakeRange(start, size)];
			}
		);

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
			case Pipeline::DirectToDisplay:
				_scanTarget.output_scans(
					outputArea,
					[&](const size_t begin, const size_t end) {
						[self outputFrom:begin to:end commandBuffer:commandBuffer];
					},
					[&](const bool was_complete) {
						if(was_complete && !_dontClearFrameBuffer) {
							[self outputFrameCleanerToCommandBuffer:commandBuffer];
						}
						_dontClearFrameBuffer = NO;
					}
				);
			break;

			case Pipeline::CompositeColour:
			case Pipeline::SVideo:
				// Build the composition buffer.
				[self composeOutputArea:outputArea commandBuffer:commandBuffer];

				if(_pipeline == Pipeline::SVideo) {
					// Filter from composition to the finalised line texture.
					id<MTLComputeCommandEncoder> computeEncoder = [commandBuffer computeCommandEncoder];
					[computeEncoder setTexture:_compositionTexture atIndex:0];
					[computeEncoder setTexture:_finalisedLineTexture atIndex:1];
					[computeEncoder setBuffer:_uniformsBuffer offset:0 atIndex:0];

					if(outputArea.end.line > outputArea.begin.line) {
						[self
							dispatchComputeCommandEncoder:computeEncoder
							pipelineState:_finalisedLineState
							width:_lineBufferPixelsPerLine
							height:outputArea.end.line - outputArea.begin.line
							offsetBuffer:[self bufferForOffset:outputArea.begin.line]
						];
					} else {
						[self
							dispatchComputeCommandEncoder:computeEncoder
							pipelineState:_finalisedLineState
							width:_lineBufferPixelsPerLine
							height:NumBufferedLines - outputArea.begin.line
							offsetBuffer:[self bufferForOffset:outputArea.begin.line]
						];

						if(outputArea.end.line) {
							[self
								dispatchComputeCommandEncoder:computeEncoder
								pipelineState:_finalisedLineState
								width:_lineBufferPixelsPerLine
								height:outputArea.end.line
								offsetBuffer:[self bufferForOffset:0]
							];
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
					offsetBuffers[0] = [self bufferForOffset:outputArea.begin.line];

					if(outputArea.end.line > outputArea.begin.line) {
						[self
							dispatchComputeCommandEncoder:computeEncoder
							pipelineState:_separatedLumaState
							width:_lineBufferPixelsPerLine
							height:outputArea.end.line - outputArea.begin.line
							offsetBuffer:offsetBuffers[0]
						];
					} else {
						[self
							dispatchComputeCommandEncoder:computeEncoder
							pipelineState:_separatedLumaState
							width:_lineBufferPixelsPerLine
							height:NumBufferedLines - outputArea.begin.line
							offsetBuffer:offsetBuffers[0]
						];
						if(outputArea.end.line) {
							offsetBuffers[1] = [self bufferForOffset:0];
							[self
								dispatchComputeCommandEncoder:computeEncoder
								pipelineState:_separatedLumaState
								width:_lineBufferPixelsPerLine
								height:outputArea.end.line
								offsetBuffer:offsetBuffers[1]
							];
						}
					}

					// Filter resulting chrominance.
					[computeEncoder setTexture:_separatedLumaTexture atIndex:0];
					[computeEncoder setTexture:_finalisedLineTexture atIndex:1];
					[computeEncoder setBuffer:_uniformsBuffer offset:0 atIndex:0];

					if(outputArea.end.line > outputArea.begin.line) {
						[self
							dispatchComputeCommandEncoder:computeEncoder
							pipelineState:_finalisedLineState
							width:_lineBufferPixelsPerLine
							height:outputArea.end.line - outputArea.begin.line
							offsetBuffer:offsetBuffers[0]
						];
					} else {
						[self
							dispatchComputeCommandEncoder:computeEncoder
							pipelineState:_finalisedLineState
							width:_lineBufferPixelsPerLine
							height:NumBufferedLines - outputArea.begin.line
							offsetBuffer:offsetBuffers[0]
						];
						if(outputArea.end.line) {
							[self
								dispatchComputeCommandEncoder:computeEncoder
								pipelineState:_finalisedLineState
								width:_lineBufferPixelsPerLine
								height:outputArea.end.line
								offsetBuffer:offsetBuffers[1]
							];
						}
					}

					[computeEncoder endEncoding];
				}

				_scanTarget.output_lines(
					outputArea,
					[&](const size_t begin, const size_t end) {
						[self outputFrom:begin to:end commandBuffer:commandBuffer];
					},
					[&](const bool was_complete) {
						if(was_complete && !_dontClearFrameBuffer) {
							[self outputFrameCleanerToCommandBuffer:commandBuffer];
						}
						_dontClearFrameBuffer = NO;
					}
				);
			break;
		}

		// Add a callback to update the scan target buffer and commit the drawing.
		[commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> _Nonnull) {
			@synchronized (self) {
				self->_scanTarget.complete_output_area(outputArea);
			}
		}];
		[commandBuffer commit];
	}
}

// MARK: - External connections.

- (Outputs::Display::ScanTarget *)scanTarget {
	return &_scanTarget;
}

- (void)willChangeOwner {
	self.scanTarget->will_change_owner();
}

- (NSBitmapImageRep *)imageRepresentation {
	// Create an NSBitmapRep as somewhere to copy pixel data to.
	NSBitmapImageRep *const result =
		[[NSBitmapImageRep alloc]
			initWithBitmapDataPlanes:NULL
			pixelsWide:(NSInteger)_frameBuffer.width
			pixelsHigh:(NSInteger)_frameBuffer.height
			bitsPerSample:8
			samplesPerPixel:4
			hasAlpha:YES
			isPlanar:NO
			colorSpaceName:NSDeviceRGBColorSpace
			bytesPerRow:4 * (NSInteger)_frameBuffer.width
			bitsPerPixel:0];

	// Create a CPU-accessible texture and copy the current contents of the _frameBuffer to it.
	// TODO: supersample rather than directly copy if appropriate?
	id<MTLTexture> cpuTexture;
	MTLTextureDescriptor *const textureDescriptor = [MTLTextureDescriptor
		texture2DDescriptorWithPixelFormat:_view.colorPixelFormat
		width:_frameBuffer.width
		height:_frameBuffer.height
		mipmapped:NO];
	textureDescriptor.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
	textureDescriptor.resourceOptions = MTLResourceStorageModeManaged;
	cpuTexture = [_view.device newTextureWithDescriptor:textureDescriptor];
	[[self copyTexture:_frameBuffer to:cpuTexture] waitUntilCompleted];

	// Copy from the CPU-visible texture to the bitmap image representation.
	uint8_t *const bitmapData = result.bitmapData;
	[cpuTexture
		getBytes:bitmapData
		bytesPerRow:_frameBuffer.width*4
		fromRegion:MTLRegionMake2D(0, 0, _frameBuffer.width, _frameBuffer.height)
		mipmapLevel:0];

	// Set alpha to fully opaque and do some byte shuffling if necessary;
	// Apple likes BGR for output but RGB is the best I can specify to NSBitmapImageRep.
	//
	// I'm not putting my foot down and having the GPU do the conversion I want
	// because this lets me reuse _copyPipeline and thereby cut down on boilerplate,
	// especially given that screenshots are not a bottleneck.
	const NSUInteger totalBytes = _frameBuffer.width * _frameBuffer.height * 4;
	const bool flipRedBlue = _view.colorPixelFormat == MTLPixelFormatBGRA8Unorm;
	for(NSUInteger offset = 0; offset < totalBytes; offset += 4) {
		if(flipRedBlue) {
			const uint8_t red = bitmapData[offset];
			bitmapData[offset] = bitmapData[offset+2];
			bitmapData[offset+2] = red;
		}
		bitmapData[offset+3] = 0xff;
	}

	return result;
}

@end
