//
//  OpenGLView.h
//  Clock Signal
//
//  Created by Thomas Harte on 16/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>

@class CSCathodeRayView;

@protocol CSCathodeRayViewDelegate
- (void)openGLView:(nonnull CSCathodeRayView *)view didUpdateToTime:(CVTimeStamp)time;
- (void)openGLViewDrawView:(nonnull CSCathodeRayView *)view;
@end

@protocol CSCathodeRayViewResponderDelegate <NSObject>
- (void)keyDown:(nonnull NSEvent *)event;
- (void)keyUp:(nonnull NSEvent *)event;
- (void)flagsChanged:(nonnull NSEvent *)newModifiers;
@end


@interface CSCathodeRayView : NSOpenGLView

@property (nonatomic, weak) id <CSCathodeRayViewDelegate> delegate;
@property (nonatomic, weak) id <CSCathodeRayViewResponderDelegate> responderDelegate;

- (void)invalidate;

@property (nonatomic, readonly) CGSize backingSize;

@end
