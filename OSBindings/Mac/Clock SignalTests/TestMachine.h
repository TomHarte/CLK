//
//  Machine.h
//  CLK
//
//  Created by Thomas Harte on 29/06/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>

typedef NS_ENUM(NSInteger, CSTestMachineRegister) {
	CSTestMachineRegisterLastOperationAddress,
	CSTestMachineRegisterProgramCounter,
	CSTestMachineRegisterStackPointer,
	CSTestMachineRegisterFlags,
	CSTestMachineRegisterA
};

extern const uint8_t CSTestMachineJamOpcode;

@class CSTestMachine;
@protocol CSTestMachineJamHandler <NSObject>
- (void)testMachine:(CSTestMachine *)machine didJamAtAddress:(uint16_t)address;
@end

@interface CSTestMachine : NSObject

- (void)setData:(NSData *)data atAddress:(uint16_t)startAddress;
- (void)runForNumberOfCycles:(int)cycles;

- (void)setValue:(uint8_t)value forAddress:(uint16_t)address;
- (uint8_t)valueForAddress:(uint16_t)address;
- (void)setValue:(uint16_t)value forRegister:(CSTestMachineRegister)reg;
- (uint16_t)valueForRegister:(CSTestMachineRegister)reg;

- (void)reset;
- (void)returnFromSubroutine;

@property (nonatomic, readonly) BOOL isJammed;
@property (nonatomic, weak) id <CSTestMachineJamHandler> jamHandler;

@end
