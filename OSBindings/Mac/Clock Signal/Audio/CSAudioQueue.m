//
//  AudioQueue.m
//  Clock Signal
//
//  Created by Thomas Harte on 14/01/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#import "CSAudioQueue.h"
@import AudioToolbox;
#include <stdatomic.h>

#define AudioQueueBufferMaxLength		8192

@implementation CSAudioQueue {
	AudioQueueRef _audioQueue;
	NSLock *_storedBuffersLock, *_deallocLock;
	atomic_int _enqueuedBuffers;
}

#pragma mark - AudioQueue callbacks

/*!
	@returns @c YES if the queue is running dry; @c NO otherwise.
*/
- (BOOL)audioQueue:(AudioQueueRef)theAudioQueue didCallbackWithBuffer:(AudioQueueBufferRef)buffer {
	[_storedBuffersLock lock];
	const int buffers = atomic_fetch_add(&_enqueuedBuffers, -1);

	// If that suggests the queue may be exhausted soon, re-enqueue whatever just came back in order to
	// keep the queue going. AudioQueues seem to stop playing and never restart no matter how much
	// encouragement if exhausted.
	if(!buffers) {
		AudioQueueEnqueueBuffer(theAudioQueue, buffer, 0, NULL);
		atomic_fetch_add(&_enqueuedBuffers, 1);
	} else {
		AudioQueueFreeBuffer(_audioQueue, buffer);
	}

	[_storedBuffersLock unlock];
	return YES;
}

- (BOOL)isRunningDry {
	return atomic_load_explicit(&_enqueuedBuffers, memory_order_relaxed) < 3;
}

#pragma mark - Standard object lifecycle

- (instancetype)initWithSamplingRate:(Float64)samplingRate isStereo:(BOOL)isStereo {
	self = [super init];

	if(self) {
		_storedBuffersLock = [[NSLock alloc] init];
		_deallocLock = [[NSLock alloc] init];

		_samplingRate = samplingRate;

		// Determine preferred buffer size as being the first power of two less than
		_preferredBufferSize = AudioQueueBufferMaxLength;
		while((Float64)_preferredBufferSize*100.0 > samplingRate) _preferredBufferSize >>= 1;

		// Describe a 16bit stream of the requested sampling rate.
		AudioStreamBasicDescription outputDescription;

		outputDescription.mSampleRate = samplingRate;
		outputDescription.mChannelsPerFrame = isStereo ? 2 : 1;

		outputDescription.mFormatID = kAudioFormatLinearPCM;
		outputDescription.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger;

		outputDescription.mFramesPerPacket = 1;
		outputDescription.mBytesPerFrame = 2 * outputDescription.mChannelsPerFrame;
		outputDescription.mBytesPerPacket = outputDescription.mBytesPerFrame * outputDescription.mFramesPerPacket;
		outputDescription.mBitsPerChannel = 16;

		outputDescription.mReserved = 0;

		// Create an audio output queue along those lines.
		__weak CSAudioQueue *weakSelf = self;
		if(AudioQueueNewOutputWithDispatchQueue(
				&_audioQueue,
				&outputDescription,
				0,
				dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0),
				^(AudioQueueRef inAQ, AudioQueueBufferRef inBuffer) {
					CSAudioQueue *queue = weakSelf;
					if(!queue) {
						return;
					}

					if([queue->_deallocLock tryLock]) {
						BOOL isRunningDry = NO;
						isRunningDry = [queue audioQueue:inAQ didCallbackWithBuffer:inBuffer];

						id<CSAudioQueueDelegate> delegate = queue.delegate;
						[queue->_deallocLock unlock];

						if(isRunningDry) [delegate audioQueueIsRunningDry:queue];
					}
				}
			)
		) {
			return nil;
		}
	}

	return self;
}

- (void)dealloc {
	[_deallocLock lock];
	if(_audioQueue) {
		AudioQueueDispose(_audioQueue, true);
		_audioQueue = NULL;
	}

	// nil out the dealloc lock before entering the critical section such
	// that it becomes impossible for anyone else to acquire.
	NSLock *deallocLock = _deallocLock;
	_deallocLock = nil;
	[deallocLock unlock];
}

#pragma mark - Audio enqueuer

- (void)enqueueAudioBuffer:(const int16_t *)buffer numberOfSamples:(size_t)lengthInSamples {
	size_t bufferBytes = lengthInSamples * sizeof(int16_t);

	[_storedBuffersLock lock];
	// Don't enqueue more than 4 buffers ahead of now, to ensure not too much latency accrues.
	if(atomic_load_explicit(&_enqueuedBuffers, memory_order_relaxed) > 4) {
		[_storedBuffersLock unlock];
		return;
	}
	const int enqueuedBuffers = atomic_fetch_add(&_enqueuedBuffers, 1);

	AudioQueueBufferRef newBuffer;
	AudioQueueAllocateBuffer(_audioQueue, (UInt32)bufferBytes * 2, &newBuffer);
	memcpy(newBuffer->mAudioData, buffer, bufferBytes);
	newBuffer->mAudioDataByteSize = (UInt32)bufferBytes;

	AudioQueueEnqueueBuffer(_audioQueue, newBuffer, 0, NULL);
	[_storedBuffersLock unlock];

	// 'Start' the queue. This is documented to be a no-op if the queue is already started,
	// and it's better to defer starting it until at least some data is available.
	if(enqueuedBuffers > 2) {
		AudioQueueStart(_audioQueue, NULL);
	}
}

#pragma mark - Sampling Rate getters

+ (AudioDeviceID)defaultOutputDevice {
	AudioObjectPropertyAddress address;
	address.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
	address.mScope = kAudioObjectPropertyScopeGlobal;
	address.mElement = kAudioObjectPropertyElementMaster;

	AudioDeviceID deviceID;
	UInt32 size = sizeof(AudioDeviceID);
	return AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, sizeof(AudioObjectPropertyAddress), NULL, &size, &deviceID) ? 0 : deviceID;
}

+ (Float64)preferredSamplingRate {
	AudioObjectPropertyAddress address;
	address.mSelector = kAudioDevicePropertyNominalSampleRate;
	address.mScope = kAudioObjectPropertyScopeGlobal;
	address.mElement = kAudioObjectPropertyElementMaster;

	Float64 samplingRate;
	UInt32 size = sizeof(Float64);
	return AudioObjectGetPropertyData([self defaultOutputDevice], &address, sizeof(AudioObjectPropertyAddress), NULL, &size, &samplingRate) ? 0.0 : samplingRate;
}

@end
