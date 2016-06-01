//
//  CSMachine.m
//  Clock Signal
//
//  Created by Thomas Harte on 04/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import "CSMachine.h"
#import "CSMachine+Subclassing.h"

@interface CSMachine()
- (void)speaker:(Outputs::Speaker *)speaker didCompleteSamples:(const int16_t *)samples length:(int)length;
@end

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
		@synchronized(self) {
			self.machine->close_output();
		}
	}];
}

- (BOOL)setSpeakerDelegate:(Outputs::Speaker::Delegate *)delegate sampleRate:(int)sampleRate {
	@synchronized(self) {
		Outputs::Speaker *speaker = self.machine->get_speaker();
		if(speaker)
		{
			speaker->set_output_rate(sampleRate, 256);
			speaker->set_delegate(delegate);
			return YES;
		}
		return NO;
	}
}

- (void)runForNumberOfCycles:(int)numberOfCycles {
	@synchronized(self) {
		self.machine->run_for_cycles(numberOfCycles);
	}
}

- (void)performSync:(dispatch_block_t)action {
	dispatch_sync(_serialDispatchQueue, action);
}

- (void)performAsync:(dispatch_block_t)action {
	dispatch_async(_serialDispatchQueue, action);
}

- (void)setView:(CSOpenGLView *)view aspectRatio:(float)aspectRatio {
	_view = view;
	[view performWithGLContext:^{
		[self setupOutputWithAspectRatio:aspectRatio];
	}];
}

- (void)setupOutputWithAspectRatio:(float)aspectRatio {
	self.machine->setup_output(aspectRatio);
}

- (void)drawViewForPixelSize:(CGSize)pixelSize onlyIfDirty:(BOOL)onlyIfDirty {
	self.machine->get_crt()->draw_frame((unsigned int)pixelSize.width, (unsigned int)pixelSize.height, onlyIfDirty ? true : false);
}


@end
