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
#define NumberOfStoredAudioQueueBuffer	16

static NSLock *CSAudioQueueDeallocLock;

/*!
	Holds a weak reference to a CSAudioQueue. Used to work around an apparent AudioQueue bug.
	See -[CSAudioQueue dealloc].
*/
@interface CSWeakAudioQueuePointer: NSObject
@property(nonatomic, weak) CSAudioQueue *queue;
@end

@implementation CSWeakAudioQueuePointer
@end

@implementation CSAudioQueue {
	AudioQueueRef _audioQueue;
	NSLock *_storedBuffersLock;
	CSWeakAudioQueuePointer *_weakPointer;
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
	if(buffers == 1) {
		AudioQueueEnqueueBuffer(theAudioQueue, buffer, 0, NULL);
		atomic_fetch_add(&_enqueuedBuffers, 1);
	} else {
		AudioQueueFreeBuffer(_audioQueue, buffer);
	}

	[_storedBuffersLock unlock];
	return YES;
}

static void audioOutputCallback(
	void *inUserData,
	AudioQueueRef inAQ,
	AudioQueueBufferRef inBuffer) {
	// Pull the delegate call for audio queue running dry outside of the locked region, to allow non-deadlocking
	// lifecycle -dealloc events to result from it.
	if([CSAudioQueueDeallocLock tryLock]) {
		CSAudioQueue *queue = ((__bridge CSWeakAudioQueuePointer *)inUserData).queue;
		BOOL isRunningDry = NO;
		isRunningDry = [queue audioQueue:inAQ didCallbackWithBuffer:inBuffer];
		id<CSAudioQueueDelegate> delegate = queue.delegate;
		[CSAudioQueueDeallocLock unlock];
		if(isRunningDry) [delegate audioQueueIsRunningDry:queue];
	}
}

- (BOOL)isRunningDry {
	return atomic_load_explicit(&_enqueuedBuffers, memory_order_relaxed) < 3;
}

#pragma mark - Standard object lifecycle

- (instancetype)initWithSamplingRate:(Float64)samplingRate isStereo:(BOOL)isStereo {
	self = [super init];

	if(self) {
		if(!CSAudioQueueDeallocLock) {
			CSAudioQueueDeallocLock = [[NSLock alloc] init];
		}
		_storedBuffersLock = [[NSLock alloc] init];

		_samplingRate = samplingRate;

		// determine preferred buffer sizes
		_preferredBufferSize = AudioQueueBufferMaxLength;
		while((Float64)_preferredBufferSize*100.0 > samplingRate) _preferredBufferSize >>= 1;

		/*
			Describe a mono 16bit stream of the requested sampling rate
		*/
		AudioStreamBasicDescription outputDescription;

		outputDescription.mSampleRate = samplingRate;

		outputDescription.mFormatID = kAudioFormatLinearPCM;
		outputDescription.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger;


		outputDescription.mChannelsPerFrame = isStereo ? 2 : 1;
		outputDescription.mFramesPerPacket = 1;
		outputDescription.mBytesPerFrame = 2 * outputDescription.mChannelsPerFrame;
		outputDescription.mBytesPerPacket = outputDescription.mBytesPerFrame * outputDescription.mFramesPerPacket;
		outputDescription.mBitsPerChannel = 16;

		outputDescription.mReserved = 0;

		// create an audio output queue along those lines; see -dealloc re: the CSWeakAudioQueuePointer
		_weakPointer = [[CSWeakAudioQueuePointer alloc] init];
		_weakPointer.queue = self;
		if(!AudioQueueNewOutput(
				&outputDescription,
				audioOutputCallback,
				(__bridge void *)(_weakPointer),
				NULL,
				kCFRunLoopCommonModes,
				0,
				&_audioQueue)) {
		}
	}

	return self;
}

- (void)dealloc {
	[CSAudioQueueDeallocLock lock];
	if(_audioQueue) {
		AudioQueueDispose(_audioQueue, true);
		_audioQueue = NULL;
	}
	[CSAudioQueueDeallocLock unlock];

	// Yuck. Horrid hack happening here. At least under macOS v10.12, I am frequently seeing calls to
	// my registered audio callback (audioOutputCallback in this case) that occur **after** the call
	// to AudioQueueDispose above, even though the second parameter there asks for a synchronous shutdown.
	// So this appears to be a bug on Apple's side.
	//
	// Since the audio callback receives a void * pointer that identifies the class it should branch into,
	// it's therefore unsafe to pass 'self'. Instead I pass a CSWeakAudioQueuePointer which points to the actual
	// queue. The lifetime of that class is the lifetime of this instance plus 1 second, as effected by the
	// artificial dispatch_after below; it serves only to keep pointerSaviour alive for an extra second.
	//
	// Why a second? That's definitely quite a lot longer than any amount of audio that may be queued. So
	// probably safe. As and where Apple's audio queue works properly, CSAudioQueueDeallocLock should provide
	// absolute safety; elsewhere the CSWeakAudioQueuePointer provides probabilistic.
	CSWeakAudioQueuePointer *pointerSaviour = _weakPointer;
	dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
		[pointerSaviour hash];
	});
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
	if(enqueuedBuffers > 3) {
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
