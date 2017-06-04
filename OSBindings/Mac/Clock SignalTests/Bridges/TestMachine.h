//
//  TestMachine.h
//  Clock Signal
//
//  Created by Thomas Harte on 03/06/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>

@class CSTestMachine;

@protocol CSTestMachineTrapHandler
- (void)testMachine:(nonnull CSTestMachine *)testMachine didTrapAtAddress:(uint16_t)address;
@end

@interface CSTestMachine : NSObject

@property(nonatomic, weak, nullable) id<CSTestMachineTrapHandler> trapHandler;
- (void)addTrapAddress:(uint16_t)trapAddress;

@end
