//
//  OpenGLView.m
//  ElectrEm
//
//  Created by Thomas Harte on 16/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#import "OpenGLView.h"
@import CoreVideo;
#import <OpenGL/gl.h>

@implementation CSOpenGLView {
	CVDisplayLinkRef displayLink;
}

- (void)prepareOpenGL
{
	// Synchronize buffer swaps with vertical refresh rate
	GLint swapInt = 1;
	[[self openGLContext] setValues:&swapInt forParameter:NSOpenGLCPSwapInterval];

	// Create a display link capable of being used with all active displays
	CVDisplayLinkCreateWithActiveCGDisplays(&displayLink);
 
	// Set the renderer output callback function
	CVDisplayLinkSetOutputCallback(displayLink, DisplayLinkCallback, (__bridge void * __nullable)(self));
 
	// Set the display link for the current renderer
	CGLContextObj cglContext = [[self openGLContext] CGLContextObj];
	CGLPixelFormatObj cglPixelFormat = [[self pixelFormat] CGLPixelFormatObj];
	CVDisplayLinkSetCurrentCGDisplayFromOpenGLContext(displayLink, cglContext, cglPixelFormat);
 
	// Activate the display link
	CVDisplayLinkStart(displayLink);
}

static CVReturn DisplayLinkCallback(CVDisplayLinkRef displayLink, const CVTimeStamp* now, const CVTimeStamp* outputTime, CVOptionFlags flagsIn, CVOptionFlags* flagsOut, void* displayLinkContext)
{
	CSOpenGLView *view = (__bridge CSOpenGLView *)displayLinkContext;
	[view.delegate openGLView:view didUpdateToTime:*now];
	return kCVReturnSuccess;
}
 
- (void)dealloc
{
	// Release the display link
	CVDisplayLinkRelease(displayLink);
}

- (void)drawRect:(NSRect)dirtyRect
{
	[self.openGLContext makeCurrentContext];

	CGSize viewSize = [self convertSize:self.bounds.size toView:self];
	glViewport((GLint)0, (GLint)0, (GLsizei)viewSize.width, (GLsizei)viewSize.height);
	[self.delegate openGLViewDrawView:self];

	glSwapAPPLE();
}

@end
