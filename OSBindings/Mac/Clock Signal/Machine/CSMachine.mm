//
//  CSMachine.m
//  Clock Signal
//
//  Created by Thomas Harte on 04/01/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#import "CSMachine.h"
#import "CSMachine+Target.h"

#include "CSROMFetcher.hpp"
#import "CSHighPrecisionTimer.h"

#include "MediaTarget.hpp"
#include "JoystickMachine.hpp"
#include "KeyboardMachine.hpp"
#include "KeyCodes.h"
#include "MachineForTarget.hpp"
#include "StandardOptions.hpp"
#include "Typer.hpp"
#include "../../../../Activity/Observer.hpp"

#import "CSStaticAnalyser+TargetVector.h"
#import "NSBundle+DataResource.h"
#import "NSData+StdVector.h"

#include <atomic>
#include <bitset>

#import <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>

#include "../../../../Outputs/OpenGL/ScanTarget.hpp"
#include "../../../../Outputs/OpenGL/Screenshot.hpp"

@interface CSMachine() <CSOpenGLViewDisplayLinkDelegate>
- (void)speaker:(Outputs::Speaker::Speaker *)speaker didCompleteSamples:(const int16_t *)samples length:(int)length;
- (void)speakerDidChangeInputClock:(Outputs::Speaker::Speaker *)speaker;
- (void)addLED:(NSString *)led;
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
	void register_led(const std::string &name) final {
		[machine addLED:[NSString stringWithUTF8String:name.c_str()]];
	}

	void set_led_status(const std::string &name, bool lit) final {
		[machine.delegate machine:machine led:[NSString stringWithUTF8String:name.c_str()] didChangeToLit:lit];
	}

	void announce_drive_event(const std::string &name, DriveEvent event) final {
		[machine.delegate machine:machine ledShouldBlink:[NSString stringWithUTF8String:name.c_str()]];
	}

	__unsafe_unretained CSMachine *machine;
};

@interface CSMissingROM (Mutability)
@property (nonatomic, nonnull, copy) NSString *machineName;
@property (nonatomic, nonnull, copy) NSString *fileName;
@property (nonatomic, nullable, copy) NSString *descriptiveName;
@property (nonatomic, readwrite) NSUInteger size;
@property (nonatomic, copy) NSArray<NSNumber *> *crc32s;
@end

@implementation CSMissingROM {
	NSString *_machineName;
	NSString *_fileName;
	NSString *_descriptiveName;
	NSUInteger _size;
	NSArray<NSNumber *> *_crc32s;
}

- (NSString *)machineName {
	return _machineName;
}

- (void)setMachineName:(NSString *)machineName {
	_machineName = [machineName copy];
}

- (NSString *)fileName {
	return _fileName;
}

- (void)setFileName:(NSString *)fileName {
	_fileName = [fileName copy];
}

- (NSString *)descriptiveName {
	return _descriptiveName;
}

- (void)setDescriptiveName:(NSString *)descriptiveName {
	_descriptiveName = [descriptiveName copy];
}

- (NSUInteger)size {
	return _size;
}

- (void)setSize:(NSUInteger)size {
	_size = size;
}

- (NSArray<NSNumber *> *)crc32s {
	return _crc32s;
}

- (void)setCrc32s:(NSArray<NSNumber *> *)crc32s {
	_crc32s = [crc32s copy];
}

- (NSString *)description {
	return [NSString stringWithFormat:@"%@/%@, %lu bytes, CRCs: %@", _fileName, _descriptiveName, (unsigned long)_size, _crc32s];
}

@end

@implementation CSMachine {
	SpeakerDelegate _speakerDelegate;
	ActivityObserver _activityObserver;
	NSLock *_delegateMachineAccessLock;

	CSStaticAnalyser *_analyser;
	std::unique_ptr<Machine::DynamicMachine> _machine;
	JoystickMachine::Machine *_joystickMachine;

	CSJoystickManager *_joystickManager;
	std::bitset<65536> _depressedKeys;
	NSMutableArray<NSString *> *_leds;

	CSHighPrecisionTimer *_timer;
	CGSize _pixelSize;
	std::atomic_flag is_updating;

	std::unique_ptr<Outputs::Display::OpenGL::ScanTarget> _scanTarget;
}

- (instancetype)initWithAnalyser:(CSStaticAnalyser *)result missingROMs:(inout NSMutableArray<CSMissingROM *> *)missingROMs {
	self = [super init];
	if(self) {
		_analyser = result;

		Machine::Error error;
		std::vector<ROMMachine::ROM> missing_roms;
		_machine.reset(Machine::MachineForTargets(_analyser.targets, CSROMFetcher(&missing_roms), error));
		if(!_machine) {
			for(const auto &missing_rom : missing_roms) {
				CSMissingROM *rom = [[CSMissingROM alloc] init];

				// Copy/convert the primitive fields.
				rom.machineName = [NSString stringWithUTF8String:missing_rom.machine_name.c_str()];
				rom.fileName = [NSString stringWithUTF8String:missing_rom.file_name.c_str()];
				rom.descriptiveName = missing_rom.descriptive_name.empty() ? nil : [NSString stringWithUTF8String:missing_rom.descriptive_name.c_str()];
				rom.size = missing_rom.size;

				// Convert the CRC list.
				NSMutableArray<NSNumber *> *crc32s = [[NSMutableArray alloc] init];
				for(const auto &crc : missing_rom.crc32s) {
					[crc32s addObject:@(crc)];
				}
				rom.crc32s = [crc32s copy];

				// Add to the missing list.
				[missingROMs addObject:rom];
			}

			return nil;
		}

		_inputMode =
			(_machine->keyboard_machine() && _machine->keyboard_machine()->get_keyboard().is_exclusive())
				? CSMachineKeyboardInputModeKeyboard : CSMachineKeyboardInputModeJoystick;

		_leds = [[NSMutableArray alloc] init];
		Activity::Source *const activity_source = _machine->activity_source();
		if(activity_source) {
			_activityObserver.machine = self;
			activity_source->set_activity_observer(&_activityObserver);
		}

		_delegateMachineAccessLock = [[NSLock alloc] init];

		_speakerDelegate.machine = self;
		_speakerDelegate.machineAccessLock = _delegateMachineAccessLock;

		_joystickMachine = _machine->joystick_machine();
		is_updating.clear();
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
			self->_scanTarget.reset();
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

- (NSTimeInterval)runForInterval:(NSTimeInterval)interval untilEvent:(int)events {
	@synchronized(self) {
		if(_joystickMachine && _joystickManager) {
			[_joystickManager update];

			// TODO: configurable mapping from physical joypad inputs to machine inputs.
			// Until then, apply a default mapping.

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
		return _machine->crt_machine()->run_until(interval, events);
	}
}

- (void)setView:(CSOpenGLView *)view aspectRatio:(float)aspectRatio {
	_view = view;
	_view.displayLinkDelegate = self;
	[view performWithGLContext:^{
		[self setupOutputWithAspectRatio:aspectRatio];
	} flushDrawable:NO];
}

- (void)setupOutputWithAspectRatio:(float)aspectRatio {
	_scanTarget = std::make_unique<Outputs::Display::OpenGL::ScanTarget>();
	_machine->crt_machine()->set_scan_target(_scanTarget.get());
}

- (void)updateViewForPixelSize:(CGSize)pixelSize {
//	_pixelSize = pixelSize;

//	@synchronized(self) {
//		const auto scan_status = _machine->crt_machine()->get_scan_status();
//		NSLog(@"FPS (hopefully): %0.2f [retrace: %0.4f]", 1.0f / scan_status.field_duration, scan_status.retrace_duration);
//	}
}

- (void)drawViewForPixelSize:(CGSize)pixelSize {
	_scanTarget->draw((int)pixelSize.width, (int)pixelSize.height);
}

- (void)paste:(NSString *)paste {
	KeyboardMachine::Machine *keyboardMachine = _machine->keyboard_machine();
	if(keyboardMachine)
		keyboardMachine->type_string([paste UTF8String]);
}

- (NSBitmapImageRep *)imageRepresentation {
	// Grab a screenshot.
	Outputs::Display::OpenGL::Screenshot screenshot(4, 3);

	// Generate an NSBitmapImageRep containing the screenshot's data.
	NSBitmapImageRep *const result =
		[[NSBitmapImageRep alloc]
			initWithBitmapDataPlanes:NULL
			pixelsWide:screenshot.width
			pixelsHigh:screenshot.height
			bitsPerSample:8
			samplesPerPixel:4
			hasAlpha:YES
			isPlanar:NO
			colorSpaceName:NSDeviceRGBColorSpace
			bytesPerRow:4 * screenshot.width
			bitsPerPixel:0];

	memcpy(result.bitmapData, screenshot.pixel_data.data(), size_t(screenshot.width*screenshot.height*4));

	return result;
}

- (void)applyMedia:(const Analyser::Static::Media &)media {
	@synchronized(self) {
		MediaTarget::Machine *const mediaTarget = _machine->media_target();
		if(mediaTarget) mediaTarget->insert_media(media);
	}
}

- (void)setJoystickManager:(CSJoystickManager *)joystickManager {
	@synchronized(self) {
		_joystickManager = joystickManager;
		if(_joystickMachine) {
			auto &machine_joysticks = _joystickMachine->get_joysticks();
			for(const auto &joystick: machine_joysticks) {
				joystick->reset_all_inputs();
			}
		}
	}
}

- (void)setKey:(uint16_t)key characters:(NSString *)characters isPressed:(BOOL)isPressed {
	auto keyboard_machine = _machine->keyboard_machine();
	if(keyboard_machine && (self.inputMode == CSMachineKeyboardInputModeKeyboard || !keyboard_machine->get_keyboard().is_exclusive())) {
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

		Inputs::Keyboard &keyboard = keyboard_machine->get_keyboard();

		if(keyboard.observed_keys().find(mapped_key) != keyboard.observed_keys().end()) {
			// Don't pass anything on if this is not new information.
			if(_depressedKeys[key] == !!isPressed) return;
			_depressedKeys[key] = !!isPressed;

			// Pick an ASCII code, if any.
			char pressedKey = '\0';
			if(characters.length) {
				unichar firstCharacter = [characters characterAtIndex:0];
				if(firstCharacter < 128) {
					pressedKey = (char)firstCharacter;
				}
			}

			@synchronized(self) {
				keyboard.set_key_pressed(mapped_key, pressedKey, isPressed);
			}
			return;
		}
	}

	auto joystick_machine = _machine->joystick_machine();
	if(self.inputMode == CSMachineKeyboardInputModeJoystick && joystick_machine) {
		@synchronized(self) {
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
	}
}

- (void)clearAllKeys {
	const auto keyboard_machine = _machine->keyboard_machine();
	if(keyboard_machine) {
		@synchronized(self) {
			keyboard_machine->get_keyboard().reset_all_keys();
		}
	}

	const auto joystick_machine = _machine->joystick_machine();
	if(joystick_machine) {
		@synchronized(self) {
			for(auto &joystick : joystick_machine->get_joysticks()) {
				joystick->reset_all_inputs();
			}
		}
	}

	const auto mouse_machine = _machine->mouse_machine();
	if(mouse_machine) {
		@synchronized(self) {
			mouse_machine->get_mouse().reset_all_buttons();
		}
	}
}

- (void)setMouseButton:(int)button isPressed:(BOOL)isPressed {
	auto mouse_machine = _machine->mouse_machine();
	if(mouse_machine) {
		@synchronized(self) {
			mouse_machine->get_mouse().set_button_pressed(button % mouse_machine->get_mouse().get_number_of_buttons(), isPressed);
		}
	}
}

- (void)addMouseMotionX:(CGFloat)deltaX y:(CGFloat)deltaY {
	auto mouse_machine = _machine->mouse_machine();
	if(mouse_machine) {
		@synchronized(self) {
			mouse_machine->get_mouse().move(int(deltaX), int(deltaY));
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

- (void)setVideoSignal:(CSMachineVideoSignal)videoSignal {
	Configurable::Device *configurable_device = _machine->configurable_device();
	if(!configurable_device) return;

	@synchronized(self) {
		_videoSignal = videoSignal;

		Configurable::SelectionSet selection_set;
		Configurable::Display display;
		switch(videoSignal) {
			case CSMachineVideoSignalRGB:					display = Configurable::Display::RGB;					break;
			case CSMachineVideoSignalSVideo:				display = Configurable::Display::SVideo;				break;
			case CSMachineVideoSignalComposite:				display = Configurable::Display::CompositeColour;		break;
			case CSMachineVideoSignalMonochromeComposite:	display = Configurable::Display::CompositeMonochrome;	break;
		}
		append_display_selection(selection_set, display);
		configurable_device->set_selections(selection_set);
	}
}

- (bool)supportsVideoSignal:(CSMachineVideoSignal)videoSignal {
	Configurable::Device *configurable_device = _machine->configurable_device();
	if(!configurable_device) return NO;

	// Get the options this machine provides.
	std::vector<std::unique_ptr<Configurable::Option>> options;
	@synchronized(self) {
		options = configurable_device->get_options();
	}

	// Get the standard option for this video signal.
	Configurable::StandardOptions option;
	switch(videoSignal) {
		case CSMachineVideoSignalRGB:					option = Configurable::DisplayRGB;					break;
		case CSMachineVideoSignalSVideo:				option = Configurable::DisplaySVideo;				break;
		case CSMachineVideoSignalComposite:				option = Configurable::DisplayCompositeColour;		break;
		case CSMachineVideoSignalMonochromeComposite:	option = Configurable::DisplayCompositeMonochrome;	break;
	}
	std::unique_ptr<Configurable::Option> display_option = std::move(standard_options(option).front());
	Configurable::ListOption *display_list_option = dynamic_cast<Configurable::ListOption *>(display_option.get());
	NSAssert(display_list_option, @"Expected display option to be a list");

	// See whether the video signal is included in the machine options.
	for(auto &candidate: options) {
		Configurable::ListOption *list_option = dynamic_cast<Configurable::ListOption *>(candidate.get());

		// Both should be list options
		if(!list_option) continue;

		// Check for same name of option.
		if(candidate->short_name != display_option->short_name) continue;

		// Check that the video signal option is included.
		return std::find(list_option->options.begin(), list_option->options.end(), display_list_option->options.front()) != list_option->options.end();
	}

	return NO;
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

- (void)setUseQuickBootingHack:(BOOL)useQuickBootingHack {
	Configurable::Device *configurable_device = _machine->configurable_device();
	if(!configurable_device) return;

	@synchronized(self) {
		_useQuickBootingHack = useQuickBootingHack;

		Configurable::SelectionSet selection_set;
		append_quick_boot_selection(selection_set, useQuickBootingHack ? true : false);
		configurable_device->set_selections(selection_set);
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

#pragma mark - Activity observation

- (void)addLED:(NSString *)led {
	[_leds addObject:led];
}

- (NSArray<NSString *> *)leds {
	return _leds;
}

#pragma mark - Timer

- (void)openGLView:(CSOpenGLView *)view didUpdateDisplayLink:(CVDisplayLinkRef)displayLink {
}

- (void)openGLViewDisplayLinkDidFire:(CSOpenGLView *)view {
	CGSize pixelSize = view.backingSize;
	@synchronized(self) {
		_pixelSize = pixelSize;
	}
	[self.view performWithGLContext:^{
		self->_scanTarget->draw((int)pixelSize.width, (int)pixelSize.height);
	} flushDrawable:YES];
}

- (void)start {
	_timer = [[CSHighPrecisionTimer alloc] initWithTask:^{
		CGSize pixelSize;
		@synchronized(self) {
			self->_machine->crt_machine()->run_for(2500000.0 / 1000000000.0);
			pixelSize = self->_pixelSize;
		}

		if(!self->is_updating.test_and_set()) {
			dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
				[self.view performWithGLContext:^{
					self->_scanTarget->update((int)pixelSize.width, (int)pixelSize.height);
				} flushDrawable:NO];
				self->is_updating.clear();
			});
		}
	} interval:2500000];
}

- (void)stop {
	[_timer invalidate];
	_timer = nil;
}

@end
