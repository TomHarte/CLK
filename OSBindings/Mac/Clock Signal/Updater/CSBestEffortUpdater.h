//
//  CSBestEffortUpdater.h
//  Clock Signal
//
//  Created by Thomas Harte on 16/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <CoreVideo/CoreVideo.h>

@class CSBestEffortUpdater;

@protocol CSBestEffortUpdaterDelegate <NSObject>

- (void)bestEffortUpdater:(CSBestEffortUpdater *)bestEffortUpdater runForInterval:(NSTimeInterval)interval didSkipPreviousUpdate:(BOOL)didSkipPreviousUpdate;

@end


@interface CSBestEffortUpdater : NSObject

@property (nonatomic, weak) id<CSBestEffortUpdaterDelegate> delegate;

- (void)update;
- (void)flush;

@end
