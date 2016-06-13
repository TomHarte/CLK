//
//  CSMachine.h
//  Clock Signal
//
//  Created by Thomas Harte on 04/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "CSOpenGLView.h"
#import "AudioQueue.h"

@interface CSMachine : NSObject

- (void)runForNumberOfCycles:(int)numberOfCycles;

- (float)idealSamplingRateFromRange:(NSRange)range;
- (void)setAudioSamplingRate:(float)samplingRate;

- (void)setView:(CSOpenGLView *)view aspectRatio:(float)aspectRatio;
- (void)drawViewForPixelSize:(CGSize)pixelSize onlyIfDirty:(BOOL)onlyIfDirty;

@property (nonatomic, weak) AudioQueue *audioQueue;
@property (nonatomic, readonly) CSOpenGLView *view;

@end
