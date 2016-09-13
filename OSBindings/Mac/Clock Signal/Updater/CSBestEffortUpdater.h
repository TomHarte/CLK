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

- (void)bestEffortUpdater:(CSBestEffortUpdater *)bestEffortUpdater runForCycles:(NSUInteger)cycles didSkipPreviousUpdate:(BOOL)didSkipPreviousUpdate;

@end


@interface CSBestEffortUpdater : NSObject

@property (nonatomic, assign) double clockRate;
@property (nonatomic, assign) BOOL runAsUnlimited;
@property (nonatomic, weak) id<CSBestEffortUpdaterDelegate> delegate;

- (void)update;

@end
