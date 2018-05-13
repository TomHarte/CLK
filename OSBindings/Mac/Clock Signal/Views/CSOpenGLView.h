//
//  CSOpenGLView.h
//  Clock Signal
//
//  Created by Thomas Harte on 16/07/2015.
//  Copyright 2015 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>

@class CSOpenGLView;

@protocol CSOpenGLViewDelegate
/*!
	Requests that the delegate produce an image of its current output state. May be called on
	any queue or thread.
	@param view The view making the request.
	@param onlyIfDirty If @c YES then the delegate may decline to redraw if its output would be
	identical to the previous frame. If @c NO then the delegate must draw.
*/
- (void)openGLView:(nonnull CSOpenGLView *)view drawViewOnlyIfDirty:(BOOL)onlyIfDirty;

/*!
	Announces receipt of a file by drag and drop to the delegate.
	@param view The view making the request.
	@param URL The file URL of the received file.
*/
- (void)openGLView:(nonnull CSOpenGLView *)view didReceiveFileAtURL:(nonnull NSURL *)URL;

@end

@protocol CSOpenGLViewResponderDelegate <NSObject>
/*!
	Supplies a keyDown event to the delegate.
	@param event The @c NSEvent describing the keyDown.
*/
- (void)keyDown:(nonnull NSEvent *)event;

/*!
	Supplies a keyUp event to the delegate.
	@param event The @c NSEvent describing the keyUp.
*/
- (void)keyUp:(nonnull NSEvent *)event;

/*!
	Supplies a flagsChanged event to the delegate.
	@param event The @c NSEvent describing the flagsChanged.
*/
- (void)flagsChanged:(nonnull NSEvent *)event;

/*!
	Supplies a paste event to the delegate.
*/
- (void)paste:(nonnull id)sender;

@end

/*!
	Provides an OpenGL canvas with a refresh-linked update timer that can forward a subset
	of typical first-responder actions.
*/
@interface CSOpenGLView : NSOpenGLView

@property (atomic, weak, nullable) id <CSOpenGLViewDelegate> delegate;
@property (nonatomic, weak, nullable) id <CSOpenGLViewResponderDelegate> responderDelegate;

/*!
	Ends the timer tracking time; should be called prior to giving up the last owning reference
	to ensure that any retain cycles implied by the timer are resolved.
*/
- (void)invalidate;

/// The size in pixels of the OpenGL canvas, factoring in screen pixel density and view size in points.
@property (nonatomic, readonly) CGSize backingSize;

/*!
	Locks this view's OpenGL context and makes it current, performs @c action and then unlocks
	the context. @c action is performed on the calling queue.
*/
- (void)performWithGLContext:(nonnull dispatch_block_t)action;

@end
