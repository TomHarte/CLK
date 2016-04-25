//
//  CSMachine.m
//  Clock Signal
//
//  Created by Thomas Harte on 04/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import "CSMachine.h"
#import "CSMachine+Subclassing.h"

struct SpeakerDelegate: public Outputs::Speaker::Delegate {
	__weak CSMachine *machine;
	void speaker_did_complete_samples(Outputs::Speaker *speaker, const int16_t *buffer, int buffer_size) {
		[machine speaker:speaker didCompleteSamples:buffer length:buffer_size];
	}
};

@implementation CSMachine {
	SpeakerDelegate _speakerDelegate;
	dispatch_queue_t _serialDispatchQueue;
}

- (void)speaker:(Outputs::Speaker *)speaker didCompleteSamples:(const int16_t *)samples length:(int)length {
	[self.audioQueue enqueueAudioBuffer:samples numberOfSamples:(unsigned int)length];
}

- (instancetype)init {
	self = [super init];

	if(self) {
		_serialDispatchQueue = dispatch_queue_create("Machine queue", DISPATCH_QUEUE_SERIAL);
		_speakerDelegate.machine = self;
		[self setSpeakerDelegate:&_speakerDelegate sampleRate:44100];
	}

	return self;
}

- (void)dealloc {
	[_view performWithGLContext:^{
		[self closeOutput];
	}];
}

- (BOOL)setSpeakerDelegate:(Outputs::Speaker::Delegate *)delegate sampleRate:(int)sampleRate {
	return NO;
}

- (void)runForNumberOfCycles:(int)numberOfCycles {}

- (void)performSync:(dispatch_block_t)action {
	dispatch_sync(_serialDispatchQueue, action);
}

- (void)performAsync:(dispatch_block_t)action {
	dispatch_async(_serialDispatchQueue, action);
}

- (void)setupOutputWithAspectRatio:(float)aspectRatio {}

- (void)closeOutput {}

- (void)setView:(CSOpenGLView *)view aspectRatio:(float)aspectRatio {
	_view = view;
	[view performWithGLContext:^{
		[self setupOutputWithAspectRatio:aspectRatio];
	}];
}

@end
