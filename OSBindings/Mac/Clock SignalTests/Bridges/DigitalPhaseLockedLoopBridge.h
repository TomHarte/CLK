//
//  DigitalPhaseLockedLoopBridge.h
//  Clock Signal
//
//  Created by Thomas Harte on 12/07/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface DigitalPhaseLockedLoopBridge : NSObject

- (instancetype)initWithClocksPerBit:(NSUInteger)clocksPerBit;

- (void)runForCycles:(NSUInteger)cycles;
- (void)addPulse;

@property(nonatomic) NSUInteger stream;

@end
