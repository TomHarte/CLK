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

#define OSSGuard(x)	{			\
	const OSStatus status = x;	\
	assert(!status);			\
	(void)status;				\
}

#define IsDry(x)	(x) < 3

@implementation CSAudioQueue {
	AudioQueueRef _audioQueue;
	NSLock *_deallocLock;
	NSLock *_queueLock;
	atomic_int _enqueuedBuffers;
}

#pragma mark - Status

- (BOOL)isRunningDry {
	return IsDry(atomic_load_explicit(&_enqueuedBuffers, memory_order_relaxed));
}

#pragma mark - Object lifecycle

- (instancetype)initWithSamplingRate:(Float64)samplingRate isStereo:(BOOL)isStereo {
	self = [super init];

	if(self) {
		_deallocLock = [[NSLock alloc] init];
		_queueLock = [[NSLock alloc] init];
		atomic_store_explicit(&_enqueuedBuffers, 0, memory_order_relaxed);

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
						[queue->_queueLock lock];

						OSSGuard(AudioQueueFreeBuffer(inAQ, inBuffer));

						const int buffers = atomic_fetch_add(&queue->_enqueuedBuffers, -1) - 1;
//						if(!buffers) {
//							OSSGuard(AudioQueueStop(inAQ, true));
//						}

						[queue->_queueLock unlock];

						id<CSAudioQueueDelegate> delegate = queue.delegate;
						[queue->_deallocLock unlock];

						if(IsDry(buffers)) [delegate audioQueueIsRunningDry:queue];
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
		OSSGuard(AudioQueueDispose(_audioQueue, true));
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

	// Don't enqueue more than 4 buffers ahead of now, to ensure not too much latency accrues.
	if(atomic_load_explicit(&_enqueuedBuffers, memory_order_relaxed) == 4) {
		return;
	}
	const int enqueuedBuffers = atomic_fetch_add(&_enqueuedBuffers, 1) + 1;

	[_queueLock lock];

	AudioQueueBufferRef newBuffer;
	OSSGuard(AudioQueueAllocateBuffer(_audioQueue, (UInt32)bufferBytes * 2, &newBuffer));
	memcpy(newBuffer->mAudioData, buffer, bufferBytes);
	newBuffer->mAudioDataByteSize = (UInt32)bufferBytes;

	OSSGuard(AudioQueueEnqueueBuffer(_audioQueue, newBuffer, 0, NULL));

	// 'Start' the queue. This is documented to be a no-op if the queue is already started,
	// and it's better to defer starting it until at least some data is available.
	if(enqueuedBuffers > 2) {
		OSSGuard(AudioQueueStart(_audioQueue, NULL));
	}

	[_queueLock unlock];
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
