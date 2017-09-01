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

struct LockProtectedDelegate {
	// Contractual promise is: machine — the pointer **and** the object ** — may be accessed only
	// in sections protected by the machineAccessLock;
	NSLock *machineAccessLock;
	__unsafe_unretained CSMachine *machine;
};

struct SpeakerDelegate: public Outputs::Speaker::Delegate, public LockProtectedDelegate {
	void speaker_did_complete_samples(Outputs::Speaker *speaker, const std::vector<int16_t> &buffer) {
		[machineAccessLock lock];
		[machine speaker:speaker didCompleteSamples:buffer.data() length:(int)buffer.size()];
		[machineAccessLock unlock];
	}
};

struct MachineDelegate: CRTMachine::Machine::Delegate, public LockProtectedDelegate {
	void machine_did_change_clock_rate(CRTMachine::Machine *sender) {
		[machineAccessLock lock];
		[machine machineDidChangeClockRate];
		[machineAccessLock unlock];
	}
	void machine_did_change_clock_is_unlimited(CRTMachine::Machine *sender) {
		[machineAccessLock lock];
		[machine machineDidChangeClockIsUnlimited];
		[machineAccessLock unlock];
	}
};

@implementation CSMachine {
	SpeakerDelegate _speakerDelegate;
	MachineDelegate _machineDelegate;
	NSLock *_delegateMachineAccessLock;
	CRTMachine::Machine *_machine;
}

- (instancetype)initWithMachine:(void *)machine {
	self = [super init];
	if(self) {
		_machine = (CRTMachine::Machine *)machine;
		_delegateMachineAccessLock = [[NSLock alloc] init];

		_machineDelegate.machine = self;
		_speakerDelegate.machine = self;
		_machineDelegate.machineAccessLock = _delegateMachineAccessLock;
		_speakerDelegate.machineAccessLock = _delegateMachineAccessLock;

		_machine->set_delegate(&_machineDelegate);
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
	// The two delegate's references to this machine are nilled out here because close_output may result
	// in a data flush, which might cause an audio callback, which could cause the audio queue to decide
	// that it's out of data, resulting in an attempt further to run the machine while it is dealloc'ing.
	//
	// They are nilled inside an explicit lock because that allows the delegates to protect their entire
	// call into the machine, not just the pointer access.
	[_delegateMachineAccessLock lock];
	_machineDelegate.machine = nil;
	_speakerDelegate.machine = nil;
	[_delegateMachineAccessLock unlock];

	[_view performWithGLContext:^{
		@synchronized(self) {
			_machine->close_output();
		}
	}];
}

- (float)idealSamplingRateFromRange:(NSRange)range {
	@synchronized(self) {
		std::shared_ptr<Outputs::Speaker> speaker = _machine->get_speaker();
		if(speaker) {
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
		std::shared_ptr<Outputs::Speaker> speaker = _machine->get_speaker();
		if(speaker) {
			speaker->set_output_rate(sampleRate, (int)bufferSize);
			speaker->set_delegate(delegate);
			return YES;
		}
		return NO;
	}
}

- (void)runForNumberOfCycles:(int)numberOfCycles {
	@synchronized(self) {
		_machine->run_for(Cycles(numberOfCycles));
	}
}

- (void)setView:(CSOpenGLView *)view aspectRatio:(float)aspectRatio {
	_view = view;
	[view performWithGLContext:^{
		[self setupOutputWithAspectRatio:aspectRatio];
	}];
}

- (void)setupOutputWithAspectRatio:(float)aspectRatio {
	_machine->setup_output(aspectRatio);

	// Since OS X v10.6, Macs have had a gamma of 2.2.
	_machine->get_crt()->set_output_gamma(2.2f);
}

- (void)drawViewForPixelSize:(CGSize)pixelSize onlyIfDirty:(BOOL)onlyIfDirty {
	_machine->get_crt()->draw_frame((unsigned int)pixelSize.width, (unsigned int)pixelSize.height, onlyIfDirty ? true : false);
}

- (double)clockRate {
	return _machine->get_clock_rate();
}

- (BOOL)clockIsUnlimited {
	return _machine->get_clock_is_unlimited() ? YES : NO;
}

- (void)paste:(NSString *)paste {
	Utility::TypeRecipient *typeRecipient = dynamic_cast<Utility::TypeRecipient *>(_machine);
	if(typeRecipient)
		typeRecipient->set_typer_for_string([paste UTF8String]);
}

- (void)applyTarget:(const StaticAnalyser::Target &)target {
	@synchronized(self) {
		ConfigurationTarget::Machine *const configurationTarget =
			dynamic_cast<ConfigurationTarget::Machine *>(_machine);
		if(configurationTarget) configurationTarget->configure_as_target(target);
	}
}

- (void)applyMedia:(const StaticAnalyser::Media &)media {
	@synchronized(self) {
		ConfigurationTarget::Machine *const configurationTarget =
			dynamic_cast<ConfigurationTarget::Machine *>(_machine);
		if(configurationTarget) configurationTarget->insert_media(media);
	}
}

@end
