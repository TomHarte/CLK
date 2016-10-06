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
#define AudioQueueNumAudioBuffers		3
#define AudioQueueMaxStreamLength		(AudioQueueBufferMaxLength*AudioQueueNumAudioBuffers)

enum {
	AudioQueueCanProceed,
	AudioQueueWait,
	AudioQueueIsInvalidated
};

@implementation CSAudioQueue
{
	NSUInteger _bufferLength;
	NSUInteger _streamLength;

	AudioQueueRef _audioQueue;
	AudioQueueBufferRef _audioBuffers[AudioQueueNumAudioBuffers];

	unsigned int _audioStreamReadPosition, _audioStreamWritePosition;
	int16_t _audioStream[AudioQueueMaxStreamLength];

	NSConditionLock *_writeLock;
	BOOL _isInvalidated;
	int _dequeuedCount;
}

#pragma mark -
#pragma mark AudioQueue callbacks and setup; for pushing audio out

- (void)audioQueue:(AudioQueueRef)theAudioQueue didCallbackWithBuffer:(AudioQueueBufferRef)buffer
{
	[self.delegate audioQueueDidCompleteBuffer:self];

	[_writeLock lock];

	const unsigned int writeLead = _audioStreamWritePosition - _audioStreamReadPosition;
	const size_t audioDataSampleSize = buffer->mAudioDataByteSize / sizeof(int16_t);

	// TODO: if write lead is too great, skip some audio
	if(writeLead >= audioDataSampleSize)
	{
		size_t samplesBeforeOverflow = _streamLength - (_audioStreamReadPosition % _streamLength);
		if(audioDataSampleSize <= samplesBeforeOverflow)
		{
			memcpy(buffer->mAudioData, &_audioStream[_audioStreamReadPosition % _streamLength], buffer->mAudioDataByteSize);
		}
		else
		{
			const size_t bytesRemaining = samplesBeforeOverflow * sizeof(int16_t);
			memcpy(buffer->mAudioData, &_audioStream[_audioStreamReadPosition % _streamLength], bytesRemaining);
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
	[(__bridge CSAudioQueue *)inUserData audioQueue:inAQ didCallbackWithBuffer:inBuffer];
}

- (instancetype)initWithSamplingRate:(Float64)samplingRate
{
	self = [super init];

	if(self)
	{
		_writeLock = [[NSConditionLock alloc] initWithCondition:AudioQueueCanProceed];
		_samplingRate = samplingRate;

		// determine buffer sizes
		_bufferLength = AudioQueueBufferMaxLength;
		while((Float64)_bufferLength*50.0 > samplingRate) _bufferLength >>= 1;
		_streamLength = _bufferLength * AudioQueueNumAudioBuffers;

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
			UInt32 bufferBytes = (UInt32)(_bufferLength * sizeof(int16_t));

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
	if([_writeLock tryLockWhenCondition:AudioQueueCanProceed])
	{
		if((_audioStreamReadPosition + _streamLength) - _audioStreamWritePosition >= lengthInSamples)
		{
			size_t samplesBeforeOverflow = _streamLength - (_audioStreamWritePosition % _streamLength);

			if(samplesBeforeOverflow < lengthInSamples)
			{
				memcpy(&_audioStream[_audioStreamWritePosition % _streamLength], buffer, samplesBeforeOverflow * sizeof(int16_t));
				memcpy(&_audioStream[0], &buffer[samplesBeforeOverflow], (lengthInSamples - samplesBeforeOverflow) * sizeof(int16_t));
			}
			else
			{
				memcpy(&_audioStream[_audioStreamWritePosition % _streamLength], buffer, lengthInSamples * sizeof(int16_t));
			}

			_audioStreamWritePosition += lengthInSamples;
			[_writeLock unlockWithCondition:[self writeLockCondition]];
		}
		else
		{
			[_writeLock unlockWithCondition:AudioQueueWait];
		}
	}
}

- (NSInteger)writeLockCondition
{
	return ((_audioStreamWritePosition - _audioStreamReadPosition) < (_streamLength - _bufferLength)) ? AudioQueueCanProceed : AudioQueueWait;
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

- (NSUInteger)bufferSize
{
	return _bufferLength;
}

@end
