//
//  CSMachine.h
//  Clock Signal
//
//  Created by Thomas Harte on 04/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "CSOpenGLView.h"
#import "CSAudioQueue.h"

@class CSMachine;
@protocol CSMachineDelegate
- (void)machineDidChangeClockRate:(CSMachine *)machine;
@end

@interface CSMachine : NSObject

- (void)runForNumberOfCycles:(int)numberOfCycles;

- (float)idealSamplingRateFromRange:(NSRange)range;
- (void)setAudioSamplingRate:(float)samplingRate;

- (void)setView:(CSOpenGLView *)view aspectRatio:(float)aspectRatio;
- (void)drawViewForPixelSize:(CGSize)pixelSize onlyIfDirty:(BOOL)onlyIfDirty;

@property (nonatomic, strong) CSAudioQueue *audioQueue;
@property (nonatomic, readonly) CSOpenGLView *view;
@property (nonatomic, weak) id<CSMachineDelegate> delegate;
@property (nonatomic, readonly) double clockRate;

@end
