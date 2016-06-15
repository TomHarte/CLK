//
//  AudioQueue.h
//  Clock Signal
//
//  Created by Thomas Harte on 14/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>

@class AudioQueue;

@protocol AudioQueueDelegate
- (void)audioQueueDidCompleteBuffer:(nonnull AudioQueue *)audioQueue;
@end

@interface AudioQueue : NSObject

- (nonnull instancetype)initWithSamplingRate:(Float64)samplingRate;
- (void)enqueueAudioBuffer:(nonnull const int16_t *)buffer numberOfSamples:(size_t)lengthInSamples;

@property (nonatomic, readonly) Float64 samplingRate;
@property (nonatomic, weak) id<AudioQueueDelegate> delegate;

+ (Float64)preferredSamplingRate;

@end
