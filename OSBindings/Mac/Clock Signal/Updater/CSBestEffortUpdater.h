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

// The following is coupled to the definitions in CRTMachine.hpp, but exposed here
// for the benefit of Swift.
typedef NS_ENUM(NSInteger, CSBestEffortUpdaterEvent) {
	CSBestEffortUpdaterEventAudioNeeded = 1 << 0
};

@interface CSBestEffortUpdater : NSObject

- (void)update;
- (void)updateWithEvent:(CSBestEffortUpdaterEvent)event;
- (void)flush;
- (void)setMachine:(CSMachine *)machine;

@end
