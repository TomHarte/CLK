//
//  CSMachine.m
//  Clock Signal
//
//  Created by Thomas Harte on 04/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import "CSMachine.h"
#import "CSMachine+Target.h"

#include "CSROMFetcher.hpp"

#include "ConfigurationTarget.hpp"
#include "JoystickMachine.hpp"
#include "KeyboardMachine.hpp"
#include "KeyCodes.h"
#include "MachineForTarget.hpp"
#include "StandardOptions.hpp"
#include "Typer.hpp"

#import "CSStaticAnalyser+TargetVector.h"
#import "NSBundle+DataResource.h"
#import "NSData+StdVector.h"

@interface CSMachine() <CSFastLoading>
- (void)speaker:(Outputs::Speaker::Speaker *)speaker didCompleteSamples:(const int16_t *)samples length:(int)length;
@end

struct LockProtectedDelegate {
	// Contractual promise is: machine — the pointer **and** the object ** — may be accessed only
	// in sections protected by the machineAccessLock;
	NSLock *machineAccessLock;
	__unsafe_unretained CSMachine *machine;
};

struct SpeakerDelegate: public Outputs::Speaker::Speaker::Delegate, public LockProtectedDelegate {
	void speaker_did_complete_samples(Outputs::Speaker::Speaker *speaker, const std::vector<int16_t> &buffer) {
		[machineAccessLock lock];
		[machine speaker:speaker didCompleteSamples:buffer.data() length:(int)buffer.size()];
		[machineAccessLock unlock];
	}
};

@implementation CSMachine {
	SpeakerDelegate _speakerDelegate;
	NSLock *_delegateMachineAccessLock;

	CSStaticAnalyser *_analyser;
	std::unique_ptr<Machine::DynamicMachine> _machine;
}

- (instancetype)initWithAnalyser:(CSStaticAnalyser *)result {
	self = [super init];
	if(self) {
		_analyser = result;

		Machine::Error error;
		_machine.reset(Machine::MachineForTargets(_analyser.targets, CSROMFetcher(), error));
		if(!_machine) return nil;

		_delegateMachineAccessLock = [[NSLock alloc] init];

		_speakerDelegate.machine = self;
		_speakerDelegate.machineAccessLock = _delegateMachineAccessLock;
	}
	return self;
}

- (void)speaker:(Outputs::Speaker::Speaker *)speaker didCompleteSamples:(const int16_t *)samples length:(int)length {
	[self.audioQueue enqueueAudioBuffer:samples numberOfSamples:(unsigned int)length];
}

- (void)dealloc {
	// The two delegate's references to this machine are nilled out here because close_output may result
	// in a data flush, which might cause an audio callback, which could cause the audio queue to decide
	// that it's out of data, resulting in an attempt further to run the machine while it is dealloc'ing.
	//
	// They are nilled inside an explicit lock because that allows the delegates to protect their entire
	// call into the machine, not just the pointer access.
	[_delegateMachineAccessLock lock];
	_speakerDelegate.machine = nil;
	[_delegateMachineAccessLock unlock];

	[_view performWithGLContext:^{
		@synchronized(self) {
			_machine->crt_machine()->close_output();
		}
	}];
}

- (float)idealSamplingRateFromRange:(NSRange)range {
	@synchronized(self) {
		Outputs::Speaker::Speaker *speaker = _machine->crt_machine()->get_speaker();
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

- (BOOL)setSpeakerDelegate:(Outputs::Speaker::Speaker::Delegate *)delegate sampleRate:(float)sampleRate bufferSize:(NSUInteger)bufferSize {
	@synchronized(self) {
		Outputs::Speaker::Speaker *speaker = _machine->crt_machine()->get_speaker();
		if(speaker) {
			speaker->set_output_rate(sampleRate, (int)bufferSize);
			speaker->set_delegate(delegate);
			return YES;
		}
		return NO;
	}
}

- (void)runForInterval:(NSTimeInterval)interval {
	@synchronized(self) {
		_machine->crt_machine()->run_for(interval);
	}
}

- (void)setView:(CSOpenGLView *)view aspectRatio:(float)aspectRatio {
	_view = view;
	[view performWithGLContext:^{
		[self setupOutputWithAspectRatio:aspectRatio];
	}];
}

- (void)setupOutputWithAspectRatio:(float)aspectRatio {
	_machine->crt_machine()->setup_output(aspectRatio);

	// Since OS X v10.6, Macs have had a gamma of 2.2.
	_machine->crt_machine()->get_crt()->set_output_gamma(2.2f);
	_machine->crt_machine()->get_crt()->set_target_framebuffer(0);
}

- (void)drawViewForPixelSize:(CGSize)pixelSize onlyIfDirty:(BOOL)onlyIfDirty {
	_machine->crt_machine()->get_crt()->draw_frame((unsigned int)pixelSize.width, (unsigned int)pixelSize.height, onlyIfDirty ? true : false);
}

- (double)clockRate {
	return _machine->crt_machine()->get_clock_rate();
}

- (void)paste:(NSString *)paste {
	KeyboardMachine::Machine *keyboardMachine = _machine->keyboard_machine();
	if(keyboardMachine)
		keyboardMachine->type_string([paste UTF8String]);
}

- (void)applyMedia:(const Analyser::Static::Media &)media {
	@synchronized(self) {
		ConfigurationTarget::Machine *const configurationTarget = _machine->configuration_target();
		if(configurationTarget) configurationTarget->insert_media(media);
	}
}

- (void)setKey:(uint16_t)key characters:(NSString *)characters isPressed:(BOOL)isPressed {
	auto keyboard_machine = _machine->keyboard_machine();
	if(keyboard_machine) {
		@synchronized(self) {
			Inputs::Keyboard &keyboard = keyboard_machine->get_keyboard();

			// Connect the Carbon-era Mac keyboard scancodes to Clock Signal's 'universal' enumeration in order
			// to pass into the platform-neutral realm.
#define BIND(source, dest) case source: keyboard.set_key_pressed(Inputs::Keyboard::Key::dest, isPressed);	break
			switch(key) {
				BIND(VK_ANSI_0, k0);	BIND(VK_ANSI_1, k1);	BIND(VK_ANSI_2, k2);	BIND(VK_ANSI_3, k3);	BIND(VK_ANSI_4, k4);
				BIND(VK_ANSI_5, k5);	BIND(VK_ANSI_6, k6);	BIND(VK_ANSI_7, k7);	BIND(VK_ANSI_8, k8);	BIND(VK_ANSI_9, k9);

				BIND(VK_ANSI_Q, Q);		BIND(VK_ANSI_W, W);		BIND(VK_ANSI_E, E);		BIND(VK_ANSI_R, R);		BIND(VK_ANSI_T, T);
				BIND(VK_ANSI_Y, Y);		BIND(VK_ANSI_U, U);		BIND(VK_ANSI_I, I);		BIND(VK_ANSI_O, O);		BIND(VK_ANSI_P, P);

				BIND(VK_ANSI_A, A);		BIND(VK_ANSI_S, S);		BIND(VK_ANSI_D, D);		BIND(VK_ANSI_F, F);		BIND(VK_ANSI_G, G);
				BIND(VK_ANSI_H, H);		BIND(VK_ANSI_J, J);		BIND(VK_ANSI_K, K);		BIND(VK_ANSI_L, L);

				BIND(VK_ANSI_Z, Z);		BIND(VK_ANSI_X, X);		BIND(VK_ANSI_C, C);		BIND(VK_ANSI_V, V);
				BIND(VK_ANSI_B, B);		BIND(VK_ANSI_N, N);		BIND(VK_ANSI_M, M);

				BIND(VK_F1, F1);		BIND(VK_F2, F2);		BIND(VK_F3, F3);		BIND(VK_F4, F4);
				BIND(VK_F5, F5);		BIND(VK_F6, F6);		BIND(VK_F7, F7);		BIND(VK_F8, F8);
				BIND(VK_F9, F9);		BIND(VK_F10, F10);		BIND(VK_F11, F11);		BIND(VK_F12, F12);

				BIND(VK_ANSI_Keypad0, KeyPad0);		BIND(VK_ANSI_Keypad1, KeyPad1);		BIND(VK_ANSI_Keypad2, KeyPad2);
				BIND(VK_ANSI_Keypad3, KeyPad3);		BIND(VK_ANSI_Keypad4, KeyPad4);		BIND(VK_ANSI_Keypad5, KeyPad5);
				BIND(VK_ANSI_Keypad6, KeyPad6);		BIND(VK_ANSI_Keypad7, KeyPad7);		BIND(VK_ANSI_Keypad8, KeyPad8);
				BIND(VK_ANSI_Keypad9, KeyPad9);

				BIND(VK_ANSI_Equal, Equals);						BIND(VK_ANSI_Minus, Hyphen);
				BIND(VK_ANSI_RightBracket, CloseSquareBracket);		BIND(VK_ANSI_LeftBracket, OpenSquareBracket);
				BIND(VK_ANSI_Quote, Quote);							BIND(VK_ANSI_Grave, BackTick);

				BIND(VK_ANSI_Semicolon, Semicolon);
				BIND(VK_ANSI_Backslash, BackSlash);					BIND(VK_ANSI_Slash, ForwardSlash);
				BIND(VK_ANSI_Comma, Comma);							BIND(VK_ANSI_Period, FullStop);

				BIND(VK_ANSI_KeypadDecimal, KeyPadDecimalPoint);	BIND(VK_ANSI_KeypadEquals, KeyPadEquals);
				BIND(VK_ANSI_KeypadMultiply, KeyPadAsterisk);		BIND(VK_ANSI_KeypadDivide, KeyPadSlash);
				BIND(VK_ANSI_KeypadPlus, KeyPadPlus);				BIND(VK_ANSI_KeypadMinus, KeyPadMinus);
				BIND(VK_ANSI_KeypadClear, KeyPadDelete);			BIND(VK_ANSI_KeypadEnter, KeyPadEnter);

				BIND(VK_Return, Enter);					BIND(VK_Tab, Tab);
				BIND(VK_Space, Space);					BIND(VK_Delete, BackSpace);
				BIND(VK_Control, LeftControl);			BIND(VK_Option, LeftOption);
				BIND(VK_Command, LeftMeta);				BIND(VK_Shift, LeftShift);
				BIND(VK_RightControl, RightControl);	BIND(VK_RightOption, RightOption);
				BIND(VK_Escape, Escape);				BIND(VK_CapsLock, CapsLock);
				BIND(VK_Home, Home);					BIND(VK_End, End);
				BIND(VK_PageUp, PageUp);				BIND(VK_PageDown, PageDown);

				BIND(VK_RightShift, RightShift);
				BIND(VK_Help, Help);
				BIND(VK_ForwardDelete, Delete);

				BIND(VK_LeftArrow, Left);		BIND(VK_RightArrow, Right);
				BIND(VK_DownArrow, Down); 		BIND(VK_UpArrow, Up);
			}
#undef BIND
		}
		return;
	}

	auto joystick_machine = _machine->joystick_machine();
	if(joystick_machine) {
		@synchronized(self) {
			std::vector<std::unique_ptr<Inputs::Joystick>> &joysticks = joystick_machine->get_joysticks();
			if(!joysticks.empty()) {
				switch(key) {
					case VK_LeftArrow:	joysticks[0]->set_digital_input(Inputs::Joystick::DigitalInput::Left, isPressed);	break;
					case VK_RightArrow:	joysticks[0]->set_digital_input(Inputs::Joystick::DigitalInput::Right, isPressed);	break;
					case VK_UpArrow:	joysticks[0]->set_digital_input(Inputs::Joystick::DigitalInput::Up, isPressed);		break;
					case VK_DownArrow:	joysticks[0]->set_digital_input(Inputs::Joystick::DigitalInput::Down, isPressed);	break;
					case VK_Space:		joysticks[0]->set_digital_input(Inputs::Joystick::DigitalInput::Fire, isPressed);	break;
					case VK_ANSI_A:		joysticks[0]->set_digital_input(Inputs::Joystick::DigitalInput(Inputs::Joystick::DigitalInput::Fire, 0), isPressed);	break;
					case VK_ANSI_S:		joysticks[0]->set_digital_input(Inputs::Joystick::DigitalInput(Inputs::Joystick::DigitalInput::Fire, 1), isPressed);	break;
					default:
						if(characters) {
							joysticks[0]->set_digital_input(Inputs::Joystick::DigitalInput([characters characterAtIndex:0]), isPressed);
						} else {
							joysticks[0]->set_digital_input(Inputs::Joystick::DigitalInput::Fire, isPressed);
						}
					break;
				}
			}
		}
	}
}

- (void)clearAllKeys {
	auto keyboard_machine = _machine->keyboard_machine();
	if(keyboard_machine) {
		@synchronized(self) {
			keyboard_machine->get_keyboard().reset_all_keys();
		}
	}

	auto joystick_machine = _machine->joystick_machine();
	if(joystick_machine) {
		@synchronized(self) {
			for(auto &joystick : joystick_machine->get_joysticks()) {
				joystick->reset_all_inputs();
			}
		}
	}
}

#pragma mark - Options

- (void)setUseFastLoadingHack:(BOOL)useFastLoadingHack {
	Configurable::Device *configurable_device = _machine->configurable_device();
	if(!configurable_device) return;

	@synchronized(self) {
		_useFastLoadingHack = useFastLoadingHack;

		Configurable::SelectionSet selection_set;
		append_quick_load_tape_selection(selection_set, useFastLoadingHack ? true : false);
		configurable_device->set_selections(selection_set);
	}
}

- (void)setUseCompositeOutput:(BOOL)useCompositeOutput {
	Configurable::Device *configurable_device = _machine->configurable_device();
	if(!configurable_device) return;

	@synchronized(self) {
		_useCompositeOutput = useCompositeOutput;

		Configurable::SelectionSet selection_set;
		append_display_selection(selection_set, useCompositeOutput ? Configurable::Display::Composite : Configurable::Display::RGB);
		configurable_device->set_selections(selection_set);
	}
}

- (void)setUseAutomaticTapeMotorControl:(BOOL)useAutomaticTapeMotorControl {
	Configurable::Device *configurable_device = _machine->configurable_device();
	if(!configurable_device) return;

	@synchronized(self) {
		_useAutomaticTapeMotorControl = useAutomaticTapeMotorControl;

		Configurable::SelectionSet selection_set;
		append_automatic_tape_motor_control_selection(selection_set, useAutomaticTapeMotorControl ? true : false);
		configurable_device->set_selections(selection_set);
	}
}

- (NSString *)userDefaultsPrefix {
	// Assumes that the first machine in the targets list is the source of user defaults.
	std::string name = Machine::ShortNameForTargetMachine(_analyser.targets.front()->machine);
	return [[NSString stringWithUTF8String:name.c_str()] lowercaseString];
}

#pragma mark - Special machines

- (CSAtari2600 *)atari2600 {
	return [[CSAtari2600 alloc] initWithAtari2600:_machine->raw_pointer() owner:self];
}

- (CSZX8081 *)zx8081 {
	return [[CSZX8081 alloc] initWithZX8081:_machine->raw_pointer() owner:self];
}

@end
