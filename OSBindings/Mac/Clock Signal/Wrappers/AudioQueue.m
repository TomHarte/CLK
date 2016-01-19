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
#define AudioQueueBufferLength		256

enum {
	AudioQueueCanWrite,
	AudioQueueWait
};

@implementation AudioQueue
{
	AudioQueueRef _audioQueue;
	AudioQueueBufferRef _audioBuffers[AudioQueueNumAudioBuffers];
	unsigned int _audioStreamReadPosition, _audioStreamWritePosition;
	int16_t _audioStream[AudioQueueStreamLength];
	NSConditionLock *_writeLock;

#ifdef DEBUG_INPUT
	NSFileHandle *
#endif
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
	AudioQueueEnqueueBuffer(theAudioQueue, buffer, 0, NULL);

	[_writeLock unlockWithCondition:AudioQueueCanWrite];
}

static void audioOutputCallback(
	void *inUserData,
	AudioQueueRef inAQ,
	AudioQueueBufferRef inBuffer)
{
	[(__bridge AudioQueue *)inUserData audioQueue:inAQ didCallbackWithBuffer:inBuffer];
}

- (instancetype)init
{
	self = [super init];

	if(self)
	{
		_writeLock = [[NSConditionLock alloc] initWithCondition:AudioQueueCanWrite];

		/*
			Describe a mono, 16bit, 44.1Khz audio format
		*/
		AudioStreamBasicDescription outputDescription;

		outputDescription.mSampleRate = 44100;

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

- (void)dealloc
{
	int c = AudioQueueNumAudioBuffers;
	while(c--)
		AudioQueueFreeBuffer(_audioQueue, _audioBuffers[c]);

	if(_audioQueue) AudioQueueDispose(_audioQueue, NO);
}

- (void)enqueueAudioBuffer:(const int16_t *)buffer numberOfSamples:(size_t)lengthInSamples
{
	while(1)
	{
		[_writeLock lockWhenCondition:AudioQueueCanWrite];
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
	return ((_audioStreamWritePosition - _audioStreamReadPosition) < (AudioQueueStreamLength - AudioQueueBufferLength)) ? AudioQueueCanWrite : AudioQueueWait;
}

@end
