//
//  CSMachine.h
//  Clock Signal
//
//  Created by Thomas Harte on 04/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>

#import "CSAudioQueue.h"
#import "CSOpenGLView.h"
#import "CSStaticAnalyser.h"

@class CSMachine;
@protocol CSMachineDelegate
- (void)machineDidChangeClockRate:(CSMachine *)machine;
- (void)machineDidChangeClockIsUnlimited:(CSMachine *)machine;
@end

@interface CSMachine : NSObject

- (instancetype)init NS_UNAVAILABLE;
/*!
	Initialises an instance of CSMachine.

	@param result The CSStaticAnalyser result that describes the machine needed.
*/
- (instancetype)initWithAnalyser:(CSStaticAnalyser *)result NS_DESIGNATED_INITIALIZER;

- (void)runForNumberOfCycles:(int)numberOfCycles;

- (float)idealSamplingRateFromRange:(NSRange)range;
- (void)setAudioSamplingRate:(float)samplingRate bufferSize:(NSUInteger)bufferSize;

- (void)setView:(CSOpenGLView *)view aspectRatio:(float)aspectRatio;
- (void)drawViewForPixelSize:(CGSize)pixelSize onlyIfDirty:(BOOL)onlyIfDirty;

- (void)setKey:(uint16_t)key isPressed:(BOOL)isPressed;
- (void)clearAllKeys;

@property (nonatomic, strong) CSAudioQueue *audioQueue;
@property (nonatomic, readonly) CSOpenGLView *view;
@property (nonatomic, weak) id<CSMachineDelegate> delegate;

@property (nonatomic, readonly) double clockRate;
@property (nonatomic, readonly) BOOL clockIsUnlimited;

@property (nonatomic, readonly) NSString *userDefaultsPrefix;

- (void)paste:(NSString *)string;

@property (nonatomic, assign) BOOL useFastLoadingHack;
@property (nonatomic, assign) BOOL useCompositeOutput;
@property (nonatomic, assign) BOOL useAutomaticTapeMotorControl;

@end
