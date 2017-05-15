//
//  TestMachine6502.h
//  CLK
//
//  Created by Thomas Harte on 29/06/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>

typedef NS_ENUM(NSInteger, CSTestMachine6502Register) {
	CSTestMachine6502RegisterLastOperationAddress,
	CSTestMachine6502RegisterProgramCounter,
	CSTestMachine6502RegisterStackPointer,
	CSTestMachine6502RegisterFlags,
	CSTestMachine6502RegisterA,
	CSTestMachine6502RegisterX,
	CSTestMachine6502RegisterY,
};

extern const uint8_t CSTestMachine6502JamOpcode;

@class CSTestMachine6502;
@protocol CSTestMachine6502JamHandler <NSObject>
- (void)testMachine:(CSTestMachine6502 *)machine didJamAtAddress:(uint16_t)address;
@end

@interface CSTestMachine6502 : NSObject

- (void)setData:(NSData *)data atAddress:(uint16_t)startAddress;
- (void)runForNumberOfCycles:(int)cycles;

- (void)setValue:(uint8_t)value forAddress:(uint16_t)address;
- (uint8_t)valueForAddress:(uint16_t)address;
- (void)setValue:(uint16_t)value forRegister:(CSTestMachine6502Register)reg;
- (uint16_t)valueForRegister:(CSTestMachine6502Register)reg;

- (void)returnFromSubroutine;

@property (nonatomic, readonly) BOOL isJammed;
@property (nonatomic, readonly) uint32_t timestamp;
@property (nonatomic, weak) id <CSTestMachine6502JamHandler> jamHandler;
@property (nonatomic, assign) BOOL irqLine;
@property (nonatomic, assign) BOOL nmiLine;

@end
