//
//  main.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/11/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>

#include <SDL2/SDL.h>

#include "../../Analyser/Static/StaticAnalyser.hpp"
#include "../../Machines/Utility/MachineForTarget.hpp"

#include "../../Machines/ConfigurationTarget.hpp"
#include "../../Machines/CRTMachine.hpp"

#include "../../Concurrency/BestEffortUpdater.hpp"

namespace {

struct BestEffortUpdaterDelegate: public Concurrency::BestEffortUpdater::Delegate {
	void update(Concurrency::BestEffortUpdater *updater, Time::Seconds duration, bool did_skip_previous_update) override {
		machine->crt_machine()->run_for(duration);
	}

	Machine::DynamicMachine *machine;
};

// This is set to a relatively large number for now.
struct SpeakerDelegate: public Outputs::Speaker::Speaker::Delegate {
	static const int buffer_size = 1024;

	void speaker_did_complete_samples(Outputs::Speaker::Speaker *speaker, const std::vector<int16_t> &buffer) override {
		std::lock_guard<std::mutex> lock_guard(audio_buffer_mutex_);
		if(audio_buffer_.size() > buffer_size) {
			audio_buffer_.erase(audio_buffer_.begin(), audio_buffer_.end() - buffer_size);
		}
		audio_buffer_.insert(audio_buffer_.end(), buffer.begin(), buffer.end());
	}

	void audio_callback(Uint8 *stream, int len) {
		updater->update();
		std::lock_guard<std::mutex> lock_guard(audio_buffer_mutex_);

		std::size_t sample_length = static_cast<std::size_t>(len) / sizeof(int16_t);
		std::size_t copy_length = std::min(sample_length, audio_buffer_.size());
		int16_t *target = static_cast<int16_t *>(static_cast<void *>(stream));

		std::memcpy(stream, audio_buffer_.data(), copy_length * sizeof(int16_t));
		if(copy_length < sample_length) {
			std::memset(&target[copy_length], 0, (sample_length - copy_length) * sizeof(int16_t));
		}
		audio_buffer_.erase(audio_buffer_.begin(), audio_buffer_.begin() + copy_length);
	}

	static void SDL_audio_callback(void *userdata, Uint8 *stream, int len) {
		reinterpret_cast<SpeakerDelegate *>(userdata)->audio_callback(stream, len);
	}

	SDL_AudioDeviceID audio_device;
	Concurrency::BestEffortUpdater *updater;

	std::mutex audio_buffer_mutex_;
	std::vector<int16_t> audio_buffer_;
};

bool KeyboardKeyForSDLScancode(SDL_Keycode scancode, Inputs::Keyboard::Key &key) {
#define BIND(x, y) case SDL_SCANCODE_##x: key = Inputs::Keyboard::Key::y; break;
	switch(scancode) {
		default: return false;

		BIND(F1, F1)	BIND(F2, F2)	BIND(F3, F3)	BIND(F4, F4)	BIND(F5, F5)	BIND(F6, F6)
		BIND(F7, F7)	BIND(F8, F8)	BIND(F9, F9)	BIND(F10, F10)	BIND(F11, F11)	BIND(F12, F12)

		BIND(1, k1)		BIND(2, k2)		BIND(3, k3)		BIND(4, k4)		BIND(5, k5)
		BIND(6, k6)		BIND(7, k7)		BIND(8, k8)		BIND(9, k9)		BIND(0, k0)

		BIND(Q, Q)		BIND(W, W)		BIND(E, E)		BIND(R, R)		BIND(T, T)
		BIND(Y, Y)		BIND(U, U)		BIND(I, I)		BIND(O, O)		BIND(P, P)
		BIND(A, A)		BIND(S, S)		BIND(D, D)		BIND(F, F)		BIND(G, G)
		BIND(H, H)		BIND(J, J)		BIND(K, K)		BIND(L, L)
		BIND(Z, Z)		BIND(X, X)		BIND(C, C)		BIND(V, V)
		BIND(B, B)		BIND(N, N)		BIND(M, M)

		BIND(KP_7, KeyPad7)	BIND(KP_8, KeyPad8)	BIND(KP_9, KeyPad9)
		BIND(KP_4, KeyPad4)	BIND(KP_5, KeyPad5)	BIND(KP_6, KeyPad6)
		BIND(KP_1, KeyPad1)	BIND(KP_2, KeyPad2)	BIND(KP_3, KeyPad3)
		BIND(KP_0, KeyPad0)

		BIND(ESCAPE, Escape)

		BIND(PRINTSCREEN, PrintScreen)	BIND(SCROLLLOCK, ScrollLock)	BIND(PAUSE, Pause)

		BIND(GRAVE, BackTick)		BIND(MINUS, Hyphen)		BIND(EQUALS, Equals)	BIND(BACKSPACE, BackSpace)

		BIND(TAB, Tab)
		BIND(LEFTBRACKET, OpenSquareBracket)	BIND(RIGHTBRACKET, CloseSquareBracket)
		BIND(BACKSLASH, BackSlash)

		BIND(CAPSLOCK, CapsLock)	BIND(SEMICOLON, Semicolon)
		BIND(APOSTROPHE, Quote)		BIND(RETURN, Enter)

		BIND(LSHIFT, LeftShift)		BIND(COMMA, Comma)		BIND(PERIOD, FullStop)
		BIND(SLASH, ForwardSlash)	BIND(RSHIFT, RightShift)

		BIND(LCTRL, LeftControl)	BIND(LALT, LeftOption)		BIND(LGUI, LeftMeta)
		BIND(SPACE, Space)
		BIND(RCTRL, RightControl)	BIND(RALT, RightOption)	BIND(RGUI, RightMeta)

		BIND(LEFT, Left)	BIND(RIGHT, Right)		BIND(UP, Up)	BIND(DOWN, Down)

		BIND(INSERT, Insert)	BIND(HOME, Home)	BIND(PAGEUP, PageUp)
		BIND(DELETE, Delete)	BIND(END, End)		BIND(PAGEDOWN, PageDown)

		BIND(NUMLOCKCLEAR, NumLock)		BIND(KP_DIVIDE, KeyPadSlash)		BIND(KP_MULTIPLY, KeyPadAsterisk)
		BIND(KP_PLUS, KeyPadPlus)		BIND(KP_MINUS, KeyPadMinus)			BIND(KP_ENTER, KeyPadEnter)
		BIND(KP_DECIMAL, KeyPadDecimalPoint)
		BIND(KP_EQUALS, KeyPadEquals)
		BIND(HELP, Help)

		// SDL doesn't seem to have scancodes for hash or keypad delete?
	}
#undef BIND
	return true;
}

struct ParsedArguments {
	std::string file_name;
	Configurable::SelectionSet selections;
};

/*! Parses an argc/argv pair to discern program arguments. */
ParsedArguments parse_arguments(int argc, char *argv[]) {
	ParsedArguments arguments;

	for(int index = 1; index < argc; ++index) {
		char *arg = argv[index];

		// Accepted format is:
		//
		//	--flag			sets a Boolean option to true.
		//	--flag=value	sets the value for a list option.
		//	name			sets the file name to load.
		
		// Anything starting with a dash always makes a selection; otherwise it's a file name.
		if(arg[0] == '-') {
			while(*arg == '-') arg++;

			// Check for an equals sign, to discern a Boolean selection from a list selection.
			std::string argument = arg;
			std::size_t split_index = argument.find("=");

			if(split_index == std::string::npos) {
				arguments.selections[argument] =  std::unique_ptr<Configurable::Selection>(new Configurable::BooleanSelection(true));
			} else {
				std::string name = argument.substr(0, split_index);
				std::string value = argument.substr(split_index+1, std::string::npos);
				arguments.selections[name] =  std::unique_ptr<Configurable::Selection>(new Configurable::ListSelection(value));
			}
		} else {
			arguments.file_name = arg;
		}
	}

	return arguments;
}

std::string final_path_component(const std::string &path) {
	// An empty path has no final component.
	if(path.empty()) {
		return "";
	}

	// Find the last slash...
	auto final_slash = path.find_last_of("/\\");
	
	// If no slash was found at all, return the whole path.
	if(final_slash == std::string::npos) {
		return path;
	}

	// If a slash was found in the final position, remove it and recurse.
	if(final_slash == path.size() - 1) {
		return final_path_component(path.substr(0, path.size() - 1));
	}

	// Otherwise return everything from just after the slash to the end of the path.
	return path.substr(final_slash+1, path.size() - final_slash - 1);
}

}

int main(int argc, char *argv[]) {
	SDL_Window *window = nullptr;

	// Attempt to parse arguments.
	ParsedArguments arguments = parse_arguments(argc, argv);

	// Print a help message if requested.
	if(arguments.selections.find("help") != arguments.selections.end() || arguments.selections.find("h") != arguments.selections.end()) {
		std::cout << "Usage: " << final_path_component(argv[0]) << " [file] [OPTIONS]" << std::endl;
		std::cout << "Use alt+enter to toggle full screen display. Use control+shift+V to paste text." << std::endl;
		std::cout << "Required machine type and configuration is determined from the file. Machines with further options:" << std::endl << std::endl;

		auto all_options = Machine::AllOptionsByMachineName();
		for(const auto &machine_options: all_options) {
			std::cout << machine_options.first << ":" << std::endl;
			for(const auto &option: machine_options.second) {
				std::cout << '\t' << "--" << option->short_name;
				
				Configurable::ListOption *list_option = dynamic_cast<Configurable::ListOption *>(option.get());
				if(list_option) {
					std::cout << "={";
					bool is_first = true;
					for(const auto &option: list_option->options) {
						if(!is_first) std::cout << '|';
						is_first = false;
						std::cout << option;
					}
					std::cout << "}";
				}
				std::cout << std::endl;
			}
			std::cout << std::endl;
		}
		return 0;
	}

	// Perform a sanity check on arguments.
	if(arguments.file_name.empty()) {
		std::cerr << "Usage: " << final_path_component(argv[0]) << " [file] [OPTIONS]" << std::endl;
		std::cerr << "Use --help to learn more about available options." << std::endl;
		return -1;
	}

	// Determine the machine for the supplied file.
	Analyser::Static::TargetList targets = Analyser::Static::GetTargets(arguments.file_name);
	if(targets.empty()) {
		std::cerr << "Cannot open " << arguments.file_name << "; no target machine found" << std::endl;
		return -1;
	}

	Concurrency::BestEffortUpdater updater;
	BestEffortUpdaterDelegate best_effort_updater_delegate;
	SpeakerDelegate speaker_delegate;

	// For vanilla SDL purposes, assume system ROMs can be found in one of:
	//
	//	/usr/local/share/CLK/[system]; or
	//	/usr/share/CLK/[system]
	std::vector<std::string> rom_names;
	std::string machine_name;
	ROMMachine::ROMFetcher rom_fetcher = [&rom_names, &machine_name]
		(const std::string &machine, const std::vector<std::string> &names) -> std::vector<std::unique_ptr<std::vector<uint8_t>>> {
			rom_names.insert(rom_names.end(), names.begin(), names.end());
			machine_name = machine;

			std::vector<std::unique_ptr<std::vector<uint8_t>>> results;
			for(const auto &name: names) {
				std::string local_path = "/usr/local/share/CLK/" + machine + "/" + name;
				FILE *file = std::fopen(local_path.c_str(), "rb");
				if(!file) {
					std::string path = "/usr/share/CLK/" + machine + "/" + name;
					file = std::fopen(path.c_str(), "rb");
				}

				if(!file) {
					results.emplace_back(nullptr);
					continue;
				}

				std::unique_ptr<std::vector<uint8_t>> data(new std::vector<uint8_t>);

				std::fseek(file, 0, SEEK_END);
				data->resize(std::ftell(file));
				std::fseek(file, 0, SEEK_SET);
				std::size_t read = fread(data->data(), 1, data->size(), file);
				std::fclose(file);

				if(read == data->size())
					results.emplace_back(std::move(data));
				else
					results.emplace_back(nullptr);
			}

			return results;
		};

	// Create and configure a machine.
	::Machine::Error error;
	std::unique_ptr<::Machine::DynamicMachine> machine(::Machine::MachineForTargets(targets, rom_fetcher, error));
	if(!machine) {
		switch(error) {
			default: break;
			case ::Machine::Error::MissingROM:
				std::cerr << "Could not find system ROMs; please install to /usr/local/share/CLK/ or /usr/share/CLK/." << std::endl;
				std::cerr << "One or more of the following were needed but not found:" << std::endl;
				for(const auto &name: rom_names) {
					std::cerr << machine_name << '/' << name << std::endl;
				}
			break;
		}

		return -1;
	}

	best_effort_updater_delegate.machine = machine.get();
	speaker_delegate.updater = &updater;
	updater.set_delegate(&best_effort_updater_delegate);

	// Attempt to set up video and audio.
	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
		std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
		return -1;
	}

	// Ask for no depth buffer, a core profile and vsync-aligned rendering.
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
	SDL_GL_SetSwapInterval(1);

	window = SDL_CreateWindow(	final_path_component(arguments.file_name).c_str(),
								SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
								400, 300,
								SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

	if(!window)
	{
		std::cerr << "Could not create window" << std::endl;
		return -1;
	}

	SDL_GLContext gl_context = SDL_GL_CreateContext(window);
	SDL_GL_MakeCurrent(window, gl_context);

	GLint target_framebuffer = 0;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &target_framebuffer);

	// Setup output, assuming a CRT machine for now, and prepare a best-effort updater.
	machine->crt_machine()->setup_output(4.0 / 3.0);
	machine->crt_machine()->get_crt()->set_output_gamma(2.2f);
	machine->crt_machine()->get_crt()->set_target_framebuffer(target_framebuffer);

	// For now, lie about audio output intentions.
	auto speaker = machine->crt_machine()->get_speaker();
	if(speaker) {
		// Create an audio pipe.
		SDL_AudioSpec desired_audio_spec;
		SDL_AudioSpec obtained_audio_spec;

		SDL_zero(desired_audio_spec);
		desired_audio_spec.freq = 48000;	// TODO: how can I get SDL to reveal the output rate of this machine?
		desired_audio_spec.format = AUDIO_S16;
		desired_audio_spec.channels = 1;
		desired_audio_spec.samples = SpeakerDelegate::buffer_size;
		desired_audio_spec.callback = SpeakerDelegate::SDL_audio_callback;
		desired_audio_spec.userdata = &speaker_delegate;

		speaker_delegate.audio_device = SDL_OpenAudioDevice(nullptr, 0, &desired_audio_spec, &obtained_audio_spec, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);

		speaker->set_output_rate(obtained_audio_spec.freq, desired_audio_spec.samples);
		speaker->set_delegate(&speaker_delegate);
		SDL_PauseAudioDevice(speaker_delegate.audio_device, 0);
	}

	int window_width, window_height;
	SDL_GetWindowSize(window, &window_width, &window_height);

	// Establish user-friendly options by default.
	Configurable::Device *configurable_device = machine->configurable_device();
	if(configurable_device) {
		configurable_device->set_selections(configurable_device->get_user_friendly_selections());
		
		// Consider transcoding any list selections that map to Boolean options.
		for(const auto &option: configurable_device->get_options()) {
			// Check for a corresponding selection.
			auto selection = arguments.selections.find(option->short_name);
			if(selection != arguments.selections.end()) {
				// Transcode selection if necessary.
				if(dynamic_cast<Configurable::BooleanOption *>(option.get())) {
					arguments.selections[selection->first] =  std::unique_ptr<Configurable::Selection>(selection->second->boolean_selection());
				}

				if(dynamic_cast<Configurable::ListOption *>(option.get())) {
					arguments.selections[selection->first] =  std::unique_ptr<Configurable::Selection>(selection->second->list_selection());
				}
			}
		}
		configurable_device->set_selections(arguments.selections);
	}

	// Run the main event loop until the OS tells us to quit.
	bool should_quit = false;
	Uint32 fullscreen_mode = 0;
	while(!should_quit) {
		// Process all pending events.
		SDL_Event event;
		while(SDL_PollEvent(&event)) {
			switch(event.type) {
				case SDL_QUIT:	should_quit = true;	break;

				case SDL_WINDOWEVENT:
					switch (event.window.event) {
						case SDL_WINDOWEVENT_RESIZED: {
							GLint target_framebuffer = 0;
							glGetIntegerv(GL_FRAMEBUFFER_BINDING, &target_framebuffer);
							machine->crt_machine()->get_crt()->set_target_framebuffer(target_framebuffer);
							SDL_GetWindowSize(window, &window_width, &window_height);
						} break;

						default: break;
					}
				break;

				case SDL_DROPFILE: {
					Analyser::Static::Media media = Analyser::Static::GetMedia(event.drop.file);
					machine->configuration_target()->insert_media(media);
				} break;

				case SDL_KEYDOWN:
					// Syphon off the key-press if it's control+shift+V (paste).
					if(event.key.keysym.sym == SDLK_v && (SDL_GetModState()&KMOD_CTRL) && (SDL_GetModState()&KMOD_SHIFT)) {
						KeyboardMachine::Machine *keyboard_machine = machine->keyboard_machine();
						if(keyboard_machine) {
							keyboard_machine->type_string(SDL_GetClipboardText());
							break;
						}
					}

					// Also syphon off alt+enter (toggle full-screen).
					if(event.key.keysym.sym == SDLK_RETURN && (SDL_GetModState()&KMOD_ALT)) {
						fullscreen_mode ^= SDL_WINDOW_FULLSCREEN_DESKTOP;
						SDL_SetWindowFullscreen(window, fullscreen_mode);
						SDL_ShowCursor((fullscreen_mode&SDL_WINDOW_FULLSCREEN_DESKTOP) ? SDL_DISABLE : SDL_ENABLE);
						break;
					}

				// deliberate fallthrough...
				case SDL_KEYUP: {
					const bool is_pressed = event.type == SDL_KEYDOWN;

					KeyboardMachine::Machine *const keyboard_machine = machine->keyboard_machine();
					if(keyboard_machine) {
						Inputs::Keyboard::Key key = Inputs::Keyboard::Key::Space;
						if(!KeyboardKeyForSDLScancode(event.key.keysym.scancode, key)) break;

						char key_value = '\0';
						const char *key_name = SDL_GetKeyName(event.key.keysym.sym);
						if(key_name[0] >= 0) key_value = key_name[0];

						keyboard_machine->get_keyboard().set_key_pressed(key, key_value, is_pressed);
						break;
					}

					JoystickMachine::Machine *const joystick_machine = machine->joystick_machine();
					if(joystick_machine) {
						std::vector<std::unique_ptr<Inputs::Joystick>> &joysticks = joystick_machine->get_joysticks();
						if(!joysticks.empty()) {
							switch(event.key.keysym.scancode) {
								case SDL_SCANCODE_LEFT:		joysticks[0]->set_digital_input(Inputs::Joystick::DigitalInput::Left, is_pressed);	break;
								case SDL_SCANCODE_RIGHT:	joysticks[0]->set_digital_input(Inputs::Joystick::DigitalInput::Right, is_pressed);	break;
								case SDL_SCANCODE_UP:		joysticks[0]->set_digital_input(Inputs::Joystick::DigitalInput::Up, is_pressed);	break;
								case SDL_SCANCODE_DOWN:		joysticks[0]->set_digital_input(Inputs::Joystick::DigitalInput::Down, is_pressed);	break;
								case SDL_SCANCODE_SPACE:	joysticks[0]->set_digital_input(Inputs::Joystick::DigitalInput::Fire, is_pressed);	break;
								case SDL_SCANCODE_A:		joysticks[0]->set_digital_input(Inputs::Joystick::DigitalInput(Inputs::Joystick::DigitalInput::Fire, 0), is_pressed);	break;
								case SDL_SCANCODE_S:		joysticks[0]->set_digital_input(Inputs::Joystick::DigitalInput(Inputs::Joystick::DigitalInput::Fire, 1), is_pressed);	break;
								default: {
									const char *key_name = SDL_GetKeyName(event.key.keysym.sym);
									joysticks[0]->set_digital_input(Inputs::Joystick::DigitalInput(key_name[0]), is_pressed);
								} break;
							}
						}
					}
				} break;

				default: break;
			}
		}

		// Display a new frame and wait for vsync.
		updater.update();
		machine->crt_machine()->get_crt()->draw_frame(static_cast<unsigned int>(window_width), static_cast<unsigned int>(window_height), false);
		SDL_GL_SwapWindow(window);
	}

	// Clean up.
	SDL_DestroyWindow( window );
	SDL_Quit();

	return 0;
}
