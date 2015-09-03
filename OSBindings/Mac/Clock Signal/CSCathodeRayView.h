//
//  OpenGLView.h
//  Clock Signal
//
//  Created by Thomas Harte on 16/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "CRTFrame.h"
#import <AppKit/AppKit.h>

@class CSCathodeRayView;

@protocol CSCathodeRayViewDelegate
- (void)openGLView:(CSCathodeRayView * __nonnull)view didUpdateToTime:(CVTimeStamp)time;
@end

@protocol CSCathodeRayViewResponderDelegate <NSObject>
- (void)keyDown:(NSEvent * __nonnull)event;
- (void)keyUp:(NSEvent * __nonnull)event;
- (void)flagsChanged:(NSEvent * __nonnull)newModifiers;
@end

@interface CSCathodeRayView : NSOpenGLView

@property (nonatomic, weak) id <CSCathodeRayViewDelegate> delegate;
@property (nonatomic, weak) id <CSCathodeRayViewResponderDelegate> responderDelegate;

- (void)invalidate;

- (BOOL)pushFrame:(CRTFrame * __nonnull)crtFrame;
- (void)setSignalDecoder:(NSString * __nonnull)signalDecoder;

@end
