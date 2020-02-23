//
//  TestMachineZ80.h
//  Clock Signal
//
//  Created by Thomas Harte on 16/05/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <stdint.h>
#import "TestMachine.h"

@class CSTestMachineZ80;

typedef NS_ENUM(NSInteger, CSTestMachineZ80BusOperationCaptureOperation) {
	CSTestMachineZ80BusOperationCaptureOperationReadOpcode,
	CSTestMachineZ80BusOperationCaptureOperationRead,
	CSTestMachineZ80BusOperationCaptureOperationWrite,
	CSTestMachineZ80BusOperationCaptureOperationPortRead,
	CSTestMachineZ80BusOperationCaptureOperationPortWrite,
	CSTestMachineZ80BusOperationCaptureOperationInternalOperation,
};

@interface CSTestMachineZ80BusOperationCapture: NSObject
@property(nonatomic, readonly) CSTestMachineZ80BusOperationCaptureOperation operation;
@property(nonatomic, readonly) uint16_t address;
@property(nonatomic, readonly) uint8_t value;
@property(nonatomic, readonly) int timeStamp;
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
	CSTestMachineZ80RegisterIFF1,	CSTestMachineZ80RegisterIFF2,	CSTestMachineZ80RegisterIM,
	CSTestMachineZ80RegisterMemPtr
};

typedef NS_ENUM(NSInteger, CSTestMachinePortLogic) {
	CSTestMachinePortLogicReturnUpperByte,
	CSTestMachinePortLogicReturn191
};

@interface CSTestMachineZ80 : CSTestMachine

- (void)setData:(nonnull NSData *)data atAddress:(uint16_t)startAddress;
- (void)setValue:(uint8_t)value atAddress:(uint16_t)address;
- (uint8_t)valueAtAddress:(uint16_t)address;

- (void)runForNumberOfCycles:(int)cycles;

- (void)setValue:(uint16_t)value forRegister:(CSTestMachineZ80Register)reg;
- (uint16_t)valueForRegister:(CSTestMachineZ80Register)reg;

@property(nonatomic, assign) BOOL captureBusActivity;
@property(nonatomic, readonly, nonnull) NSArray<CSTestMachineZ80BusOperationCapture *> *busOperationCaptures;

@property(nonatomic, readonly) BOOL isHalted;
@property(nonatomic, readonly) int completedHalfCycles;

@property(nonatomic) BOOL nmiLine;
@property(nonatomic) BOOL irqLine;
@property(nonatomic) BOOL waitLine;

@property(nonatomic) CSTestMachinePortLogic portLogic;

@end
