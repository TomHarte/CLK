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

typedef OSStatus (^OSGuardable)(void);
static void OSSGuard(OSGuardable guardable) {
	const OSStatus status = guardable();
	assert(!status);
	(void)status;
}

static BOOL IsDry(int x) { return x < 2; }

#define MaximumBacklog 4
#define NumBuffers MaximumBacklog + 1

@implementation CSAudioQueue {
	AudioQueueRef _audioQueue;

	NSLock *_deallocLock;
	NSLock *_queueLock;

	atomic_int _enqueuedBuffers;
	AudioQueueBufferRef _buffers[NumBuffers];
	int _bufferWritePointer;

	unsigned int _numChannels;
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
		_deallocLock.name = @"Dealloc lock";

		_queueLock = [[NSLock alloc] init];
		_queueLock.name = @"Audio queue access lock";

		atomic_store_explicit(&_enqueuedBuffers, 0, memory_order_relaxed);

		_samplingRate = samplingRate;
		_numChannels = isStereo ? 2 : 1;

		// Determine preferred buffer size as being the first power of two
		// not less than 1/100th of a second.
		_preferredBufferSize = 1;
		const NSUInteger oneHundredthOfRate = (NSUInteger)(samplingRate / 100.0);
		while(_preferredBufferSize < oneHundredthOfRate) _preferredBufferSize <<= 1;

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
					(void)inBuffer;

					CSAudioQueue *queue = weakSelf;
					if(!queue) {
						return;
					}

					if([queue->_deallocLock tryLock]) {
						const int buffers = atomic_fetch_add(&queue->_enqueuedBuffers, -1) - 1;
						if(!buffers) {
							[queue->_queueLock lock];
								OSSGuard(^{return AudioQueuePause(inAQ);});
							[queue->_queueLock unlock];
						}

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
		// Ensure no buffers remain enqueued by stopping the queue.
		if(_audioQueue) {
			OSSGuard(^{return AudioQueueStop(self->_audioQueue, true);});
		}

		// Free all buffers.
		for(size_t c = 0; c < NumBuffers; c++) {
			if(_buffers[c]) {
				OSSGuard(^{return AudioQueueFreeBuffer(self->_audioQueue, self->_buffers[c]);});
				_buffers[c] = NULL;
			}
		}

		// Dispose of the queue.
		if(_audioQueue) {
			OSSGuard(^{return AudioQueueDispose(self->_audioQueue, true);});
			_audioQueue = NULL;
		}

		// nil out the dealloc lock before entering the critical section such
		// that it becomes impossible for anyone else to acquire.
		NSLock *deallocLock = _deallocLock;
		_deallocLock = nil;
	[deallocLock unlock];
}

#pragma mark - Audio enqueuer

- (void)setBufferSize:(NSUInteger)bufferSize {
	_bufferSize = bufferSize;

	// Allocate future audio buffers.
	[_queueLock lock];
		const size_t bufferBytes = self.bufferSize * sizeof(int16_t) * _numChannels;
		for(size_t c = 0; c < NumBuffers; c++) {
			if(_buffers[c]) {
				OSSGuard(^{return AudioQueueFreeBuffer(self->_audioQueue, self->_buffers[c]);});
			}

			OSSGuard(^{return AudioQueueAllocateBuffer(self->_audioQueue, (UInt32)bufferBytes, &self->_buffers[c]);});
			_buffers[c]->mAudioDataByteSize = (UInt32)bufferBytes;
		}
	[_queueLock unlock];
}

- (void)enqueueAudioBuffer:(const int16_t *)buffer {
	const size_t bufferBytes = self.bufferSize * sizeof(int16_t) * _numChannels;

	// Don't enqueue more than the allowed number of future buffers,
	// to ensure not too much latency accrues.
	if(atomic_load_explicit(&_enqueuedBuffers, memory_order_relaxed) == MaximumBacklog) {
		return;
	}
	const int enqueuedBuffers = atomic_fetch_add(&_enqueuedBuffers, 1) + 1;

	const int targetBuffer = _bufferWritePointer;
	_bufferWritePointer = (_bufferWritePointer + 1) % NumBuffers;
	memcpy(_buffers[targetBuffer]->mAudioData, buffer, bufferBytes);

	[_queueLock lock];
		OSSGuard(^{return AudioQueueEnqueueBuffer(self->_audioQueue, self->_buffers[targetBuffer], 0, NULL);});

		// Starting is a no-op if the queue is already playing, but it may not have been started
		// yet, or may have been paused due to a pipeline failure if the producer is running slowly.
		if(enqueuedBuffers > 1) {
			OSSGuard(^{
				const OSStatus result = AudioQueueStart(self->_audioQueue, NULL);
				if(result == kAudioQueueErr_CannotStart) {
					// Accept cannot-start, hoping it's ephemeral; Apple's specific advice is:
					// "Sleeping briefly and retrying is recommended".
					return kAudioSessionNoError;
				}
				return result;
			});
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
