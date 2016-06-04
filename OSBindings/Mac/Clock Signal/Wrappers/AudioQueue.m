//
//  AudioQueue.m
//  Clock Signal
//
//  Created by Thomas Harte on 14/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import "AudioQueue.h"
@import AudioToolbox;

#define AudioQueueNumAudioBuffers	4
#define AudioQueueStreamLength		1024
#define AudioQueueBufferLength		512

enum {
	AudioQueueCanProceed,
	AudioQueueWait,
	AudioQueueIsInvalidated
};

@implementation AudioQueue
{
	AudioQueueRef _audioQueue;
	AudioQueueBufferRef _audioBuffers[AudioQueueNumAudioBuffers];
	unsigned int _audioStreamReadPosition, _audioStreamWritePosition;
	int16_t _audioStream[AudioQueueStreamLength];
	NSConditionLock *_writeLock;
	BOOL _isInvalidated;
	int _dequeuedCount;
}


#pragma mark -
#pragma mark AudioQueue callbacks and setup; for pushing audio out

- (void)audioQueue:(AudioQueueRef)theAudioQueue didCallbackWithBuffer:(AudioQueueBufferRef)buffer
{
	[_writeLock lock];

	const unsigned int writeLead = _audioStreamWritePosition - _audioStreamReadPosition;
	const size_t audioDataSampleSize = buffer->mAudioDataByteSize / sizeof(int16_t);

	// TODO: if write lead is too great, skip some audio
	if(writeLead >= audioDataSampleSize)
	{
		size_t samplesBeforeOverflow = AudioQueueStreamLength - (_audioStreamReadPosition % AudioQueueStreamLength);
		if(audioDataSampleSize <= samplesBeforeOverflow)
		{
			memcpy(buffer->mAudioData, &_audioStream[_audioStreamReadPosition % AudioQueueStreamLength], buffer->mAudioDataByteSize);
		}
		else
		{
			const size_t bytesRemaining = samplesBeforeOverflow * sizeof(int16_t);
			memcpy(buffer->mAudioData, &_audioStream[_audioStreamReadPosition % AudioQueueStreamLength], bytesRemaining);
			memcpy(buffer->mAudioData, &_audioStream[0], buffer->mAudioDataByteSize - bytesRemaining);
		}
		_audioStreamReadPosition += audioDataSampleSize;
	}
	else
	{
		memset(buffer->mAudioData, 0, buffer->mAudioDataByteSize);
	}

	if(!_isInvalidated)
	{
		[_writeLock unlockWithCondition:AudioQueueCanProceed];
		AudioQueueEnqueueBuffer(theAudioQueue, buffer, 0, NULL);
	}
	else
	{
		_dequeuedCount++;
		if(_dequeuedCount == AudioQueueNumAudioBuffers)
			[_writeLock unlockWithCondition:AudioQueueIsInvalidated];
		else
			[_writeLock unlockWithCondition:AudioQueueCanProceed];
	}
}

static void audioOutputCallback(
	void *inUserData,
	AudioQueueRef inAQ,
	AudioQueueBufferRef inBuffer)
{
	[(__bridge AudioQueue *)inUserData audioQueue:inAQ didCallbackWithBuffer:inBuffer];
}

- (instancetype)initWithSamplingRate:(Float64)samplingRate
{
	self = [super init];

	if(self)
	{
		_writeLock = [[NSConditionLock alloc] initWithCondition:AudioQueueCanProceed];
		_samplingRate = samplingRate;

		/*
			Describe a mono, 16bit, 44.1Khz audio format
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
			UInt32 bufferBytes = AudioQueueBufferLength * sizeof(int16_t);

			int c = AudioQueueNumAudioBuffers;
			while(c--)
			{
				AudioQueueAllocateBuffer(_audioQueue, bufferBytes, &_audioBuffers[c]);
				memset(_audioBuffers[c]->mAudioData, 0, bufferBytes);
				_audioBuffers[c]->mAudioDataByteSize = bufferBytes;
				AudioQueueEnqueueBuffer(_audioQueue, _audioBuffers[c], 0, NULL);
			}

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
	[_writeLock lock];
	_isInvalidated = YES;
	[_writeLock unlock];

	[_writeLock lockWhenCondition:AudioQueueIsInvalidated];
	[_writeLock unlock];

	int c = AudioQueueNumAudioBuffers;
	while(c--)
		AudioQueueFreeBuffer(_audioQueue, _audioBuffers[c]);

	if(_audioQueue) AudioQueueDispose(_audioQueue, NO);
}

- (void)enqueueAudioBuffer:(const int16_t *)buffer numberOfSamples:(size_t)lengthInSamples
{
	while(1)
	{
		[_writeLock lockWhenCondition:AudioQueueCanProceed];
		if((_audioStreamReadPosition + AudioQueueStreamLength) - _audioStreamWritePosition >= lengthInSamples)
		{
			size_t samplesBeforeOverflow = AudioQueueStreamLength - (_audioStreamWritePosition % AudioQueueStreamLength);

			if(samplesBeforeOverflow < lengthInSamples)
			{
				memcpy(&_audioStream[_audioStreamWritePosition % AudioQueueStreamLength], buffer, samplesBeforeOverflow * sizeof(int16_t));
				memcpy(&_audioStream[0], &buffer[samplesBeforeOverflow], (lengthInSamples - samplesBeforeOverflow) * sizeof(int16_t));
			}
			else
			{
				memcpy(&_audioStream[_audioStreamWritePosition % AudioQueueStreamLength], buffer, lengthInSamples * sizeof(int16_t));
			}

			_audioStreamWritePosition += lengthInSamples;
			[_writeLock unlockWithCondition:[self writeLockCondition]];
			break;
		}
		else
		{
			[_writeLock unlockWithCondition:AudioQueueWait];
		}
	}
}

- (NSInteger)writeLockCondition
{
	return ((_audioStreamWritePosition - _audioStreamReadPosition) < (AudioQueueStreamLength - AudioQueueBufferLength)) ? AudioQueueCanProceed : AudioQueueWait;
}

+ (AudioDeviceID)defaultOutputDevice
{
	AudioObjectPropertyAddress address;
	address.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
	address.mScope = kAudioObjectPropertyScopeGlobal;
	address.mElement = kAudioObjectPropertyElementMaster;

	AudioDeviceID deviceID;
	UInt32 size = sizeof(AudioDeviceID);
	return AudioHardwareServiceGetPropertyData(kAudioObjectSystemObject, &address, 0, NULL, &size, &deviceID) ? 0 : deviceID;
}

+ (Float64)preferredSamplingRate
{
	AudioObjectPropertyAddress address;
	address.mSelector = kAudioDevicePropertyNominalSampleRate;
	address.mScope = kAudioObjectPropertyScopeGlobal;
	address.mElement = kAudioObjectPropertyElementMaster;

	Float64 samplingRate;
	UInt32 size = sizeof(Float64);
	return AudioHardwareServiceGetPropertyData([self defaultOutputDevice], &address, 0, NULL, &size, &samplingRate) ? 0.0 : samplingRate;
}

@end
