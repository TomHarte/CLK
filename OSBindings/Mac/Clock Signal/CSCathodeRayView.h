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

@interface CSCathodeRayView : NSOpenGLView

@property (nonatomic, weak) id <CSCathodeRayViewDelegate> delegate;

- (void)invalidate;

- (BOOL)pushFrame:(CRTFrame * __nonnull)crtFrame;

@end
