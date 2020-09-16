//
//  CSMachine.h
//  Clock Signal
//
//  Created by Thomas Harte on 04/01/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>

#import "CSAudioQueue.h"
#import "CSJoystickManager.h"
#import "CSScanTargetView.h"
#import "CSStaticAnalyser.h"

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
	CSMachineKeyboardInputModeKeyboardPhysical,
	CSMachineKeyboardInputModeKeyboardLogical,
	CSMachineKeyboardInputModeJoystick,
};

@interface CSMissingROM: NSObject
@property (nonatomic, readonly, nonnull) NSString *machineName;
@property (nonatomic, readonly, nonnull) NSString *fileName;
@property (nonatomic, readonly, nullable) NSString *descriptiveName;
@property (nonatomic, readonly) NSUInteger size;
@property (nonatomic, readonly, nonnull) NSArray<NSNumber *> *crc32s;
@end

// Deliberately low; to ensure CSMachine has been declared as an @class already.
#import "CSAtari2600.h"
#import "CSZX8081.h"

@interface CSMachine : NSObject

- (nonnull instancetype)init NS_UNAVAILABLE;

/*!
	Initialises an instance of CSMachine.

	@param result The CSStaticAnalyser result that describes the machine needed.
	@param missingROMs An array that is filled with a list of ROMs that the machine requested but which
		were not found; populated only if this `init` has failed.
*/
- (nullable instancetype)initWithAnalyser:(nonnull CSStaticAnalyser *)result missingROMs:(nullable inout NSMutableArray<CSMissingROM *> *)missingROMs NS_DESIGNATED_INITIALIZER;

- (float)idealSamplingRateFromRange:(NSRange)range;
- (BOOL)isStereo;
- (void)setAudioSamplingRate:(float)samplingRate bufferSize:(NSUInteger)bufferSize stereo:(BOOL)stereo;

- (void)setView:(nullable CSScanTargetView *)view aspectRatio:(float)aspectRatio;

- (void)start;
- (void)stop;

- (void)setKey:(uint16_t)key characters:(nullable NSString *)characters isPressed:(BOOL)isPressed;
- (void)clearAllKeys;

- (void)setMouseButton:(int)button isPressed:(BOOL)isPressed;
- (void)addMouseMotionX:(CGFloat)deltaX y:(CGFloat)deltaY;

@property (atomic, strong, nullable) CSAudioQueue *audioQueue;
@property (nonatomic, readonly, nonnull) CSScanTargetView *view;
@property (nonatomic, weak, nullable) id<CSMachineDelegate> delegate;

@property (nonatomic, readonly, nonnull) NSString *userDefaultsPrefix;

- (void)paste:(nonnull NSString *)string;
@property (nonatomic, readonly, nonnull) NSBitmapImageRep *imageRepresentation;

@property (nonatomic, assign) BOOL useFastLoadingHack;
@property (nonatomic, assign) CSMachineVideoSignal videoSignal;
@property (nonatomic, assign) BOOL useAutomaticTapeMotorControl;
@property (nonatomic, assign) BOOL useQuickBootingHack;

@property (nonatomic, readonly) BOOL canInsertMedia;

- (BOOL)supportsVideoSignal:(CSMachineVideoSignal)videoSignal;

// Volume contorl.
- (void)setVolume:(float)volume;
@property (nonatomic, readonly) BOOL hasAudioOutput;

// Input control.
@property (nonatomic, readonly) BOOL hasExclusiveKeyboard;
@property (nonatomic, readonly) BOOL shouldUsurpCommand;
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
