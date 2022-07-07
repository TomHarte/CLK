//
//  CSMachine.m
//  Clock Signal
//
//  Created by Thomas Harte on 04/01/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#import "CSMachine.h"
#import "CSMachine+Target.h"

#import "CSHighPrecisionTimer.h"
#include "CSROMFetcher.hpp"
#import "CSScanTarget+CppScanTarget.h"

#include "MediaTarget.hpp"
#include "JoystickMachine.hpp"
#include "KeyboardMachine.hpp"
#include "KeyCodes.h"
#include "MachineForTarget.hpp"
#include "StandardOptions.hpp"
#include "Typer.hpp"
#include "../../../../Activity/Observer.hpp"

#include "../../../../ClockReceiver/TimeTypes.hpp"
#include "../../../../ClockReceiver/ScanSynchroniser.hpp"
#include "../../../../Concurrency/AsyncUpdater.hpp"

#import "CSStaticAnalyser+TargetVector.h"
#import "NSBundle+DataResource.h"
#import "NSData+StdVector.h"

#include <atomic>
#include <bitset>
#include <codecvt>
#include <locale>

namespace {

struct Updater {};

Concurrency::AsyncUpdater<Updater> updater();

}

@interface CSMachine() <CSScanTargetViewDisplayLinkDelegate>
- (void)speaker:(Outputs::Speaker::Speaker *)speaker didCompleteSamples:(const int16_t *)samples length:(int)length;
- (void)speakerDidChangeInputClock:(Outputs::Speaker::Speaker *)speaker;
- (void)addLED:(NSString *)led isPersistent:(BOOL)isPersistent;
@end

struct LockProtectedDelegate {
	// Contractual promise is: machine, the pointer **and** the object **, may be accessed only
	// in sections protected by the machineAccessLock;
	NSLock *machineAccessLock;
	__unsafe_unretained CSMachine *machine;
};

struct SpeakerDelegate: public Outputs::Speaker::Speaker::Delegate, public LockProtectedDelegate {
	void speaker_did_complete_samples(Outputs::Speaker::Speaker *speaker, const std::vector<int16_t> &buffer) final {
		[machineAccessLock lock];
		[machine speaker:speaker didCompleteSamples:buffer.data() length:(int)buffer.size()];
		[machineAccessLock unlock];
	}
	void speaker_did_change_input_clock(Outputs::Speaker::Speaker *speaker) final {
		[machineAccessLock lock];
		[machine speakerDidChangeInputClock:speaker];
		[machineAccessLock unlock];
	}
};

struct ActivityObserver: public Activity::Observer {
	void register_led(const std::string &name, uint8_t flags) final {
		[machine addLED:[NSString stringWithUTF8String:name.c_str()] isPersistent:(flags & Activity::Observer::LEDPresentation::Persistent) ? YES : NO];
	}

	void set_led_status(const std::string &name, bool lit) final {
		[machine.delegate machine:machine led:[NSString stringWithUTF8String:name.c_str()] didChangeToLit:lit];
	}

	void announce_drive_event(const std::string &name, DriveEvent) final {
		[machine.delegate machine:machine ledShouldBlink:[NSString stringWithUTF8String:name.c_str()]];
	}

	__unsafe_unretained CSMachine *machine;
};

@implementation CSMachineLED

- (instancetype)initWithName:(NSString *)name isPersistent:(BOOL)isPersistent {
	self = [super init];
	if(self) {
		_name = name;
		_isPersisent = isPersistent;
	}
	return self;
}

@end

@implementation CSMachine {
	SpeakerDelegate _speakerDelegate;
	ActivityObserver _activityObserver;
	NSLock *_delegateMachineAccessLock;

	CSStaticAnalyser *_analyser;
	std::unique_ptr<Machine::DynamicMachine> _machine;
	MachineTypes::JoystickMachine *_joystickMachine;

	CSJoystickManager *_joystickManager;
	NSMutableArray<CSMachineLED *> *_leds;

	CSHighPrecisionTimer *_timer;
	std::atomic_flag _isUpdating;
	Time::Nanos _syncTime;
	Time::Nanos _timeDiff;
	double _refreshPeriod;
	BOOL _isSyncLocking;

	Time::ScanSynchroniser _scanSynchroniser;

	NSTimer *_joystickTimer;

	// This array exists to reduce blocking on the main queue; anything that would otherwise need
	// to synchronise on self in order to post input to the machine can instead synchronise on
	// _inputEvents and add a block to it. The main machine execution loop promises to synchronise
	// on _inputEvents very briefly at the start of every tick and execute all enqueued blocks.
	NSMutableArray<dispatch_block_t> *_inputEvents;
}

- (instancetype)initWithAnalyser:(CSStaticAnalyser *)result missingROMs:(inout NSMutableString *)missingROMs {
	self = [super init];
	if(self) {
		_analyser = result;

		Machine::Error error;
		ROM::Request missing_roms;
		_machine.reset(Machine::MachineForTargets(_analyser.targets, CSROMFetcher(&missing_roms), error));
		if(!_machine) {
			std::wstring_convert<std::codecvt_utf8<wchar_t>> wstring_converter;
			const std::wstring description = missing_roms.description(0, L'•');
			[missingROMs appendString:[NSString stringWithUTF8String:wstring_converter.to_bytes(description).c_str()]];
			return nil;
		}

		// Use the keyboard as a joystick if the machine has no keyboard, or if it has a 'non-exclusive' keyboard.
		_inputMode =
			(_machine->keyboard_machine() && _machine->keyboard_machine()->get_keyboard().is_exclusive())
				? CSMachineKeyboardInputModeKeyboardPhysical : CSMachineKeyboardInputModeJoystick;

		_leds = [[NSMutableArray alloc] init];
		Activity::Source *const activity_source = _machine->activity_source();
		if(activity_source) {
			_activityObserver.machine = self;
			activity_source->set_activity_observer(&_activityObserver);
		}

		_delegateMachineAccessLock = [[NSLock alloc] init];

		_speakerDelegate.machine = self;
		_speakerDelegate.machineAccessLock = _delegateMachineAccessLock;

		_inputEvents = [[NSMutableArray alloc] init];

		_joystickMachine = _machine->joystick_machine();
		[self updateJoystickTimer];
		_isUpdating.clear();
	}
	return self;
}

- (void)speaker:(Outputs::Speaker::Speaker *)speaker didCompleteSamples:(const int16_t *)samples length:(int)length {
	[self.audioQueue enqueueAudioBuffer:samples numberOfSamples:(unsigned int)length];
}

- (void)speakerDidChangeInputClock:(Outputs::Speaker::Speaker *)speaker {
	[self.delegate machineSpeakerDidChangeInputClock:self];
}

- (void)dealloc {
	[_joystickTimer invalidate];

	// The two delegate's references to this machine are nilled out here because close_output may result
	// in a data flush, which might cause an audio callback, which could cause the audio queue to decide
	// that it's out of data, resulting in an attempt further to run the machine while it is dealloc'ing.
	//
	// They are nilled inside an explicit lock because that allows the delegates to protect their entire
	// call into the machine, not just the pointer access.
	[_delegateMachineAccessLock lock];
	_speakerDelegate.machine = nil;
	[_delegateMachineAccessLock unlock];
}

- (Outputs::Speaker::Speaker *)speaker {
	const auto audio_producer = _machine->audio_producer();
	if(!audio_producer) {
		return nullptr;
	}
	return audio_producer->get_speaker();
}

- (float)idealSamplingRateFromRange:(NSRange)range {
	@synchronized(self) {
		Outputs::Speaker::Speaker *speaker = [self speaker];
		if(speaker) {
			return speaker->get_ideal_clock_rate_in_range((float)range.location, (float)(range.location + range.length));
		}
		return 0;
	}
}

- (BOOL)isStereo {
	@synchronized(self) {
		Outputs::Speaker::Speaker *speaker = [self speaker];
		if(speaker) {
			return speaker->get_is_stereo();
		}
		return NO;
	}
}

- (void)setAudioSamplingRate:(float)samplingRate bufferSize:(NSUInteger)bufferSize stereo:(BOOL)stereo {
	@synchronized(self) {
		[self setSpeakerDelegate:&_speakerDelegate sampleRate:samplingRate bufferSize:bufferSize stereo:stereo];
	}
}

- (BOOL)setSpeakerDelegate:(Outputs::Speaker::Speaker::Delegate *)delegate sampleRate:(float)sampleRate bufferSize:(NSUInteger)bufferSize stereo:(BOOL)stereo {
	@synchronized(self) {
		Outputs::Speaker::Speaker *speaker = [self speaker];
		if(speaker) {
			speaker->set_output_rate(sampleRate, (int)bufferSize, stereo);
			speaker->set_delegate(delegate);
			return YES;
		}
		return NO;
	}
}

- (void)updateJoystickTimer {
	// Joysticks updates are scheduled for a nominal 200 polls/second, using a plain old NSTimer.
	if(_joystickMachine && _joystickManager) {
		_joystickTimer = [NSTimer scheduledTimerWithTimeInterval:1.0 / 200.0 target:self selector:@selector(updateJoysticks) userInfo:nil repeats:YES];
	} else {
		[_joystickTimer invalidate];
		_joystickTimer = nil;
	}
}

- (void)updateJoysticks {
	[_joystickManager update];

	// TODO: configurable mapping from physical joypad inputs to machine inputs.
	// Until then, apply a default mapping.

	@synchronized(self) {
		size_t c = 0;
		auto &machine_joysticks = _joystickMachine->get_joysticks();
		for(CSJoystick *joystick in _joystickManager.joysticks) {
			size_t target = c % machine_joysticks.size();
			++c;

			// Post the first two analogue axes presented by the controller as horizontal and vertical inputs,
			// unless the user seems to be using a hat.
			// SDL will return a value in the range [-32768, 32767], so map from that to [0, 1.0]
			if(!joystick.hats.count || !joystick.hats[0].direction) {
				if(joystick.axes.count > 0) {
					const float x_axis = joystick.axes[0].position;
					machine_joysticks[target]->set_input(Inputs::Joystick::Input(Inputs::Joystick::Input::Type::Horizontal), x_axis);
				}
				if(joystick.axes.count > 1) {
					const float y_axis = joystick.axes[1].position;
					machine_joysticks[target]->set_input(Inputs::Joystick::Input(Inputs::Joystick::Input::Type::Vertical), y_axis);
				}
			} else {
				// Forward hats as directions; hats always override analogue inputs.
				for(CSJoystickHat *hat in joystick.hats) {
					machine_joysticks[target]->set_input(Inputs::Joystick::Input(Inputs::Joystick::Input::Type::Up), !!(hat.direction & CSJoystickHatDirectionUp));
					machine_joysticks[target]->set_input(Inputs::Joystick::Input(Inputs::Joystick::Input::Type::Down), !!(hat.direction & CSJoystickHatDirectionDown));
					machine_joysticks[target]->set_input(Inputs::Joystick::Input(Inputs::Joystick::Input::Type::Left), !!(hat.direction & CSJoystickHatDirectionLeft));
					machine_joysticks[target]->set_input(Inputs::Joystick::Input(Inputs::Joystick::Input::Type::Right), !!(hat.direction & CSJoystickHatDirectionRight));
				}
			}

			// Forward all fire buttons, mapping as a function of index.
			if(machine_joysticks[target]->get_number_of_fire_buttons()) {
				std::vector<bool> button_states((size_t)machine_joysticks[target]->get_number_of_fire_buttons());
				for(CSJoystickButton *button in joystick.buttons) {
					if(button.isPressed) button_states[(size_t)(((int)button.index - 1) % machine_joysticks[target]->get_number_of_fire_buttons())] = true;
				}
				for(size_t index = 0; index < button_states.size(); ++index) {
					machine_joysticks[target]->set_input(
						Inputs::Joystick::Input(Inputs::Joystick::Input::Type::Fire, index),
						button_states[index]);
				}
			}
		}
	}
}

- (void)setView:(CSScanTargetView *)view aspectRatio:(float)aspectRatio {
	_view = view;
	_view.displayLinkDelegate = self;
	_machine->scan_producer()->set_scan_target(_view.scanTarget.scanTarget);
}

- (void)paste:(NSString *)paste {
	auto keyboardMachine = _machine->keyboard_machine();
	if(keyboardMachine)
		keyboardMachine->type_string([paste UTF8String]);
}

- (NSBitmapImageRep *)imageRepresentation {
	return self.view.imageRepresentation;
}

- (void)applyMedia:(const Analyser::Static::Media &)media {
	@synchronized(self) {
		const auto mediaTarget = _machine->media_target();
		if(mediaTarget) mediaTarget->insert_media(media);
	}
}

- (void)setJoystickManager:(CSJoystickManager *)joystickManager {
	_joystickManager = joystickManager;
	if(_joystickMachine) {
		@synchronized(self) {
			auto &machine_joysticks = _joystickMachine->get_joysticks();
			for(const auto &joystick: machine_joysticks) {
				joystick->reset_all_inputs();
			}
		}
	}

	[self updateJoystickTimer];
}

- (void)setKey:(uint16_t)key characters:(NSString *)characters isPressed:(BOOL)isPressed {
	[self applyInputEvent:^{
		auto keyboard_machine = self->_machine->keyboard_machine();
		if(keyboard_machine && (self.inputMode != CSMachineKeyboardInputModeJoystick || !keyboard_machine->get_keyboard().is_exclusive())) {
			Inputs::Keyboard::Key mapped_key = Inputs::Keyboard::Key::Help;	// Make an innocuous default guess.
#define BIND(source, dest) case source: mapped_key = Inputs::Keyboard::Key::dest; break;
			// Connect the Carbon-era Mac keyboard scancodes to Clock Signal's 'universal' enumeration in order
			// to pass into the platform-neutral realm.
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

				BIND(VK_ANSI_Keypad0, Keypad0);		BIND(VK_ANSI_Keypad1, Keypad1);		BIND(VK_ANSI_Keypad2, Keypad2);
				BIND(VK_ANSI_Keypad3, Keypad3);		BIND(VK_ANSI_Keypad4, Keypad4);		BIND(VK_ANSI_Keypad5, Keypad5);
				BIND(VK_ANSI_Keypad6, Keypad6);		BIND(VK_ANSI_Keypad7, Keypad7);		BIND(VK_ANSI_Keypad8, Keypad8);
				BIND(VK_ANSI_Keypad9, Keypad9);

				BIND(VK_ANSI_Equal, Equals);						BIND(VK_ANSI_Minus, Hyphen);
				BIND(VK_ANSI_RightBracket, CloseSquareBracket);		BIND(VK_ANSI_LeftBracket, OpenSquareBracket);
				BIND(VK_ANSI_Quote, Quote);							BIND(VK_ANSI_Grave, BackTick);

				BIND(VK_ANSI_Semicolon, Semicolon);
				BIND(VK_ANSI_Backslash, Backslash);					BIND(VK_ANSI_Slash, ForwardSlash);
				BIND(VK_ANSI_Comma, Comma);							BIND(VK_ANSI_Period, FullStop);

				BIND(VK_ANSI_KeypadDecimal, KeypadDecimalPoint);	BIND(VK_ANSI_KeypadEquals, KeypadEquals);
				BIND(VK_ANSI_KeypadMultiply, KeypadAsterisk);		BIND(VK_ANSI_KeypadDivide, KeypadSlash);
				BIND(VK_ANSI_KeypadPlus, KeypadPlus);				BIND(VK_ANSI_KeypadMinus, KeypadMinus);
				BIND(VK_ANSI_KeypadClear, KeypadDelete);			BIND(VK_ANSI_KeypadEnter, KeypadEnter);

				BIND(VK_Return, Enter);					BIND(VK_Tab, Tab);
				BIND(VK_Space, Space);					BIND(VK_Delete, Backspace);
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
				BIND(VK_DownArrow, Down);		BIND(VK_UpArrow, Up);
			}
#undef BIND

			// Pick an ASCII code, if any.
			char pressedKey = '\0';
			if(characters.length) {
				unichar firstCharacter = [characters characterAtIndex:0];
				if(firstCharacter < 128) {
					pressedKey = (char)firstCharacter;
				}
			}

			@synchronized(self) {
				if(keyboard_machine->apply_key(mapped_key, pressedKey, isPressed, self.inputMode == CSMachineKeyboardInputModeKeyboardLogical)) {
					return;
				}
			}
		}

		auto joystick_machine = self->_machine->joystick_machine();
		if(self.inputMode == CSMachineKeyboardInputModeJoystick && joystick_machine) {
			auto &joysticks = joystick_machine->get_joysticks();
			if(!joysticks.empty()) {
				// Convert to a C++ bool so that the following calls are resolved correctly even if overloaded.
				bool is_pressed = !!isPressed;
				switch(key) {
					case VK_LeftArrow:	joysticks[0]->set_input(Inputs::Joystick::Input::Left, is_pressed);		break;
					case VK_RightArrow:	joysticks[0]->set_input(Inputs::Joystick::Input::Right, is_pressed);	break;
					case VK_UpArrow:	joysticks[0]->set_input(Inputs::Joystick::Input::Up, is_pressed);		break;
					case VK_DownArrow:	joysticks[0]->set_input(Inputs::Joystick::Input::Down, is_pressed);		break;
					case VK_Space:		joysticks[0]->set_input(Inputs::Joystick::Input::Fire, is_pressed);		break;
					case VK_ANSI_A:		joysticks[0]->set_input(Inputs::Joystick::Input(Inputs::Joystick::Input::Fire, 0), is_pressed);	break;
					case VK_ANSI_S:		joysticks[0]->set_input(Inputs::Joystick::Input(Inputs::Joystick::Input::Fire, 1), is_pressed);	break;
					case VK_ANSI_D:		joysticks[0]->set_input(Inputs::Joystick::Input(Inputs::Joystick::Input::Fire, 2), is_pressed);	break;
					case VK_ANSI_F:		joysticks[0]->set_input(Inputs::Joystick::Input(Inputs::Joystick::Input::Fire, 3), is_pressed);	break;
					default:
						if(characters.length) {
							joysticks[0]->set_input(Inputs::Joystick::Input([characters characterAtIndex:0]), is_pressed);
						} else {
							joysticks[0]->set_input(Inputs::Joystick::Input::Fire, is_pressed);
						}
					break;
				}
			}
		}
	}];
}

- (void)applyInputEvent:(dispatch_block_t)event {
	@synchronized(_inputEvents) {
		[_inputEvents addObject:event];
	}
}

- (void)clearAllKeys {
	const auto keyboard_machine = _machine->keyboard_machine();
	if(keyboard_machine) {
		[self applyInputEvent:^{
			keyboard_machine->get_keyboard().reset_all_keys();
		}];
	}

	const auto joystick_machine = _machine->joystick_machine();
	if(joystick_machine) {
		[self applyInputEvent:^{
			for(auto &joystick : joystick_machine->get_joysticks()) {
				joystick->reset_all_inputs();
			}
		}];
	}

	const auto mouse_machine = _machine->mouse_machine();
	if(mouse_machine) {
		[self applyInputEvent:^{
			mouse_machine->get_mouse().reset_all_buttons();
		}];
	}
}

- (void)setMouseButton:(int)button isPressed:(BOOL)isPressed {
	auto mouse_machine = _machine->mouse_machine();
	if(mouse_machine) {
		[self applyInputEvent:^{
			mouse_machine->get_mouse().set_button_pressed(button % mouse_machine->get_mouse().get_number_of_buttons(), isPressed);
		}];
	}
}

- (void)addMouseMotionX:(CGFloat)deltaX y:(CGFloat)deltaY {
	auto mouse_machine = _machine->mouse_machine();
	if(mouse_machine) {
		[self applyInputEvent:^{
			mouse_machine->get_mouse().move(int(deltaX), int(deltaY));
		}];
	}
}

#pragma mark - Options

- (void)setUseFastLoadingHack:(BOOL)useFastLoadingHack {
	Configurable::Device *configurable_device = _machine->configurable_device();
	if(!configurable_device) return;

	@synchronized(self) {
		_useFastLoadingHack = useFastLoadingHack;

		auto options = configurable_device->get_options();
		Reflection::set(*options, "quickload", useFastLoadingHack ? true : false);
		configurable_device->set_options(options);
	}
}

- (void)setVideoSignal:(CSMachineVideoSignal)videoSignal {
	Configurable::Device *configurable_device = _machine->configurable_device();
	if(!configurable_device) return;

	@synchronized(self) {
		_videoSignal = videoSignal;

		Configurable::Display display;
		switch(videoSignal) {
			case CSMachineVideoSignalRGB:					display = Configurable::Display::RGB;					break;
			case CSMachineVideoSignalSVideo:				display = Configurable::Display::SVideo;				break;
			case CSMachineVideoSignalComposite:				display = Configurable::Display::CompositeColour;		break;
			case CSMachineVideoSignalMonochromeComposite:	display = Configurable::Display::CompositeMonochrome;	break;
		}

		auto options = configurable_device->get_options();
		Reflection::set(*options, "output", int(display));
		configurable_device->set_options(options);
	}
}

- (BOOL)supportsVideoSignal:(CSMachineVideoSignal)videoSignal {
	Configurable::Device *configurable_device = _machine->configurable_device();
	if(!configurable_device) return NO;

	// Get the options this machine provides.
	@synchronized(self) {
		auto options = configurable_device->get_options();

		// Get the standard option for this video signal.
		Configurable::Display option;
		switch(videoSignal) {
			case CSMachineVideoSignalRGB:					option = Configurable::Display::RGB;					break;
			case CSMachineVideoSignalSVideo:				option = Configurable::Display::SVideo;					break;
			case CSMachineVideoSignalComposite:				option = Configurable::Display::CompositeColour;		break;
			case CSMachineVideoSignalMonochromeComposite:	option = Configurable::Display::CompositeMonochrome;	break;
		}

		// Map to a string and check against returned options for the 'output' field.
		const auto string_option = Reflection::Enum::to_string<Configurable::Display>(option);
		const auto all_values = options->values_for("output");

		return std::find(all_values.begin(), all_values.end(), string_option) != all_values.end();
	}

	return NO;
}

- (void)setUseAutomaticTapeMotorControl:(BOOL)useAutomaticTapeMotorControl {
	Configurable::Device *configurable_device = _machine->configurable_device();
	if(!configurable_device) return;

	@synchronized(self) {
		_useAutomaticTapeMotorControl = useAutomaticTapeMotorControl;

		auto options = configurable_device->get_options();
		Reflection::set(*options, "automatic_tape_motor_control", useAutomaticTapeMotorControl ? true : false);
		configurable_device->set_options(options);
	}
}

- (void)setUseQuickBootingHack:(BOOL)useQuickBootingHack {
	Configurable::Device *configurable_device = _machine->configurable_device();
	if(!configurable_device) return;

	@synchronized(self) {
		_useQuickBootingHack = useQuickBootingHack;

		auto options = configurable_device->get_options();
		Reflection::set(*options, "quickboot", useQuickBootingHack ? true : false);
		configurable_device->set_options(options);
	}
}

- (NSString *)userDefaultsPrefix {
	// Assumes that the first machine in the targets list is the source of user defaults.
	std::string name = Machine::ShortNameForTargetMachine(_analyser.targets.front()->machine);
	return [[NSString stringWithUTF8String:name.c_str()] lowercaseString];
}

- (BOOL)canInsertMedia {
	return !!_machine->media_target();
}

#pragma mark - Special machines

- (CSAtari2600 *)atari2600 {
	return [[CSAtari2600 alloc] initWithAtari2600:_machine->raw_pointer() owner:self];
}

- (CSZX8081 *)zx8081 {
	return [[CSZX8081 alloc] initWithZX8081:_machine->raw_pointer() owner:self];
}

- (CSAppleII *)appleII {
	return [[CSAppleII alloc] initWithAppleII:_machine->raw_pointer() owner:self];
}

#pragma mark - Input device queries

- (BOOL)hasJoystick {
	return !!_machine->joystick_machine();
}

- (BOOL)hasMouse {
	return !!_machine->mouse_machine();
}

- (BOOL)hasExclusiveKeyboard {
	return !!_machine->keyboard_machine() && _machine->keyboard_machine()->get_keyboard().is_exclusive();
}

- (BOOL)shouldUsurpCommand {
	if(!_machine->keyboard_machine()) return NO;

	const auto essential_modifiers = _machine->keyboard_machine()->get_keyboard().get_essential_modifiers();
	return	essential_modifiers.find(Inputs::Keyboard::Key::LeftMeta) != essential_modifiers.end() ||
			essential_modifiers.find(Inputs::Keyboard::Key::RightMeta) != essential_modifiers.end();
}

#pragma mark - Volume control

- (void)setVolume:(float)volume {
	@synchronized(self) {
		Outputs::Speaker::Speaker *speaker = [self speaker];
		if(speaker) {
			return speaker->set_output_volume(volume);
		}
	}
}

- (BOOL)hasAudioOutput {
	@synchronized(self) {
		Outputs::Speaker::Speaker *speaker = [self speaker];
		return speaker ? YES : NO;
	}
}

#pragma mark - Activity observation

- (void)addLED:(NSString *)led isPersistent:(BOOL)isPersistent {
	[_leds addObject:[[CSMachineLED alloc] initWithName:led isPersistent:isPersistent]];
}

- (NSArray<CSMachineLED *> *)leds {
	return _leds;
}

#pragma mark - Timer

- (void)scanTargetViewDisplayLinkDidFire:(CSScanTargetView *)view now:(const CVTimeStamp *)now outputTime:(const CVTimeStamp *)outputTime {
	// First order of business: grab a timestamp.
	const auto timeNow = Time::nanos_now();

	BOOL isSyncLocking;
	@synchronized(self) {
		// Store a means to map from CVTimeStamp.hostTime to Time::Nanos;
		// there is an extremely dodgy assumption here that the former is in ns.
		// If you can find a well-defined way to get the CVTimeStamp.hostTime units,
		// whether at runtime or via preprocessor define, I'd love to know about it.
		if(!_timeDiff) {
			_timeDiff = int64_t(timeNow) - int64_t(now->hostTime);
		}

		// Store the next end-of-frame time. TODO: and start of next and implied visible duration, if raster racing?
		_syncTime = int64_t(now->hostTime) + _timeDiff;

		// Set the current refresh period.
		_refreshPeriod = double(now->videoRefreshPeriod) / double(now->videoTimeScale);

		// Determine where responsibility lies for drawing.
		isSyncLocking = _isSyncLocking;
	}

	// Draw the current output. (TODO: do this within the timer if either raster racing or, at least, sync matching).
	if(!isSyncLocking) {
		[self.view draw];
	}
}

#define TICKS	1000

- (void)start {
	__block auto lastTime = Time::nanos_now();

	_timer = [[CSHighPrecisionTimer alloc] initWithTask:^{
		// Grab the time now and, therefore, the amount of time since the timer last fired
		// (subject to a cap to avoid potential perpetual regression).
		const auto timeNow = Time::nanos_now();
		lastTime = std::max(timeNow - Time::Nanos(10'000'000'000 / TICKS), lastTime);
		const auto duration = timeNow - lastTime;

		BOOL splitAndSync = NO;
		@synchronized(self) {
			// Post on input events.
			@synchronized(self->_inputEvents) {
				for(dispatch_block_t action: self->_inputEvents) {
					action();
				}
				[self->_inputEvents removeAllObjects];
			}

			// If this tick includes vsync then inspect the machine.
			//
			// _syncTime = 0 is used here as a sentinel to mark that a sync time is known;
			// this with the >= test ensures that no syncs are missed even if some sort of
			// performance problem is afoot (e.g. I'm debugging).
			if(self->_syncTime && timeNow >= self->_syncTime) {
				splitAndSync = self->_isSyncLocking = self->_scanSynchroniser.can_synchronise(self->_machine->scan_producer()->get_scan_status(), self->_refreshPeriod);

				// If the time window is being split, run up to the split, then check out machine speed, possibly
				// adjusting multiplier, then run after the split. Include a sanity check against an out-of-bounds
				// _syncTime; that can happen when debugging (possibly inter alia?).
				if(splitAndSync) {
					if(self->_syncTime >= lastTime) {
						self->_machine->timed_machine()->run_for((double)(self->_syncTime - lastTime) / 1e9);
						self->_machine->timed_machine()->set_speed_multiplier(
							self->_scanSynchroniser.next_speed_multiplier(self->_machine->scan_producer()->get_scan_status())
						);
						self->_machine->timed_machine()->run_for((double)(timeNow - self->_syncTime) / 1e9);
					} else {
						self->_machine->timed_machine()->run_for((double)(timeNow - lastTime) / 1e9);
					}
				}

				self->_syncTime = 0;
			}

			// If the time window is being split, run up to the split, then check out machine speed, possibly
			// adjusting multiplier, then run after the split.
			if(!splitAndSync) {
				self->_machine->timed_machine()->run_for((double)duration / 1e9);
			}
		}

		// If this was not a split-and-sync then dispatch the update request asynchronously, unless
		// there is an earlier one not yet finished, in which case don't worry about it for now.
		//
		// If it was a split-and-sync then spin until it is safe to dispatch, and dispatch with
		// a concluding draw. Implicit assumption here: whatever is left to be done in the final window
		// can be done within the retrace period.
		auto wasUpdating = self->_isUpdating.test_and_set();
		if(wasUpdating && splitAndSync) {
			while(self->_isUpdating.test_and_set());
			wasUpdating = false;
		}
		if(!wasUpdating) {
			dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
				[self.view updateBacking];
				if(splitAndSync) {
					[self.view draw];
				}
				self->_isUpdating.clear();
			});
		}

		lastTime = timeNow;
	} interval:uint64_t(1000000000) / uint64_t(TICKS)];
}

#undef TICKS

- (void)stop {
	[_timer invalidate];
	_timer = nil;
}

+ (BOOL)attemptInstallROM:(NSURL *)url {
	return CSInstallROM(url);
}

@end
