//
//  CSMachine.h
//  Clock Signal
//
//  Created by Thomas Harte on 04/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "CSCathodeRayView.h"
#import "AudioQueue.h"

@interface CSMachine : NSObject

- (void)runForNumberOfCycles:(int)numberOfCycles;
- (void)sync;

@property (nonatomic, weak) CSCathodeRayView *view;
@property (nonatomic, weak) AudioQueue *audioQueue;

@end
