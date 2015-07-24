//
//  OpenGLView.h
//  Clock Signal
//
//  Created by Thomas Harte on 16/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>
@import AppKit;

@class CSOpenGLView;

@protocol CSOpenGLViewDelegate
- (void)openGLView:(CSOpenGLView *)view didUpdateToTime:(CVTimeStamp)time;
- (void)openGLViewDrawView:(CSOpenGLView *)view;
@end

@interface CSOpenGLView : NSOpenGLView

@property (nonatomic, weak) id <CSOpenGLViewDelegate> delegate;

@end
