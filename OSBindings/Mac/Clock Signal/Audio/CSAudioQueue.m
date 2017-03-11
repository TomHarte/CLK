//
//  AudioQueue.m
//  Clock Signal
//
//  Created by Thomas Harte on 14/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import "CSAudioQueue.h"
@import AudioToolbox;

#define AudioQueueBufferMaxLength		8192
#define NumberOfStoredAudioQueueBuffer	16

static NSLock *CSAudioQueueDeallocLock;

@implementation CSAudioQueue
{
	AudioQueueRef _audioQueue;
	AudioQueueBufferRef _storedBuffers[NumberOfStoredAudioQueueBuffer];
	NSLock *_storedBuffersLock;
}

#pragma mark - AudioQueue callbacks

/*!
	@returns @c YES if the queue is running dry; @c NO otherwise.
*/
- (BOOL)audioQueue:(AudioQueueRef)theAudioQueue didCallbackWithBuffer:(AudioQueueBufferRef)buffer
{
	[_storedBuffersLock lock];
	for(int c = 0; c < NumberOfStoredAudioQueueBuffer; c++)
	{
		if(!_storedBuffers[c] || buffer->mAudioDataBytesCapacity > _storedBuffers[c]->mAudioDataBytesCapacity)
		{
			if(_storedBuffers[c]) AudioQueueFreeBuffer(_audioQueue, _storedBuffers[c]);
			_storedBuffers[c] = buffer;
			[_storedBuffersLock unlock];
			return YES;
		}
	}
	[_storedBuffersLock unlock];
	AudioQueueFreeBuffer(_audioQueue, buffer);
	return YES;
}

static void audioOutputCallback(
	void *inUserData,
	AudioQueueRef inAQ,
	AudioQueueBufferRef inBuffer)
{
	// Pull the delegate call for audio queue running dry outside of the locked region, to allow non-deadlocking
	// lifecycle -dealloc events to result from it.
	CSAudioQueue *queue = (__bridge CSAudioQueue *)inUserData;
	BOOL isRunningDry = NO;
	if([CSAudioQueueDeallocLock tryLock])
	{
		isRunningDry = [queue audioQueue:inAQ didCallbackWithBuffer:inBuffer];
		[CSAudioQueueDeallocLock unlock];
	}
	if(isRunningDry) [queue.delegate audioQueueIsRunningDry:queue];
}

#pragma mark - Standard object lifecycle

- (instancetype)initWithSamplingRate:(Float64)samplingRate
{
	self = [super init];

	if(self)
	{
		if(!CSAudioQueueDeallocLock)
		{
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

		outputDescription.mBytesPerPacket = 2;
		outputDescription.mFramesPerPacket = 1;
		outputDescription.mBytesPerFrame = 2;
		outputDescription.mChannelsPerFrame = 1;
		outputDescription.mBitsPerChannel = 16;

		outputDescription.mReserved = 0;

		// create an audio output queue along those lines
		if(!AudioQueueNewOutput(
				&outputDescription,
				audioOutputCallback,
				(__bridge void *)(self),
				NULL,
				kCFRunLoopCommonModes,
				0,
				&_audioQueue))
		{
			AudioQueueStart(_audioQueue, NULL);
		}
	}

	return self;
}

- (instancetype)init
{
	return [self initWithSamplingRate:[[self class] preferredSamplingRate]];
}

- (void)dealloc
{
	[CSAudioQueueDeallocLock lock];
	if(_audioQueue) AudioQueueDispose(_audioQueue, true);
	[CSAudioQueueDeallocLock unlock];
}

#pragma mark - Audio enqueuer

- (void)enqueueAudioBuffer:(const int16_t *)buffer numberOfSamples:(size_t)lengthInSamples
{
	size_t bufferBytes = lengthInSamples * sizeof(int16_t);

	[_storedBuffersLock lock];
	for(int c = 0; c < NumberOfStoredAudioQueueBuffer; c++)
	{
		if(_storedBuffers[c] && _storedBuffers[c]->mAudioDataBytesCapacity >= bufferBytes)
		{
			memcpy(_storedBuffers[c]->mAudioData, buffer, bufferBytes);
			_storedBuffers[c]->mAudioDataByteSize = (UInt32)bufferBytes;

			AudioQueueEnqueueBuffer(_audioQueue, _storedBuffers[c], 0, NULL);
			_storedBuffers[c] = NULL;
			[_storedBuffersLock unlock];
			return;
		}
	}
	[_storedBuffersLock unlock];

	AudioQueueBufferRef newBuffer;
	AudioQueueAllocateBuffer(_audioQueue, (UInt32)bufferBytes * 2, &newBuffer);
	memcpy(newBuffer->mAudioData, buffer, bufferBytes);
	newBuffer->mAudioDataByteSize = (UInt32)bufferBytes;

	AudioQueueEnqueueBuffer(_audioQueue, newBuffer, 0, NULL);
}

#pragma mark - Sampling Rate getters

+ (AudioDeviceID)defaultOutputDevice
{
	AudioObjectPropertyAddress address;
	address.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
	address.mScope = kAudioObjectPropertyScopeGlobal;
	address.mElement = kAudioObjectPropertyElementMaster;

	AudioDeviceID deviceID;
	UInt32 size = sizeof(AudioDeviceID);
	return AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, sizeof(AudioObjectPropertyAddress), NULL, &size, &deviceID) ? 0 : deviceID;
}

+ (Float64)preferredSamplingRate
{
	AudioObjectPropertyAddress address;
	address.mSelector = kAudioDevicePropertyNominalSampleRate;
	address.mScope = kAudioObjectPropertyScopeGlobal;
	address.mElement = kAudioObjectPropertyElementMaster;

	Float64 samplingRate;
	UInt32 size = sizeof(Float64);
	return AudioObjectGetPropertyData([self defaultOutputDevice], &address, sizeof(AudioObjectPropertyAddress), NULL, &size, &samplingRate) ? 0.0 : samplingRate;
}

@end
