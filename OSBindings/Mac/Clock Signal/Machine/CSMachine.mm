//
//  CSMachine.m
//  Clock Signal
//
//  Created by Thomas Harte on 04/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import "CSMachine.h"
#import "CSMachine+Subclassing.h"
#import "CSMachine+Target.h"

#include "Typer.hpp"
#include "ConfigurationTarget.hpp"

@interface CSMachine()
- (void)speaker:(Outputs::Speaker *)speaker didCompleteSamples:(const int16_t *)samples length:(int)length;
- (void)machineDidChangeClockRate;
- (void)machineDidChangeClockIsUnlimited;
@end

struct SpeakerDelegate: public Outputs::Speaker::Delegate {
	__weak CSMachine *machine;
	void speaker_did_complete_samples(Outputs::Speaker *speaker, const std::vector<int16_t> &buffer) {
		[machine speaker:speaker didCompleteSamples:buffer.data() length:(int)buffer.size()];
	}
};

struct MachineDelegate: CRTMachine::Machine::Delegate {
	__weak CSMachine *machine;
	void machine_did_change_clock_rate(CRTMachine::Machine *sender) {
		[machine machineDidChangeClockRate];
	}
	void machine_did_change_clock_is_unlimited(CRTMachine::Machine *sender) {
		[machine machineDidChangeClockIsUnlimited];
	}
};

@implementation CSMachine {
	SpeakerDelegate _speakerDelegate;
	MachineDelegate _machineDelegate;
}

- (instancetype)init {
	self = [super init];
	if(self)
	{
		_machineDelegate.machine = self;
		self.machine->set_delegate(&_machineDelegate);
		_speakerDelegate.machine = self;
	}
	return self;
}

- (void)speaker:(Outputs::Speaker *)speaker didCompleteSamples:(const int16_t *)samples length:(int)length {
	[self.audioQueue enqueueAudioBuffer:samples numberOfSamples:(unsigned int)length];
}

- (void)machineDidChangeClockRate {
	[self.delegate machineDidChangeClockRate:self];
}

- (void)machineDidChangeClockIsUnlimited {
	[self.delegate machineDidChangeClockIsUnlimited:self];
}

- (void)dealloc {
	[_view performWithGLContext:^{
		@synchronized(self) {
			self.machine->close_output();
		}
	}];
}

- (float)idealSamplingRateFromRange:(NSRange)range {
	@synchronized(self) {
		std::shared_ptr<Outputs::Speaker> speaker = self.machine->get_speaker();
		if(speaker)
		{
			return speaker->get_ideal_clock_rate_in_range((float)range.location, (float)(range.location + range.length));
		}
		return 0;
	}
}

- (void)setAudioSamplingRate:(float)samplingRate bufferSize:(NSUInteger)bufferSize {
	@synchronized(self) {
		[self setSpeakerDelegate:&_speakerDelegate sampleRate:samplingRate bufferSize:bufferSize];
	}
}

- (BOOL)setSpeakerDelegate:(Outputs::Speaker::Delegate *)delegate sampleRate:(float)sampleRate bufferSize:(NSUInteger)bufferSize {
	@synchronized(self) {
		std::shared_ptr<Outputs::Speaker> speaker = self.machine->get_speaker();
		if(speaker)
		{
			speaker->set_output_rate(sampleRate, (int)bufferSize);
			speaker->set_delegate(delegate);
			return YES;
		}
		return NO;
	}
}

- (void)runForNumberOfCycles:(int)numberOfCycles {
	@synchronized(self) {
		self.machine->run_for(Cycles(numberOfCycles));
	}
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

- (double)clockRate {
	return self.machine->get_clock_rate();
}

- (BOOL)clockIsUnlimited {
	return self.machine->get_clock_is_unlimited() ? YES : NO;
}

- (void)paste:(NSString *)paste {
	Utility::TypeRecipient *typeRecipient = dynamic_cast<Utility::TypeRecipient *>(self.machine);
	if(typeRecipient)
		typeRecipient->set_typer_for_string([paste UTF8String]);
}

- (void)applyTarget:(StaticAnalyser::Target)target {
	@synchronized(self) {
		ConfigurationTarget::Machine *const configurationTarget =
			dynamic_cast<ConfigurationTarget::Machine *>(self.machine);
		if(configurationTarget) configurationTarget->configure_as_target(target);
	}
}

@end
