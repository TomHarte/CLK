//
//  AudioQueue.h
//  Clock Signal
//
//  Created by Thomas Harte on 14/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface AudioQueue : NSObject

- (void)enqueueAudioBuffer:(const int16_t *)buffer numberOfSamples:(unsigned int)lengthInSamples;

@end
