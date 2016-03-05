//
//  CSOpenGLView.h
//  Clock Signal
//
//  Created by Thomas Harte on 16/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>

@class CSOpenGLView;

@protocol CSOpenGLViewDelegate
- (void)openGLView:(nonnull CSOpenGLView *)view didUpdateToTime:(CVTimeStamp)time;
- (void)openGLView:(nonnull CSOpenGLView *)view drawViewOnlyIfDirty:(BOOL)onlyIfDirty;
@end

@protocol CSOpenGLViewResponderDelegate <NSObject>
- (void)keyDown:(nonnull NSEvent *)event;
- (void)keyUp:(nonnull NSEvent *)event;
- (void)flagsChanged:(nonnull NSEvent *)newModifiers;
@end

/*!
	Provides an OpenGL canvas with a refresh-linked update timer and manages a serial dispatch queue
	such that a delegate may produce video and respond to keyboard events.
*/
@interface CSOpenGLView : NSOpenGLView

@property (nonatomic, weak) id <CSOpenGLViewDelegate> delegate;
@property (nonatomic, weak) id <CSOpenGLViewResponderDelegate> responderDelegate;

- (void)invalidate;

@property (nonatomic, readonly) CGSize backingSize;

@end
