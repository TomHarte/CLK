//
//  main.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/11/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <sys/stat.h>
#include <unistd.h>

#include <SDL2/SDL.h>

#include "../../Analyser/Static/StaticAnalyser.hpp"
#include "../../Machines/Utility/MachineForTarget.hpp"

#include "../../Machines/MediaTarget.hpp"
#include "../../Machines/CRTMachine.hpp"

#include "../../Concurrency/BestEffortUpdater.hpp"

#include "../../Activity/Observer.hpp"
#include "../../Outputs/OpenGL/Primitives/Rectangle.hpp"
#include "../../Outputs/OpenGL/ScanTarget.hpp"
#include "../../Outputs/OpenGL/Screenshot.hpp"

namespace {

struct BestEffortUpdaterDelegate: public Concurrency::BestEffortUpdater::Delegate {
	void update(Concurrency::BestEffortUpdater *updater, Time::Seconds duration, bool did_skip_previous_update) override {
		machine->crt_machine()->run_for(duration);
	}

	Machine::DynamicMachine *machine;
};

struct SpeakerDelegate: public Outputs::Speaker::Speaker::Delegate {
	// This is set to a relatively large number for now.
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

class ActivityObserver: public Activity::Observer {
	public:
		ActivityObserver(Activity::Source *source, float aspect_ratio) {
			// Get the suorce to supply all LEDs and drives.
			source->set_activity_observer(this);

			// The objective is to display drives on one side of the screen, other LEDs on the other. Drives
			// may or may not have LEDs and this code intends to display only those which do; so a quick
			// comparative processing of the two lists is called for.

			// Strip the list of drives to only those which have LEDs. Thwy're the ones that'll be displayed.
			drives_.resize(std::remove_if(drives_.begin(), drives_.end(), [this](const std::string &string) {
				return std::find(leds_.begin(), leds_.end(), string) == leds_.end();
			}) - drives_.begin());

			// Remove from the list of LEDs any which are drives. Those will be represented separately.
			leds_.resize(std::remove_if(leds_.begin(), leds_.end(), [this](const std::string &string) {
				return std::find(drives_.begin(), drives_.end(), string) != drives_.end();
			}) - leds_.begin());

			set_aspect_ratio(aspect_ratio);
		}

		void set_aspect_ratio(float aspect_ratio) {
			lights_.clear();

			// Generate a bunch of LEDs for connected drives.
			const float height = 0.05f;
			const float width = height / aspect_ratio;
			const float right_x = 1.0f - 2.0f * width;
			float y = 1.0f - 2.0f * height;
			for(const auto &drive: drives_) {
				// TODO: use std::make_unique as below, if/when formally embracing C++14.
				lights_.emplace(std::make_pair(drive, std::unique_ptr<Outputs::Display::OpenGL::Rectangle>(new Outputs::Display::OpenGL::Rectangle(right_x, y, width, height))));
				y -= height * 2.0f;
			}

			/*
				This would generate LEDs for things other than drives; I'm declining for now
				due to the inexpressiveness of just painting a rectangle.

				const float left_x = -1.0f + 2.0f * width;
				y = 1.0f - 2.0f * height;
				for(const auto &led: leds_) {
					lights_.emplace(std::make_pair(led, std::make_unique<OpenGL::Rectangle>(left_x, y, width, height)));
					y -= height * 2.0f;
				}
			*/
		}

		void draw() {
			for(const auto &lit_led: lit_leds_) {
				if(blinking_leds_.find(lit_led) == blinking_leds_.end() && lights_.find(lit_led) != lights_.end())
					lights_[lit_led]->draw(0.0, 0.8, 0.0);
			}
			blinking_leds_.clear();
		}

	private:
		std::vector<std::string> leds_;
		void register_led(const std::string &name) override {
			leds_.push_back(name);
		}

		std::vector<std::string> drives_;
		void register_drive(const std::string &name) override {
			drives_.push_back(name);
		}

		void set_led_status(const std::string &name, bool lit) override {
			if(lit) lit_leds_.insert(name);
			else lit_leds_.erase(name);
		}

		void announce_drive_event(const std::string &name, DriveEvent event) override {
			blinking_leds_.insert(name);
		}

		std::map<std::string, std::unique_ptr<Outputs::Display::OpenGL::Rectangle>> lights_;
		std::set<std::string> lit_leds_;
		std::set<std::string> blinking_leds_;
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
				arguments.selections[argument].reset(new Configurable::BooleanSelection(true));
			} else {
				std::string name = argument.substr(0, split_index);
				std::string value = argument.substr(split_index+1, std::string::npos);
				arguments.selections[name].reset(new Configurable::ListSelection(value));
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

/*!
	Executes @c command and returns its STDOUT.
*/
std::string system_get(const char *command) {
	std::unique_ptr<FILE, decltype((pclose))> pipe(popen(command, "r"), pclose);
	if(!pipe) return "";

	std::string result;
	while(!feof(pipe.get())) {
		std::array<char, 256> buffer;
		if(fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
			result += buffer.data();
	}
	return result;
}

}

int main(int argc, char *argv[]) {
	SDL_Window *window = nullptr;

	// Attempt to parse arguments.
	ParsedArguments arguments = parse_arguments(argc, argv);

	// This may be printed either as
	const std::string usage_suffix = " [file] [OPTIONS] [--rompath={path to ROMs}]";

	// Print a help message if requested.
	if(arguments.selections.find("help") != arguments.selections.end() || arguments.selections.find("h") != arguments.selections.end()) {
		std::cout << "Usage: " << final_path_component(argv[0]) << usage_suffix << std::endl;
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
		std::cerr << "Usage: " << final_path_component(argv[0]) << usage_suffix << std::endl;
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
	//	/usr/local/share/CLK/[system];
	//	/usr/share/CLK/[system]; or
	//	[user-supplied path]/[system]
	std::vector<std::string> rom_names;
	std::string machine_name;
	ROMMachine::ROMFetcher rom_fetcher = [&rom_names, &machine_name, &arguments]
		(const std::string &machine, const std::vector<std::string> &names) -> std::vector<std::unique_ptr<std::vector<uint8_t>>> {
			rom_names.insert(rom_names.end(), names.begin(), names.end());
			machine_name = machine;

			std::vector<std::string> paths = {
				"/usr/local/share/CLK/",
				"/usr/share/CLK/"
			};
			if(arguments.selections.find("rompath") != arguments.selections.end()) {
				std::string user_path = arguments.selections["rompath"]->list_selection()->value;
				if(user_path.back() != '/') {
					paths.push_back(user_path + "/");
				} else {
					paths.push_back(user_path);
				}
			}

			std::vector<std::unique_ptr<std::vector<uint8_t>>> results;
			for(const auto &name: names) {
				FILE *file = nullptr;
				for(const auto &path: paths) {
					std::string local_path = path + machine + "/" + name;
					file = std::fopen(local_path.c_str(), "rb");
					if(file) break;
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
				std::cerr << "Could not find system ROMs; please install to /usr/local/share/CLK/ or /usr/share/CLK/, or provide a --rompath." << std::endl;
				std::cerr << "One or more of the following was needed but not found:" << std::endl;
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

	SDL_GLContext gl_context = nullptr;
	if(window) {
		gl_context = SDL_GL_CreateContext(window);
	}
	if(!window || !gl_context) {
		std::cerr << "Could not create " << (window ? "OpenGL context" : "window");
		std::cerr << "; reported error: \"" << SDL_GetError() << "\"" << std::endl;
		return -1;
	}

	SDL_GL_MakeCurrent(window, gl_context);

	GLint target_framebuffer = 0;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &target_framebuffer);

	// Setup output, assuming a CRT machine for now, and prepare a best-effort updater.
	Outputs::Display::OpenGL::ScanTarget scan_target(target_framebuffer);
	machine->crt_machine()->set_scan_target(&scan_target);

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

	Configurable::Device *const configurable_device = machine->configurable_device();
	if(configurable_device) {
		// Establish user-friendly options by default.
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

		// Apply the user's actual selections to override the defaults.
		configurable_device->set_selections(arguments.selections);
	}

	// If this is a joystick machine, check for and open attached joysticks.
	/*!
		Provides a wrapper for SDL_Joystick pointers that can keep track
		of historic hat values.
	*/
	class SDLJoystick {
		public:
			SDLJoystick(SDL_Joystick *joystick) : joystick_(joystick) {
				hat_values_.resize(SDL_JoystickNumHats(joystick));
			}

			~SDLJoystick() {
				SDL_JoystickClose(joystick_);
			}

			/// @returns The underlying SDL_Joystick.
			SDL_Joystick *get() {
				return joystick_;
			}

			/// @returns A reference to the storage for the previous state of hat @c c.
			Uint8 &last_hat_value(int c) {
				return hat_values_[c];
			}

			/// @returns The logic OR of all stored hat states.
			Uint8 hat_values() {
				Uint8 value = 0;
				for(const auto hat_value: hat_values_) {
					value |= hat_value;
				}
				return value;
			}

		private:
			SDL_Joystick *joystick_;
			std::vector<Uint8> hat_values_;
	};
	std::vector<SDLJoystick> joysticks;
	JoystickMachine::Machine *const joystick_machine = machine->joystick_machine();
	if(joystick_machine) {
		SDL_InitSubSystem(SDL_INIT_JOYSTICK);
		for(int c = 0; c < SDL_NumJoysticks(); ++c) {
			joysticks.emplace_back(SDL_JoystickOpen(c));
		}
	}

	/*
		If the machine offers anything for activity observation,
		create and register an activity observer.
	*/
	std::unique_ptr<ActivityObserver> activity_observer;
	Activity::Source *const activity_source = machine->activity_source();
	if(activity_source) {
		activity_observer.reset(new ActivityObserver(activity_source, 4.0f / 3.0f));
	}

	// Run the main event loop until the OS tells us to quit.
	const bool uses_mouse = !!machine->mouse_machine();
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
							scan_target.set_target_framebuffer(target_framebuffer);
							SDL_GetWindowSize(window, &window_width, &window_height);
							if(activity_observer) activity_observer->set_aspect_ratio(static_cast<float>(window_width) / static_cast<float>(window_height));
						} break;

						default: break;
					}
				break;

				case SDL_DROPFILE: {
					Analyser::Static::Media media = Analyser::Static::GetMedia(event.drop.file);
					machine->media_target()->insert_media(media);
				} break;

				case SDL_KEYDOWN:
					// Syphon off the key-press if it's control+shift+V (paste).
					if(event.key.keysym.sym == SDLK_v && (SDL_GetModState()&KMOD_CTRL) && (SDL_GetModState()&KMOD_SHIFT)) {
						const auto keyboard_machine = machine->keyboard_machine();
						if(keyboard_machine) {
							keyboard_machine->type_string(SDL_GetClipboardText());
							break;
						}
					}

					// Use ctrl+escape to release the mouse (if captured).
					if(event.key.keysym.sym == SDLK_ESCAPE && (SDL_GetModState()&KMOD_CTRL)) {
						SDL_SetRelativeMouseMode(SDL_FALSE);
					}

					// Capture ctrl+shift+d as a take-a-screenshot command.
					if(event.key.keysym.sym == SDLK_d && (SDL_GetModState()&KMOD_CTRL) && (SDL_GetModState()&KMOD_SHIFT)) {
						// Grab the screen buffer.
						Outputs::Display::OpenGL::Screenshot screenshot(4, 3);

						// Pick the directory for images. Try `xdg-user-dir PICTURES` first.
						std::string target_directory = system_get("xdg-user-dir PICTURES");

						// Make sure there are no newlines.
						target_directory.erase(std::remove(target_directory.begin(), target_directory.end(), '\n'), target_directory.end());
						target_directory.erase(std::remove(target_directory.begin(), target_directory.end(), '\r'), target_directory.end());

						// Fall back on the HOME directory if necessary.
						if(target_directory.empty()) target_directory = getenv("HOME");

						// Find the first available name of the form ~/clk-screenshot-<number>.bmp.
						size_t index = 0;
						std::string target;
						while(true) {
							target = target_directory + "/clk-screenshot-" + std::to_string(index) + ".bmp";

							struct stat file_stats;
							if(stat(target.c_str(), &file_stats))
								break;

							++index;
						}

						// Create a suitable SDL surface and save the thing.
						const bool is_big_endian = SDL_BYTEORDER == SDL_BIG_ENDIAN;
						SDL_Surface *const surface = SDL_CreateRGBSurfaceFrom(
							screenshot.pixel_data.data(),
							screenshot.width, screenshot.height,
							8*4,
							screenshot.width*4,
							is_big_endian ? 0xff000000 : 0x000000ff,
							is_big_endian ? 0x00ff0000 : 0x0000ff00,
							is_big_endian ? 0x0000ff00 : 0x00ff0000,
							0);
						SDL_SaveBMP(surface, target.c_str());
						SDL_FreeSurface(surface);
						break;
					}

				// deliberate fallthrough...
				case SDL_KEYUP: {

					// Syphon off alt+enter (toggle full-screen) upon key up only; this was previously a key down action,
					// but the SDL_KEYDOWN announcement was found to be reposted after changing graphics mode on some
					// systems so key up is safer.
					if(event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_RETURN && (SDL_GetModState()&KMOD_ALT)) {
						fullscreen_mode ^= SDL_WINDOW_FULLSCREEN_DESKTOP;
						SDL_SetWindowFullscreen(window, fullscreen_mode);
						SDL_ShowCursor((fullscreen_mode&SDL_WINDOW_FULLSCREEN_DESKTOP) ? SDL_DISABLE : SDL_ENABLE);

						// Announce a potential discontinuity in keyboard input.
						const auto keyboard_machine = machine->keyboard_machine();
						if(keyboard_machine) {
							keyboard_machine->get_keyboard().reset_all_keys();
						}
						break;
					}

					const bool is_pressed = event.type == SDL_KEYDOWN;

					const auto keyboard_machine = machine->keyboard_machine();
					if(keyboard_machine) {
						Inputs::Keyboard::Key key = Inputs::Keyboard::Key::Space;
						if(!KeyboardKeyForSDLScancode(event.key.keysym.scancode, key)) break;

						if(keyboard_machine->get_keyboard().observed_keys().find(key) != keyboard_machine->get_keyboard().observed_keys().end()) {
							char key_value = '\0';
							const char *key_name = SDL_GetKeyName(event.key.keysym.sym);
							if(key_name[0] >= 0) key_value = key_name[0];

							keyboard_machine->get_keyboard().set_key_pressed(key, key_value, is_pressed);
							break;
						}
					}

					JoystickMachine::Machine *const joystick_machine = machine->joystick_machine();
					if(joystick_machine) {
						std::vector<std::unique_ptr<Inputs::Joystick>> &joysticks = joystick_machine->get_joysticks();
						if(!joysticks.empty()) {
							switch(event.key.keysym.scancode) {
								case SDL_SCANCODE_LEFT:		joysticks[0]->set_input(Inputs::Joystick::Input::Left, is_pressed);		break;
								case SDL_SCANCODE_RIGHT:	joysticks[0]->set_input(Inputs::Joystick::Input::Right, is_pressed);	break;
								case SDL_SCANCODE_UP:		joysticks[0]->set_input(Inputs::Joystick::Input::Up, is_pressed);		break;
								case SDL_SCANCODE_DOWN:		joysticks[0]->set_input(Inputs::Joystick::Input::Down, is_pressed);		break;
								case SDL_SCANCODE_SPACE:	joysticks[0]->set_input(Inputs::Joystick::Input::Fire, is_pressed);		break;
								case SDL_SCANCODE_A:		joysticks[0]->set_input(Inputs::Joystick::Input(Inputs::Joystick::Input::Fire, 0), is_pressed);	break;
								case SDL_SCANCODE_S:		joysticks[0]->set_input(Inputs::Joystick::Input(Inputs::Joystick::Input::Fire, 1), is_pressed);	break;
								default: {
									const char *key_name = SDL_GetKeyName(event.key.keysym.sym);
									joysticks[0]->set_input(Inputs::Joystick::Input(key_name[0]), is_pressed);
								} break;
							}
						}
					}
				} break;

				case SDL_MOUSEBUTTONDOWN:
					if(uses_mouse && !SDL_GetRelativeMouseMode()) {
						SDL_SetRelativeMouseMode(SDL_TRUE);
						break;
					}
				case SDL_MOUSEBUTTONUP: {
					const auto mouse_machine = machine->mouse_machine();
					if(mouse_machine) {
						printf("%d %s\n", event.button.button, (event.type == SDL_PRESSED) ? "pressed" : "released");
						mouse_machine->get_mouse().set_button_pressed(
							event.button.button % mouse_machine->get_mouse().get_number_of_buttons(),
							event.type == SDL_PRESSED);
					}
				} break;

				case SDL_MOUSEMOTION: {
					if(SDL_GetRelativeMouseMode()) {
						const auto mouse_machine = machine->mouse_machine();
						if(mouse_machine) {
							mouse_machine->get_mouse().move(event.motion.xrel, event.motion.yrel);
						}
					}
				} break;

				default: break;
			}
		}

		// Push new joystick state, if any.
		JoystickMachine::Machine *const joystick_machine = machine->joystick_machine();
		if(joystick_machine) {
			std::vector<std::unique_ptr<Inputs::Joystick>> &machine_joysticks = joystick_machine->get_joysticks();
			for(size_t c = 0; c < joysticks.size(); ++c) {
				size_t target = c % machine_joysticks.size();

				// Post the first two analogue axes presented by the controller as horizontal and vertical inputs,
				// unless the user seems to be using a hat.
				// SDL will return a value in the range [-32768, 32767], so map from that to [0, 1.0]
				if(!joysticks[c].hat_values()) {
					const float x_axis = static_cast<float>(SDL_JoystickGetAxis(joysticks[c].get(), 0) + 32768) / 65535.0f;
					const float y_axis = static_cast<float>(SDL_JoystickGetAxis(joysticks[c].get(), 1) + 32768) / 65535.0f;
					machine_joysticks[target]->set_input(Inputs::Joystick::Input(Inputs::Joystick::Input::Type::Horizontal), x_axis);
					machine_joysticks[target]->set_input(Inputs::Joystick::Input(Inputs::Joystick::Input::Type::Vertical), y_axis);
				}

				// Forward hats as directions; hats always override analogue inputs.
				const int number_of_hats = SDL_JoystickNumHats(joysticks[c].get());
				for(int hat = 0; hat < number_of_hats; ++hat) {
					const Uint8 hat_value = SDL_JoystickGetHat(joysticks[c].get(), hat);
					const Uint8 changes = hat_value ^ joysticks[c].last_hat_value(hat);
					joysticks[c].last_hat_value(hat) = hat_value;

					if(changes & SDL_HAT_UP) {
						machine_joysticks[target]->set_input(Inputs::Joystick::Input(Inputs::Joystick::Input::Type::Up), !!(hat_value & SDL_HAT_UP));
					}
					if(changes & SDL_HAT_DOWN) {
						machine_joysticks[target]->set_input(Inputs::Joystick::Input(Inputs::Joystick::Input::Type::Down), !!(hat_value & SDL_HAT_DOWN));
					}
					if(changes & SDL_HAT_LEFT) {
						machine_joysticks[target]->set_input(Inputs::Joystick::Input(Inputs::Joystick::Input::Type::Left), !!(hat_value & SDL_HAT_LEFT));
					}
					if(changes & SDL_HAT_RIGHT) {
						machine_joysticks[target]->set_input(Inputs::Joystick::Input(Inputs::Joystick::Input::Type::Right), !!(hat_value & SDL_HAT_RIGHT));
					}
				}

				// Forward all fire buttons, retaining their original indices.
				const int number_of_buttons = SDL_JoystickNumButtons(joysticks[c].get());
				for(int button = 0; button < number_of_buttons; ++button) {
					machine_joysticks[target]->set_input(
						Inputs::Joystick::Input(Inputs::Joystick::Input::Type::Fire, button),
						SDL_JoystickGetButton(joysticks[c].get(), button) ? true : false);
				}
			}
		}

		// Display a new frame and wait for vsync.
		updater.update();
		scan_target.update(int(window_width), int(window_height));
		scan_target.draw(int(window_width), int(window_height));
		if(activity_observer) activity_observer->draw();
		SDL_GL_SwapWindow(window);
	}

	// Clean up.
	joysticks.clear();
	SDL_DestroyWindow( window );
	SDL_Quit();

	return 0;
}
