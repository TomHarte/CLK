//
//  CSOpenGLView
//  CLK
//
//  Created by Thomas Harte on 16/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#import "CSOpenGLView.h"
@import CoreVideo;
@import GLKit;

@interface CSOpenGLView () <NSDraggingDestination>
@end

@implementation CSOpenGLView {
	CVDisplayLinkRef _displayLink;
}

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

	// set the clear colour
	[self.openGLContext makeCurrentContext];
	glClearColor(0.0, 0.0, 0.0, 1.0);

	// Activate the display link
	CVDisplayLinkStart(_displayLink);
}

static CVReturn DisplayLinkCallback(CVDisplayLinkRef displayLink, const CVTimeStamp *now, const CVTimeStamp *outputTime, CVOptionFlags flagsIn, CVOptionFlags *flagsOut, void *displayLinkContext)
{
	CSOpenGLView *const view = (__bridge CSOpenGLView *)displayLinkContext;
	[view drawAtTime:now frequency:CVDisplayLinkGetActualOutputVideoRefreshPeriod(displayLink)];
	return kCVReturnSuccess;
}

- (void)drawAtTime:(const CVTimeStamp *)now frequency:(double)frequency
{
	// Draw the display now regardless of other activity.
	[self drawViewOnlyIfDirty:YES];
}

- (void)invalidate
{
	CVDisplayLinkStop(_displayLink);
}

- (void)dealloc
{
	// Release the display link
	CVDisplayLinkRelease(_displayLink);
}

- (CGSize)backingSize
{
	return [self convertSizeToBacking:self.bounds.size];
}

- (void)reshape
{
	[super reshape];

	[self performWithGLContext:^{
		CGSize viewSize = [self backingSize];
		glViewport(0, 0, (GLsizei)viewSize.width, (GLsizei)viewSize.height);
	}];
}

- (void)awakeFromNib
{
	NSOpenGLPixelFormatAttribute attributes[] =
	{
		NSOpenGLPFADoubleBuffer,
		NSOpenGLPFAOpenGLProfile,	NSOpenGLProfileVersion3_2Core,
		NSOpenGLPFAMultisample,
		NSOpenGLPFASampleBuffers,	1,
		NSOpenGLPFASamples,			2,
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

	// Register to receive dragged and dropped file URLs.
	[self registerForDraggedTypes:@[(__bridge NSString *)kUTTypeFileURL]];
}

- (void)drawRect:(NSRect)dirtyRect
{
	[self drawViewOnlyIfDirty:NO];
}

- (void)drawViewOnlyIfDirty:(BOOL)onlyIfDirty
{
	[self performWithGLContext:^{
		[self.delegate openGLView:self drawViewOnlyIfDirty:onlyIfDirty];
		CGLFlushDrawable([[self openGLContext] CGLContextObj]);
	}];
}

- (void)performWithGLContext:(dispatch_block_t)action
{
	CGLLockContext([[self openGLContext] CGLContextObj]);
	[self.openGLContext makeCurrentContext];
	action();
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

#pragma mark - NSDraggingDestination

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
	for(NSPasteboardItem *item in [[sender draggingPasteboard] pasteboardItems])
	{
		NSURL *URL = [NSURL URLWithString:[item stringForType:(__bridge NSString *)kUTTypeFileURL]];

		NSLog(@"%@", URL);
		[self.delegate openGLView:self didReceiveFileAtURL:URL];
	}
	return YES;
}

- (NSDragOperation)draggingEntered:(id < NSDraggingInfo >)sender
{
	// we'll drag and drop, yeah?
	return NSDragOperationLink;
}

@end
