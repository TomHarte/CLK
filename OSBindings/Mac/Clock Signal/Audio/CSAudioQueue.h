//
//  AudioQueue.h
//  Clock Signal
//
//  Created by Thomas Harte on 14/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>

@class CSAudioQueue;

@protocol CSAudioQueueDelegate
- (void)audioQueueDidCompleteBuffer:(nonnull CSAudioQueue *)audioQueue;
@end

@interface CSAudioQueue : NSObject

- (nonnull instancetype)initWithSamplingRate:(Float64)samplingRate;
- (void)enqueueAudioBuffer:(nonnull const int16_t *)buffer numberOfSamples:(size_t)lengthInSamples;

@property (nonatomic, readonly) Float64 samplingRate;
@property (nonatomic, weak, nullable) id<CSAudioQueueDelegate> delegate;

+ (Float64)preferredSamplingRate;
@property (nonatomic, readonly) NSUInteger preferredBufferSize;

@end
