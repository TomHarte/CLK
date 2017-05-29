//
//  TestMachineZ80.h
//  Clock Signal
//
//  Created by Thomas Harte on 16/05/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <stdint.h>

@class CSTestMachineZ80;

@protocol CSTestMachineTrapHandler
- (void)testMachine:(CSTestMachineZ80 *)testMachine didTrapAtAddress:(uint16_t)address;
@end

typedef NS_ENUM(NSInteger, CSTestMachineZ80BusOperationCaptureOperation) {
	CSTestMachineZ80BusOperationCaptureOperationRead,
	CSTestMachineZ80BusOperationCaptureOperationWrite
};

@interface CSTestMachineZ80BusOperationCapture: NSObject
@property(nonatomic, assign) CSTestMachineZ80BusOperationCaptureOperation operation;
@property(nonatomic, assign) uint16_t address;
@property(nonatomic, assign) uint8_t value;
@property(nonatomic, assign) int timeStamp;
@end

typedef NS_ENUM(NSInteger, CSTestMachineZ80Register) {
	CSTestMachineZ80RegisterProgramCounter,
	CSTestMachineZ80RegisterStackPointer,

	CSTestMachineZ80RegisterA,		CSTestMachineZ80RegisterF,		CSTestMachineZ80RegisterAF,
	CSTestMachineZ80RegisterB,		CSTestMachineZ80RegisterC,		CSTestMachineZ80RegisterBC,
	CSTestMachineZ80RegisterD,		CSTestMachineZ80RegisterE,		CSTestMachineZ80RegisterDE,
	CSTestMachineZ80RegisterH,		CSTestMachineZ80RegisterL,		CSTestMachineZ80RegisterHL,
	CSTestMachineZ80RegisterAFDash,
	CSTestMachineZ80RegisterBCDash,
	CSTestMachineZ80RegisterDEDash,
	CSTestMachineZ80RegisterHLDash,
	CSTestMachineZ80RegisterIX,		CSTestMachineZ80RegisterIY,
	CSTestMachineZ80RegisterI,		CSTestMachineZ80RegisterR,
	CSTestMachineZ80RegisterIFF1,	CSTestMachineZ80RegisterIFF2,	CSTestMachineZ80RegisterIM
};

@interface CSTestMachineZ80 : NSObject

- (void)setData:(NSData *)data atAddress:(uint16_t)startAddress;
- (void)setValue:(uint8_t)value atAddress:(uint16_t)address;
- (uint8_t)valueAtAddress:(uint16_t)address;

- (void)runForNumberOfCycles:(int)cycles;
- (void)runToNextInstruction;

- (void)setValue:(uint16_t)value forRegister:(CSTestMachineZ80Register)reg;
- (uint16_t)valueForRegister:(CSTestMachineZ80Register)reg;

@property(nonatomic, weak) id<CSTestMachineTrapHandler> trapHandler;
- (void)addTrapAddress:(uint16_t)trapAddress;

@property(nonatomic, assign) BOOL captureBusActivity;
@property(nonatomic, readonly) NSArray<CSTestMachineZ80BusOperationCapture *> *busOperationCaptures;

@property(nonatomic, readonly) BOOL isHalted;

@end
