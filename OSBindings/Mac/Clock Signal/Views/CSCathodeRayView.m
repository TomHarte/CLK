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

typedef NS_ENUM(NSInteger, CSOpenGLViewCondition) {
	CSOpenGLViewConditionReadyForUpdate,
	CSOpenGLViewConditionUpdating
};

@implementation CSCathodeRayView {
	CVDisplayLinkRef _displayLink;
	NSConditionLock *_runningLock;
	dispatch_queue_t _dispatchQueue;
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

	// Create a queue and a condition lock for dispatching to it
	_runningLock = [[NSConditionLock alloc] initWithCondition:CSOpenGLViewConditionReadyForUpdate];
	_dispatchQueue = dispatch_queue_create("com.thomasharte.clocksignal.GL", DISPATCH_QUEUE_SERIAL);
 
	// Set the display link for the current renderer
	CGLContextObj cglContext = [[self openGLContext] CGLContextObj];
	CGLPixelFormatObj cglPixelFormat = [[self pixelFormat] CGLPixelFormatObj];
	CVDisplayLinkSetCurrentCGDisplayFromOpenGLContext(_displayLink, cglContext, cglPixelFormat);

	// set the clear colour
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
	[view drawAtTime:now];
	return kCVReturnSuccess;
}

- (void)drawAtTime:(const CVTimeStamp *)now
{
	if([_runningLock tryLockWhenCondition:CSOpenGLViewConditionReadyForUpdate])
	{
		CVTimeStamp timeStamp = *now;
		dispatch_async(_dispatchQueue, ^{
			[_runningLock lockWhenCondition:CSOpenGLViewConditionUpdating];
			[self.delegate openGLView:self didUpdateToTime:timeStamp];
			[self drawViewOnlyIfDirty:YES];
			[_runningLock unlockWithCondition:CSOpenGLViewConditionReadyForUpdate];
		});
		[_runningLock unlockWithCondition:CSOpenGLViewConditionUpdating];
	}
}

- (void)invalidate
{
	CVDisplayLinkStop(_displayLink);
	[_runningLock lockWhenCondition:CSOpenGLViewConditionReadyForUpdate];
	[_runningLock unlock];
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

	[self.openGLContext makeCurrentContext];
	CGLLockContext([[self openGLContext] CGLContextObj]);

	CGSize viewSize = [self backingSize];
	glViewport(0, 0, (GLsizei)viewSize.width, (GLsizei)viewSize.height);

	CGLUnlockContext([[self openGLContext] CGLContextObj]);
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
