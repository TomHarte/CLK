//
//  CSCathodeRayView.m
//  CLK
//
//  Created by Thomas Harte on 16/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#import "CSCathodeRayView.h"
@import CoreVideo;
@import GLKit;
//#import <OpenGL/gl3.h>
//#import <OpenGL/gl3ext.h>
//#import <libkern/OSAtomic.h>


@implementation CSCathodeRayView {
	CVDisplayLinkRef _displayLink;
	CGRect _aspectRatioCorrectedBounds;
}

/*- (GLuint)textureForImageNamed:(NSString *)name
{
	NSImage *const image = [NSImage imageNamed:name];
	NSBitmapImageRep *bitmapRepresentation = [[NSBitmapImageRep alloc] initWithData: [image TIFFRepresentation]];

	GLuint textureName;
	glGenTextures(1, &textureName);
	glBindTexture(GL_TEXTURE_2D, textureName);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, (GLsizei)image.size.width, (GLsizei)image.size.height, 0, GL_RGB, GL_UNSIGNED_BYTE, bitmapRepresentation.bitmapData);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glGenerateMipmap(GL_TEXTURE_2D);

	return textureName;
}*/

- (void)prepareOpenGL
{
	// Synchronize buffer swaps with vertical refresh rate
	GLint swapInt = 1;
	[[self openGLContext] setValues:&swapInt forParameter:NSOpenGLCPSwapInterval];

	// Create a display link capable of being used with all active displays
	CVDisplayLinkCreateWithActiveCGDisplays(&_displayLink);
 
	// Set the renderer output callback function
	CVDisplayLinkSetOutputCallback(_displayLink, DisplayLinkCallback, (__bridge void * __nullable)(self));
 
	// Set the display link for the current renderer
	CGLContextObj cglContext = [[self openGLContext] CGLContextObj];
	CGLPixelFormatObj cglPixelFormat = [[self pixelFormat] CGLPixelFormatObj];
	CVDisplayLinkSetCurrentCGDisplayFromOpenGLContext(_displayLink, cglContext, cglPixelFormat);

	// install the shadow mask texture as the second texture
/*	glActiveTexture(GL_TEXTURE1);
	_shadowMaskTextureName = [self textureForImageNamed:@"ShadowMask"];

	// otherwise, we'll be working on the first texture
	glActiveTexture(GL_TEXTURE0);*/

	// get the shader ready, set the clear colour
	[self.openGLContext makeCurrentContext];
	glClearColor(0.0, 0.0, 0.0, 1.0);

	// Activate the display link
	CVDisplayLinkStart(_displayLink);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
}

static CVReturn DisplayLinkCallback(CVDisplayLinkRef displayLink, const CVTimeStamp *now, const CVTimeStamp *outputTime, CVOptionFlags flagsIn, CVOptionFlags *flagsOut, void *displayLinkContext)
{
	CSCathodeRayView *view = (__bridge CSCathodeRayView *)displayLinkContext;
	[view.delegate openGLView:view didUpdateToTime:*now];
	[view drawViewOnlyIfDirty:YES];
	return kCVReturnSuccess;
}

- (void)invalidate
{
	CVDisplayLinkStop(_displayLink);
}

- (void)dealloc
{
	// Release the display link
	CVDisplayLinkRelease(_displayLink);

	// Release OpenGL buffers
//	[self.openGLContext makeCurrentContext];
//	glDeleteBuffers(1, &_arrayBuffer);
//	glDeleteVertexArrays(1, &_vertexArray);
//	glDeleteTextures(1, &_textureName);
//	glDeleteTextures(1, &_shadowMaskTextureName);
//	glDeleteProgram(_shaderProgram);
}

- (CGSize)backingSize
{
	return [self convertSizeToBacking:self.bounds.size];
}

- (void)reshape
{
	[super reshape];

	[self.openGLContext makeCurrentContext];
	CGLLockContext([[self openGLContext] CGLContextObj]);

	CGSize viewSize = [self backingSize];
	glViewport(0, 0, (GLsizei)viewSize.width, (GLsizei)viewSize.height);

//	[self pushSizeUniforms];

	CGLUnlockContext([[self openGLContext] CGLContextObj]);
}

//- (void)setFrameBounds:(CGRect)frameBounds
//{
//	_frameBounds = frameBounds;
//
//	[self.openGLContext makeCurrentContext];
//	CGLLockContext([[self openGLContext] CGLContextObj]);
//
//	[self pushSizeUniforms];
//
//	CGLUnlockContext([[self openGLContext] CGLContextObj]);
//}
//
//- (void)pushSizeUniforms
//{
//	if(_shaderProgram)
//	{
//		NSPoint viewSize = [self backingViewSize];
//		if(_windowSizeUniform >= 0)
//		{
//			glUniform2f(_windowSizeUniform, (GLfloat)viewSize.x, (GLfloat)viewSize.y);
//		}
//
//		CGFloat outputAspectRatioMultiplier = (viewSize.x / viewSize.y) / (4.0 / 3.0);
//
////		NSLog(@"%0.2f v %0.2f", outputAspectRatio, desiredOutputAspectRatio);
//		_aspectRatioCorrectedBounds = _frameBounds;
//
//		CGFloat bonusWidth = (outputAspectRatioMultiplier - 1.0f) * _frameBounds.size.width;
//		_aspectRatioCorrectedBounds.origin.x -= bonusWidth * 0.5f * _aspectRatioCorrectedBounds.size.width;
//		_aspectRatioCorrectedBounds.size.width *= outputAspectRatioMultiplier;
//
//		if(_boundsOriginUniform >= 0) glUniform2f(_boundsOriginUniform, (GLfloat)_aspectRatioCorrectedBounds.origin.x, (GLfloat)_aspectRatioCorrectedBounds.origin.y);
//		if(_boundsSizeUniform >= 0) glUniform2f(_boundsSizeUniform, (GLfloat)_aspectRatioCorrectedBounds.size.width, (GLfloat)_aspectRatioCorrectedBounds.size.height);
//	}
//}

- (void)awakeFromNib
{
	NSOpenGLPixelFormatAttribute attributes[] =
	{
		NSOpenGLPFADoubleBuffer,
		NSOpenGLPFAOpenGLProfile,	NSOpenGLProfileVersion3_2Core,
		NSOpenGLPFASampleBuffers,	1,
		NSOpenGLPFASamples,			16,
		0
	};

	NSOpenGLPixelFormat *pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
	NSOpenGLContext *context = [[NSOpenGLContext alloc] initWithFormat:pixelFormat shareContext:nil];

#ifdef DEBUG
	// When we're using a CoreProfile context, crash if we call a legacy OpenGL function
	// This will make it much more obvious where and when such a function call is made so
	// that we can remove such calls.
	// Without this we'd simply get GL_INVALID_OPERATION error for calling legacy functions
	// but it would be more difficult to see where that function was called.
	CGLEnable([context CGLContextObj], kCGLCECrashOnRemovedFunctions);
#endif

	self.pixelFormat = pixelFormat;
	self.openGLContext = context;
	self.wantsBestResolutionOpenGLSurface = YES;

	// establish default instance variable values
//	self.frameBounds = CGRectMake(0.0, 0.0, 1.0, 1.0);
}

- (void)drawRect:(NSRect)dirtyRect
{
	[self drawViewOnlyIfDirty:NO];
}

- (void)drawViewOnlyIfDirty:(BOOL)onlyIfDirty
{
	[self.openGLContext makeCurrentContext];
	CGLLockContext([[self openGLContext] CGLContextObj]);

	[self.delegate openGLView:self drawViewOnlyIfDirty:onlyIfDirty];

	CGLFlushDrawable([[self openGLContext] CGLContextObj]);
	CGLUnlockContext([[self openGLContext] CGLContextObj]);
}

#pragma mark - NSResponder

- (BOOL)acceptsFirstResponder
{
	return YES;
}

- (void)keyDown:(NSEvent *)theEvent
{
	[self.responderDelegate keyDown:theEvent];
}

- (void)keyUp:(NSEvent *)theEvent
{
	[self.responderDelegate keyUp:theEvent];
}

- (void)flagsChanged:(NSEvent *)theEvent
{
	[self.responderDelegate flagsChanged:theEvent];
}

@end
