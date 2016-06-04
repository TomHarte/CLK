//
//  AudioQueue.h
//  Clock Signal
//
//  Created by Thomas Harte on 14/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface AudioQueue : NSObject

- (instancetype)initWithSamplingRate:(Float64)samplingRate;
- (void)enqueueAudioBuffer:(const int16_t *)buffer numberOfSamples:(size_t)lengthInSamples;

@property (nonatomic, readonly) Float64 samplingRate;

+ (Float64)preferredSamplingRate;

@end
