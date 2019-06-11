//
//  CSMachine.h
//  Clock Signal
//
//  Created by Thomas Harte on 04/01/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>

#import "CSAudioQueue.h"
#import "CSFastLoading.h"
#import "CSOpenGLView.h"
#import "CSStaticAnalyser.h"
#import "CSJoystickManager.h"

@class CSMachine;
@protocol CSMachineDelegate
- (void)machineSpeakerDidChangeInputClock:(nonnull CSMachine *)machine;
- (void)machine:(nonnull CSMachine *)machine led:(nonnull NSString *)led didChangeToLit:(BOOL)isLit;
- (void)machine:(nonnull CSMachine *)machine ledShouldBlink:(nonnull NSString *)led;
@end

typedef NS_ENUM(NSInteger, CSMachineVideoSignal) {
	CSMachineVideoSignalComposite,
	CSMachineVideoSignalSVideo,
	CSMachineVideoSignalRGB,
	CSMachineVideoSignalMonochromeComposite
};

typedef NS_ENUM(NSInteger, CSMachineKeyboardInputMode) {
	CSMachineKeyboardInputModeKeyboard,
	CSMachineKeyboardInputModeJoystick
};

// Deliberately low; to ensure CSMachine has been declared as an @class already.
#import "CSAtari2600.h"
#import "CSZX8081.h"

@interface CSMachine : NSObject

- (nonnull instancetype)init NS_UNAVAILABLE;

/*!
	Initialises an instance of CSMachine.

	@param result The CSStaticAnalyser result that describes the machine needed.
*/
- (nullable instancetype)initWithAnalyser:(nonnull CSStaticAnalyser *)result NS_DESIGNATED_INITIALIZER;

- (void)runForInterval:(NSTimeInterval)interval;

- (float)idealSamplingRateFromRange:(NSRange)range;
- (void)setAudioSamplingRate:(float)samplingRate bufferSize:(NSUInteger)bufferSize;

- (void)setView:(nullable CSOpenGLView *)view aspectRatio:(float)aspectRatio;

- (void)updateViewForPixelSize:(CGSize)pixelSize;
- (void)drawViewForPixelSize:(CGSize)pixelSize;

- (void)setKey:(uint16_t)key characters:(nullable NSString *)characters isPressed:(BOOL)isPressed;
- (void)clearAllKeys;

- (void)setMouseButton:(int)button isPressed:(BOOL)isPressed;
- (void)addMouseMotionX:(CGFloat)deltaX y:(CGFloat)deltaY;

@property (nonatomic, strong, nullable) CSAudioQueue *audioQueue;
@property (nonatomic, readonly, nonnull) CSOpenGLView *view;
@property (nonatomic, weak, nullable) id<CSMachineDelegate> delegate;

@property (nonatomic, readonly, nonnull) NSString *userDefaultsPrefix;

- (void)paste:(nonnull NSString *)string;
@property (nonatomic, readonly, nonnull) NSBitmapImageRep *imageRepresentation;

@property (nonatomic, assign) BOOL useFastLoadingHack;
@property (nonatomic, assign) CSMachineVideoSignal videoSignal;
@property (nonatomic, assign) BOOL useAutomaticTapeMotorControl;

@property (nonatomic, readonly) BOOL canInsertMedia;

- (bool)supportsVideoSignal:(CSMachineVideoSignal)videoSignal;

// Input control.
@property (nonatomic, readonly) BOOL hasExclusiveKeyboard;
@property (nonatomic, readonly) BOOL hasJoystick;
@property (nonatomic, readonly) BOOL hasMouse;
@property (nonatomic, assign) CSMachineKeyboardInputMode inputMode;
@property (nonatomic, nullable) CSJoystickManager *joystickManager;

// LED list.
@property (nonatomic, readonly, nonnull) NSArray<NSString *> *leds;

// Special-case accessors; undefined behaviour if accessed for a machine not of the corresponding type.
@property (nonatomic, readonly, nullable) CSAtari2600 *atari2600;
@property (nonatomic, readonly, nullable) CSZX8081 *zx8081;

@end
