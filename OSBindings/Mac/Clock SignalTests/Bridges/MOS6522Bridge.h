//
//  MOS6522Bridge.h
//  Clock Signal
//
//  Created by Thomas Harte on 18/06/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface MOS6522Bridge : NSObject

@property (nonatomic, readonly) BOOL irqLine;
@property (nonatomic) uint8_t portBInput;
@property (nonatomic) uint8_t portAInput;

- (void)setValue:(uint8_t)value forRegister:(NSUInteger)registerNumber;
- (uint8_t)valueForRegister:(NSUInteger)registerNumber;

- (void)runForHalfCycles:(NSUInteger)numberOfHalfCycles;

@end
