//
//  MOS6522Bridge.h
//  Clock Signal
//
//  Created by Thomas Harte on 18/06/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>

typedef NS_ENUM(NSInteger, MOS6522BridgePort) {
	MOS6522BridgePortA = 0, MOS6522BridgePortB = 1
};

typedef NS_ENUM(NSInteger, MOS6522BridgeLine) {
	MOS6522BridgeLineOne = 0, MOS6522BridgeLineTwo = 1
};

@interface MOS6522Bridge : NSObject

@property (nonatomic, readonly) BOOL irqLine;
@property (nonatomic) uint8_t portBInput;
@property (nonatomic) uint8_t portAInput;

- (void)setValue:(uint8_t)value forRegister:(NSUInteger)registerNumber;
- (uint8_t)valueForRegister:(NSUInteger)registerNumber;
- (BOOL)valueForControlLine:(MOS6522BridgeLine)line port:(MOS6522BridgePort)port;

- (void)runForHalfCycles:(NSUInteger)numberOfHalfCycles;

@end
