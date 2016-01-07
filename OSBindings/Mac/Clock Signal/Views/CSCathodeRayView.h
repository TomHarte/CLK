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
- (void)openGLView:(nonnull CSCathodeRayView *)view didUpdateToTime:(CVTimeStamp)time;
@end

@protocol CSCathodeRayViewResponderDelegate <NSObject>
- (void)keyDown:(nonnull NSEvent *)event;
- (void)keyUp:(nonnull NSEvent *)event;
- (void)flagsChanged:(nonnull NSEvent *)newModifiers;
@end

typedef NS_ENUM(NSInteger, CSCathodeRayViewSignalType) {
	CSCathodeRayViewSignalTypeNTSC,
	CSCathodeRayViewSignalTypeRGB
};

@interface CSCathodeRayView : NSOpenGLView

@property (nonatomic, weak) id <CSCathodeRayViewDelegate> delegate;
@property (nonatomic, weak) id <CSCathodeRayViewResponderDelegate> responderDelegate;

- (void)invalidate;

- (BOOL)pushFrame:(nonnull CRTFrame *)crtFrame;
- (void)setSignalDecoder:(nonnull NSString *)decoder type:(CSCathodeRayViewSignalType)type;

// these are relative to a [0, 1] range in both width and height;
// default is .origin = (0, 0), .size = (1, 1)
@property (nonatomic, assign) CGRect frameBounds;

@end
