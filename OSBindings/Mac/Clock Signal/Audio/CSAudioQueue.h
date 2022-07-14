//
//  AudioQueue.h
//  Clock Signal
//
//  Created by Thomas Harte on 14/01/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>

@class CSAudioQueue;

@protocol CSAudioQueueDelegate
- (void)audioQueueIsRunningDry:(nonnull CSAudioQueue *)audioQueue;
@end

/*!
	CSAudioQueue provides an audio queue to which packets of arbitrary size may be appended;
	it can notify a delegate each time a buffer is completed and offer advice as to the preferred
	output sampling rate and a manageable buffer size for this machine.
*/
@interface CSAudioQueue : NSObject

/*!
	Creates an instance of CSAudioQueue.

	@param samplingRate The output audio rate.
	@param isStereo @c YES if audio buffers will contain stereo audio, @c NO otherwise.

	@returns An instance of CSAudioQueue if successful; @c nil otherwise.
*/
- (nullable instancetype)initWithSamplingRate:(Float64)samplingRate isStereo:(BOOL)isStereo NS_DESIGNATED_INITIALIZER;
- (nonnull instancetype)init __attribute((unavailable));

/*!
	Enqueues a buffer for playback.

	@param buffer A pointer to the data that comprises the buffer.
	@param lengthInSamples The length of the buffer, in samples.
*/
- (void)enqueueAudioBuffer:(nonnull const int16_t *)buffer numberOfSamples:(size_t)lengthInSamples;

/// @returns The sampling rate at which this queue is playing audio.
@property (nonatomic, readonly) Float64 samplingRate;

/// A delegate, if set, will receive notification upon the completion of each enqueue buffer.
@property (nonatomic, weak, nullable) id<CSAudioQueueDelegate> delegate;

/*!
	@returns The ideal output sampling rate for this computer; likely to be 44.1Khz or
	48Khz or 96Khz or one of the other comon numbers but not guaranteed to be.
*/
+ (Float64)preferredSamplingRate;

/*!
	@returns A selected preferred buffer size (in samples). If an owner cannot otherwise
	decide in what size to enqueue audio, this is a helpful suggestion.
*/
@property (nonatomic, readonly) NSUInteger preferredBufferSize;

/*!
	@returns @C YES if this queue is running low or is completely exhausted of new audio buffers.
*/
@property (atomic, readonly) BOOL isRunningDry;

@end
