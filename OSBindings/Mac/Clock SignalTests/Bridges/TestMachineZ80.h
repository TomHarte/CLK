//
//  TestMachineZ80.h
//  Clock Signal
//
//  Created by Thomas Harte on 16/05/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>

typedef NS_ENUM(NSInteger, CSTestMachineZ80Register) {
	CSTestMachineZ80RegisterProgramCounter,
	CSTestMachineZ80RegisterStackPointer,
};

@class CSTestMachineZ80;

@interface CSTestMachineTrapHandler
- (void)testMachine:(CSTestMachineZ80 *)testMachine didTrapAtAddress:(uint16_t)address;
@end

@interface CSTestMachineZ80 : NSObject

- (void)setData:(NSData *)data atAddress:(uint16_t)startAddress;
- (void)runForNumberOfCycles:(int)cycles;

- (void)setValue:(uint16_t)value forRegister:(CSTestMachineZ80Register)reg;
- (uint16_t)valueForRegister:(CSTestMachineZ80Register)reg;

@property(nonatomic, weak) id<CSTestMachineTrapHandler> trapHandler;
- (void)addTrapAddress:(uint16_t)trapAddress;

@end
