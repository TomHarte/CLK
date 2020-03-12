//
//  CSOpenGLView
//  CLK
//
//  Created by Thomas Harte on 16/07/2015.
//  Copyright 2015 Thomas Harte. All rights reserved.
//

#import "CSOpenGLView.h"
#import "CSApplication.h"
@import CoreVideo;
@import GLKit;

@interface CSOpenGLView () <NSDraggingDestination, CSApplicationEventDelegate>
@end

@implementation CSOpenGLView {
	CVDisplayLinkRef _displayLink;
	CGSize _backingSize;
	NSScreen *_currentScreen;

	NSTrackingArea *_mouseTrackingArea;
	NSTimer *_mouseHideTimer;
	BOOL _mouseIsCaptured;
}

- (void)prepareOpenGL {
	[super prepareOpenGL];

	// Note the initial screen.
	_currentScreen = self.window.screen;

	// set the clear colour
	[self.openGLContext makeCurrentContext];
	glClearColor(0.0, 0.0, 0.0, 1.0);

	// Setup the [initial] display link.
	[self setupDisplayLink];
}

- (void)setupDisplayLink {
	// Kill the existing link if there is one, then wait until its final shout is definitely done.
	if(_displayLink) {
		[self invalidate];

		CVDisplayLinkRelease(_displayLink);
	}

	// Create a display link capable of being used with all active displays
	NSNumber *const screenNumber = self.window.screen.deviceDescription[@"NSScreenNumber"];
	CVDisplayLinkCreateWithCGDisplay(screenNumber.unsignedIntValue, &_displayLink);

	// Set the renderer output callback function
	CVDisplayLinkSetOutputCallback(_displayLink, DisplayLinkCallback, (__bridge void * __nullable)(self));

	// Set the display link for the current renderer
	CGLContextObj cglContext = [[self openGLContext] CGLContextObj];
	CGLPixelFormatObj cglPixelFormat = [[self pixelFormat] CGLPixelFormatObj];
	CVDisplayLinkSetCurrentCGDisplayFromOpenGLContext(_displayLink, cglContext, cglPixelFormat);

	// Activate the display link
	CVDisplayLinkStart(_displayLink);
}

static CVReturn DisplayLinkCallback(CVDisplayLinkRef displayLink, const CVTimeStamp *now, const CVTimeStamp *outputTime, CVOptionFlags flagsIn, CVOptionFlags *flagsOut, void *displayLinkContext) {
	CSOpenGLView *const view = (__bridge CSOpenGLView *)displayLinkContext;

	[view checkDisplayLink];
	[view.displayLinkDelegate openGLViewDisplayLinkDidFire:view now:now outputTime:outputTime];
	/*
		Do not touch the display link from after this call; there's a bit of a race condition with setupDisplayLink.
		Specifically: Apple provides CVDisplayLinkStop but a call to that merely prevents future calls to the callback,
		it doesn't wait for completion of any current calls. So I've set up a usleep for one callback's duration,
		so code in here gets one callback's duration to access the display link.

		In practice, it should do so only upon entry, and before calling into the view. The view promises not to
		access the display link itself as part of -drawAtTime:frequency:.
	*/

	return kCVReturnSuccess;
}

- (void)checkDisplayLink {
	// Test now whether the screen this view is on has changed since last time it was checked.
	// There's likely a callback available for this, on NSWindow if nowhere else, or an NSNotification,
	// but since this method is going to be called repeatedly anyway, and the test is cheap, polling
	// feels fine.
	if(self.window.screen != _currentScreen) {
		_currentScreen = self.window.screen;

		// Issue a reshape, in case a switch to/from a Retina display has
		// happened, changing the results of -convertSizeToBacking:, etc.
		[self reshape];

		// Also switch display links, to make sure synchronisation is with the display
		// the window is actually on, and at its rate.
		[self setupDisplayLink];
	}
}

- (void)drawAtTime:(const CVTimeStamp *)now frequency:(double)frequency {
	[self redrawWithEvent:CSOpenGLViewRedrawEventTimer];
}

- (void)drawRect:(NSRect)dirtyRect {
	[self redrawWithEvent:CSOpenGLViewRedrawEventAppKit];
}

- (void)redrawWithEvent:(CSOpenGLViewRedrawEvent)event  {
	[self performWithGLContext:^{
		[self.delegate openGLViewRedraw:self event:event];
	} flushDrawable:YES];
}

- (void)invalidate {
	const double duration = CVDisplayLinkGetActualOutputVideoRefreshPeriod(_displayLink);
	CVDisplayLinkStop(_displayLink);

	// This is a workaround; I could find no way to ensure that a callback from the display
	// link is not currently ongoing. In short: call stop, wait for an entire refresh period,
	// then assume (/hope) the coast is clear.
	usleep((useconds_t)ceil(duration * 1000000.0));
}

- (void)dealloc {
	// Stop and release the display link
	CVDisplayLinkStop(_displayLink);
	CVDisplayLinkRelease(_displayLink);
}

- (CGSize)backingSize {
	@synchronized(self) {
		return _backingSize;
	}
}

- (void)reshape {
	[super reshape];
	@synchronized(self) {
		_backingSize = [self convertSizeToBacking:self.bounds.size];
	}

	[self performWithGLContext:^{
		CGSize viewSize = [self backingSize];
		glViewport(0, 0, (GLsizei)viewSize.width, (GLsizei)viewSize.height);
	} flushDrawable:NO];
}

- (void)awakeFromNib {
	NSOpenGLPixelFormatAttribute attributes[] = {
		NSOpenGLPFADoubleBuffer,
		NSOpenGLPFAOpenGLProfile,	NSOpenGLProfileVersion3_2Core,
//		NSOpenGLPFAMultisample,
//		NSOpenGLPFASampleBuffers,	1,
//		NSOpenGLPFASamples,			2,
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

- (void)performWithGLContext:(dispatch_block_t)action flushDrawable:(BOOL)flushDrawable {
	CGLLockContext([[self openGLContext] CGLContextObj]);
	[self.openGLContext makeCurrentContext];
	action();
	CGLUnlockContext([[self openGLContext] CGLContextObj]);

	if(flushDrawable) CGLFlushDrawable([[self openGLContext] CGLContextObj]);
}

- (void)performWithGLContext:(nonnull dispatch_block_t)action {
	[self performWithGLContext:action flushDrawable:NO];
}

#pragma mark - NSResponder

- (BOOL)acceptsFirstResponder {
	return YES;
}

- (void)keyDown:(NSEvent *)event {
	[self.responderDelegate keyDown:event];
}

- (void)keyUp:(NSEvent *)event {
	[self.responderDelegate keyUp:event];
}

- (void)flagsChanged:(NSEvent *)event {
	// Release the mouse upon a control + command.
	if(_mouseIsCaptured &&
		event.modifierFlags & NSEventModifierFlagControl &&
		event.modifierFlags & NSEventModifierFlagCommand) {
		[self releaseMouse];
	}

	[self.responderDelegate flagsChanged:event];
}

- (BOOL)application:(nonnull CSApplication *)application shouldSendEvent:(nonnull NSEvent *)event {
	switch(event.type) {
		default: return YES;
		case NSEventTypeKeyUp:			[self keyUp:event];		return NO;
		case NSEventTypeKeyDown:		[self keyDown:event];	return NO;
		case NSEventTypeFlagsChanged:	[self flagsChanged:event];	return NO;
	}
}

- (void)paste:(id)sender {
	[self.responderDelegate paste:sender];
}

#pragma mark - NSDraggingDestination

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender {
	for(NSPasteboardItem *item in [[sender draggingPasteboard] pasteboardItems]) {
		NSURL *URL = [NSURL URLWithString:[item stringForType:(__bridge NSString *)kUTTypeFileURL]];
		[self.delegate openGLView:self didReceiveFileAtURL:URL];
	}
	return YES;
}

- (NSDragOperation)draggingEntered:(id < NSDraggingInfo >)sender {
	return NSDragOperationLink;
}

#pragma mark - Mouse hiding

- (void)setShouldCaptureMouse:(BOOL)shouldCaptureMouse {
	_shouldCaptureMouse = shouldCaptureMouse;
}

- (void)updateTrackingAreas {
	[super updateTrackingAreas];

	if(_mouseTrackingArea) {
		[self removeTrackingArea:_mouseTrackingArea];
	}
	_mouseTrackingArea =
		[[NSTrackingArea alloc]
			initWithRect:self.bounds
			options:NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved | NSTrackingActiveWhenFirstResponder
			owner:self
			userInfo:nil];
	[self addTrackingArea:_mouseTrackingArea];
}

- (void)scheduleMouseHide {
	if(!self.shouldCaptureMouse) {
		[_mouseHideTimer invalidate];

		_mouseHideTimer = [NSTimer scheduledTimerWithTimeInterval:3.0 repeats:NO block:^(NSTimer * _Nonnull timer) {
			[NSCursor setHiddenUntilMouseMoves:YES];
		}];
	}
}

- (void)mouseEntered:(NSEvent *)event {
	[super mouseEntered:event];
	[self scheduleMouseHide];
}

- (void)mouseExited:(NSEvent *)event {
	[super mouseExited:event];
	[_mouseHideTimer invalidate];
	_mouseHideTimer = nil;
}

- (void)releaseMouse {
	if(_mouseIsCaptured) {
		_mouseIsCaptured = NO;
		CGAssociateMouseAndMouseCursorPosition(true);
		[NSCursor unhide];
		[self.delegate openGLViewDidReleaseMouse:self];
		((CSApplication *)[NSApplication sharedApplication]).eventDelegate = nil;
	}
}

#pragma mark - Mouse motion

- (void)applyMouseMotion:(NSEvent *)event {
	if(!self.shouldCaptureMouse) {
		// Mouse capture is off, so don't play games with the cursor, just schedule it to
		// hide in the near future.
		[self scheduleMouseHide];
	} else {
		if(_mouseIsCaptured) {
			// Mouse capture is on, so move the cursor back to the middle of the window, and
			// forward the deltas to the listener.
			//
			// TODO: should I really need to invert the y coordinate myself? It suggests I
			// might have an error in mapping here.
			const NSPoint windowCentre = [self convertPoint:CGPointMake(self.bounds.size.width * 0.5, self.bounds.size.height * 0.5) toView:nil];
			const NSPoint screenCentre = [self.window convertPointToScreen:windowCentre];
			const CGRect screenFrame = self.window.screen.frame;
			CGWarpMouseCursorPosition(NSMakePoint(
				screenFrame.origin.x + screenCentre.x,
				screenFrame.origin.y + screenFrame.size.height - screenCentre.y
			));

			[self.responderDelegate mouseMoved:event];
		}
	}
}

- (void)mouseDragged:(NSEvent *)event {
	[self applyMouseMotion:event];
	[super mouseDragged:event];
}

- (void)rightMouseDragged:(NSEvent *)event {
	[self applyMouseMotion:event];
	[super rightMouseDragged:event];
}

- (void)otherMouseDragged:(NSEvent *)event {
	[self applyMouseMotion:event];
	[super otherMouseDragged:event];
}

- (void)mouseMoved:(NSEvent *)event {
	[self applyMouseMotion:event];
	[super mouseMoved:event];
}

#pragma mark - Mouse buttons

- (void)applyButtonDown:(NSEvent *)event {
	if(self.shouldCaptureMouse) {
		if(!_mouseIsCaptured) {
			_mouseIsCaptured = YES;
			[NSCursor hide];
			CGAssociateMouseAndMouseCursorPosition(false);
			[self.delegate openGLViewDidCaptureMouse:self];
			if(self.shouldUsurpCommand) {
				((CSApplication *)[NSApplication sharedApplication]).eventDelegate = self;
			}

			// Don't report the first click to the delegate; treat that as merely
			// an invitation to capture the cursor.
			return;
		}

		[self.responderDelegate mouseDown:event];
	}
}

- (void)applyButtonUp:(NSEvent *)event {
	if(self.shouldCaptureMouse) {
		[self.responderDelegate mouseUp:event];
	}
}

- (void)mouseDown:(NSEvent *)event {
	[self applyButtonDown:event];
	[super mouseDown:event];
}

- (void)rightMouseDown:(NSEvent *)event {
	[self applyButtonDown:event];
	[super rightMouseDown:event];
}

- (void)otherMouseDown:(NSEvent *)event {
	[self applyButtonDown:event];
	[super otherMouseDown:event];
}

- (void)mouseUp:(NSEvent *)event {
	[self applyButtonUp:event];
	[super mouseUp:event];
}

- (void)rightMouseUp:(NSEvent *)event  {
	[self applyButtonUp:event];
	[super rightMouseUp:event];
}

- (void)otherMouseUp:(NSEvent *)event {
	[self applyButtonUp:event];
	[super otherMouseUp:event];
}

@end
