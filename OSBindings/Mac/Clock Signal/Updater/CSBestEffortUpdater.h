//
//  CSBestEffortUpdater.h
//  Clock Signal
//
//  Created by Thomas Harte on 16/06/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <CoreVideo/CoreVideo.h>

#import "CSMachine.h"

@interface CSBestEffortUpdater : NSObject

- (void)update;
- (void)flush;
- (void)setMachine:(CSMachine *)machine;

@end
