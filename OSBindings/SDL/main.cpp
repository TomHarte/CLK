//
//  main.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/11/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <sys/stat.h>
#include <unistd.h>

#include <SDL2/SDL.h>

#include "../../Analyser/Static/StaticAnalyser.hpp"
#include "../../Machines/Utility/MachineForTarget.hpp"

#include "../../ClockReceiver/TimeTypes.hpp"
#include "../../ClockReceiver/ScanSynchroniser.hpp"

#include "../../Machines/MachineTypes.hpp"

#include "../../Activity/Observer.hpp"
#include "../../Outputs/OpenGL/Primitives/Rectangle.hpp"
#include "../../Outputs/OpenGL/ScanTarget.hpp"
#include "../../Outputs/OpenGL/Screenshot.hpp"

#include "../../Reflection/Enum.hpp"
#include "../../Reflection/Struct.hpp"

namespace {

struct MachineRunner {
	MachineRunner() {
		frame_lock_.clear();
	}

	~MachineRunner() {
		stop();
	}

	void start() {
		last_time_ = Time::nanos_now();
		timer_ = SDL_AddTimer(timer_period, &sdl_callback, reinterpret_cast<void *>(this));
	}

	void stop() {
		if(timer_) {
			// SDL doesn't define whether SDL_RemoveTimer will block until any pending calls
			// have been completed, or will return instantly. So: do an ordered shutdown,
			// then remove the timer.
			state_ = State::Stopping;
			while(state_ == State::Stopping) {
				frame_lock_.clear();
			}

			SDL_RemoveTimer(timer_);
			timer_ = 0;
		}
	}

	void signal_vsync() {
		const auto now = Time::nanos_now();
		const auto previous_vsync_time = vsync_time_.load();
		vsync_time_.store(now);

		// Update estimate of current frame time.
		frame_time_average_ -= frame_times_[frame_time_pointer_];
		frame_times_[frame_time_pointer_] = now - previous_vsync_time;
		frame_time_average_ += frame_times_[frame_time_pointer_];
		frame_time_pointer_ = (frame_time_pointer_ + 1) & (frame_times_.size() - 1);

		_frame_period.store((1e9 * 32.0) / double(frame_time_average_));
	}

	void signal_did_draw() {
		frame_lock_.clear();
	}

	void set_speed_multiplier(double multiplier) {
		scan_synchroniser_.set_base_speed_multiplier(multiplier);
	}

	std::mutex *machine_mutex;
	Machine::DynamicMachine *machine;

	private:
		SDL_TimerID timer_ = 0;
		Time::Nanos last_time_ = 0;
		std::atomic<Time::Nanos> vsync_time_;
		std::atomic_flag frame_lock_;

		enum class State {
			Running,
			Stopping,
			Stopped
		};
		std::atomic<State> state_{State::Running};

		Time::ScanSynchroniser scan_synchroniser_;

		// A slightly clumsy means of trying to derive frame rate from calls to
		// signal_vsync(); SDL_DisplayMode provides only an integral quantity
		// whereas, empirically, it's fairly common for monitors to run at the
		// NTSC-esque frame rates of 59.94Hz.
		std::array<Time::Nanos, 32> frame_times_;
		Time::Nanos frame_time_average_ = 0;
		size_t frame_time_pointer_ = 0;
		std::atomic<double> _frame_period;

		static constexpr Uint32 timer_period = 4;
		static Uint32 sdl_callback(Uint32, void *param) {
			reinterpret_cast<MachineRunner *>(param)->update();
			return timer_period;
		}

		void update() {
			// If a shutdown is in progress, signal stoppage and do nothing.
			if(state_ != State::Running) {
				state_ = State::Stopped;
				return;
			}

			// Get time now and determine how long it has been since the last time this
			// function was called. If it's more than half a second then forego any activity
			// now, as there's obviously been some sort of substantial time glitch.
			const auto time_now = Time::nanos_now();
			if(time_now - last_time_ > Time::Nanos(500'000'000)) {
				last_time_ = time_now - Time::Nanos(500'000'000);
			}

			const auto vsync_time = vsync_time_.load();

			std::unique_lock lock_guard(*machine_mutex);
			const auto scan_producer = machine->scan_producer();
			const auto timed_machine = machine->timed_machine();

			bool split_and_sync = false;
			if(last_time_ < vsync_time && time_now >= vsync_time) {
				split_and_sync = scan_synchroniser_.can_synchronise(scan_producer->get_scan_status(), _frame_period);
			}

			if(split_and_sync) {
				timed_machine->run_for(double(vsync_time - last_time_) / 1e9);
				timed_machine->set_speed_multiplier(
					scan_synchroniser_.next_speed_multiplier(scan_producer->get_scan_status())
				);

				// This is a bit of an SDL ugliness; wait here until the next frame is drawn.
				// That is, unless and until I can think of a good way of running background
				// updates via a share group — possibly an extra intermediate buffer is needed?
				lock_guard.unlock();
				while(frame_lock_.test_and_set());
				lock_guard.lock();

				timed_machine->run_for(double(time_now - vsync_time) / 1e9);
			} else {
				timed_machine->set_speed_multiplier(scan_synchroniser_.get_base_speed_multiplier());
				timed_machine->run_for(double(time_now - last_time_) / 1e9);
			}
			last_time_ = time_now;
		}
};

struct SpeakerDelegate: public Outputs::Speaker::Speaker::Delegate {
	// This is empirically the best that I can seem to do with SDL's timer precision.
	static constexpr size_t buffered_samples = 1024;
	bool is_stereo = false;

	void speaker_did_complete_samples(Outputs::Speaker::Speaker *, const std::vector<int16_t> &buffer) final {
		std::lock_guard lock_guard(audio_buffer_mutex_);
		const size_t buffer_size = buffered_samples * (is_stereo ? 2 : 1);
		if(audio_buffer_.size() > buffer_size) {
			audio_buffer_.erase(audio_buffer_.begin(), audio_buffer_.end() - buffer_size);
		}
		audio_buffer_.insert(audio_buffer_.end(), buffer.begin(), buffer.end());
	}

	void audio_callback(Uint8 *stream, int len) {
		std::lock_guard lock_guard(audio_buffer_mutex_);

		// SDL buffer length is in bytes, so there's no need to adjust for stereo/mono in here.
		const std::size_t sample_length = size_t(len) / sizeof(int16_t);
		const std::size_t copy_length = std::min(sample_length, audio_buffer_.size());
		int16_t *const target = static_cast<int16_t *>(static_cast<void *>(stream));

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
			std::lock_guard lock_guard(mutex);
			lights_.clear();

			// Generate a bunch of LEDs for connected drives.
			constexpr float height = 0.05f;
			const float width = height / aspect_ratio;
			const float right_x = 1.0f - 2.0f * width;
			float y = 1.0f - 2.0f * height;
			for(const auto &drive: drives_) {
				lights_.emplace(std::make_pair(drive, std::make_unique<Outputs::Display::OpenGL::Rectangle>(right_x, y, width, height)));
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
			std::lock_guard lock_guard(mutex);
			for(const auto &lit_led: lit_leds_) {
				if(blinking_leds_.find(lit_led) == blinking_leds_.end() && lights_.find(lit_led) != lights_.end())
					lights_[lit_led]->draw(0.0, 0.8, 0.0);
			}
			blinking_leds_.clear();
		}

	private:
		std::vector<std::string> leds_;
		void register_led(const std::string &name) final {
			std::lock_guard lock_guard(mutex);
			leds_.push_back(name);
		}

		std::vector<std::string> drives_;
		void register_drive(const std::string &name) final {
			std::lock_guard lock_guard(mutex);
			drives_.push_back(name);
		}

		void set_led_status(const std::string &name, bool lit) final {
			std::lock_guard lock_guard(mutex);
			if(lit) lit_leds_.insert(name);
			else lit_leds_.erase(name);
		}

		void announce_drive_event(const std::string &name, DriveEvent) final {
			std::lock_guard lock_guard(mutex);
			blinking_leds_.insert(name);
		}

		std::map<std::string, std::unique_ptr<Outputs::Display::OpenGL::Rectangle>> lights_;
		std::set<std::string> lit_leds_;
		std::set<std::string> blinking_leds_;
		std::mutex mutex;
};

bool KeyboardKeyForSDLScancode(SDL_Scancode scancode, Inputs::Keyboard::Key &key) {
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

		BIND(KP_7, Keypad7)	BIND(KP_8, Keypad8)	BIND(KP_9, Keypad9)
		BIND(KP_4, Keypad4)	BIND(KP_5, Keypad5)	BIND(KP_6, Keypad6)
		BIND(KP_1, Keypad1)	BIND(KP_2, Keypad2)	BIND(KP_3, Keypad3)
		BIND(KP_0, Keypad0)

		BIND(ESCAPE, Escape)

		BIND(PRINTSCREEN, PrintScreen)	BIND(SCROLLLOCK, ScrollLock)	BIND(PAUSE, Pause)

		BIND(GRAVE, BackTick)		BIND(MINUS, Hyphen)		BIND(EQUALS, Equals)	BIND(BACKSPACE, Backspace)

		BIND(TAB, Tab)
		BIND(LEFTBRACKET, OpenSquareBracket)	BIND(RIGHTBRACKET, CloseSquareBracket)
		BIND(BACKSLASH, Backslash)

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

		BIND(NUMLOCKCLEAR, NumLock)		BIND(KP_DIVIDE, KeypadSlash)		BIND(KP_MULTIPLY, KeypadAsterisk)
		BIND(KP_PLUS, KeypadPlus)		BIND(KP_MINUS, KeypadMinus)			BIND(KP_ENTER, KeypadEnter)
		BIND(KP_DECIMAL, KeypadDecimalPoint)
		BIND(KP_EQUALS, KeypadEquals)
		BIND(HELP, Help)

		// SDL doesn't seem to have scancodes for hash or keypad delete?
	}
#undef BIND
	return true;
}

struct ParsedArguments {
	std::vector<std::string> file_names;
	std::map<std::string, std::string> selections;	// The empty string will be inserted for arguments without an = suffix.

	void apply(Reflection::Struct *reflectable) const {
		for(const auto &argument: selections) {
			// Replace any dashes with underscores in the argument name.
			std::string property;
			std::transform(argument.first.begin(), argument.first.end(), std::back_inserter(property), [](char c) { return c == '-' ? '_' : c; });

			if(argument.second.empty()) {
				Reflection::set<bool>(*reflectable, property, true);
			} else {
				Reflection::fuzzy_set(*reflectable, property, argument.second);
			}
		}
	}
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
				arguments.selections[argument];	// To create an entry with the default empty string.
			} else {
				const std::string name = argument.substr(0, split_index);
				std::string value = argument.substr(split_index+1, std::string::npos);
				arguments.selections[name] = value;
			}
		} else {
			arguments.file_names.push_back(arg);
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

/*!
	Maintains a communicative window title.
*/
class DynamicWindowTitler {
	public:
		DynamicWindowTitler(SDL_Window *window) : window_(window), file_name_(SDL_GetWindowTitle(window)) {}

		std::string window_title() {
			if(!mouse_is_captured_) return file_name_;
			return file_name_ + " (press control+escape to release mouse)";
		}

		void set_mouse_is_captured(bool is_captured) {
			mouse_is_captured_ = is_captured;
			update_window_title();
		}

		void set_file_name(const std::string &name) {
			file_name_ = name;
			update_window_title();
		}

	private:
		void update_window_title() {
			SDL_SetWindowTitle(window_, window_title().c_str());
		}
		bool mouse_is_captured_ = false;
		SDL_Window *window_ = nullptr;
		std::string file_name_;
};

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

}

int main(int argc, char *argv[]) {
	SDL_Window *window = nullptr;

	// Attempt to parse arguments.
	const ParsedArguments arguments = parse_arguments(argc, argv);

	// This may be printed either as
	const std::string usage_suffix = " [file or --new={machine}] [OPTIONS] [--rompath={path to ROMs}] [--speed={speed multiplier, e.g. 1.5}]  [--logical-keyboard] [--volume={0.0 to 1.0}]";

	// Print a help message if requested.
	if(arguments.selections.find("help") != arguments.selections.end() || arguments.selections.find("h") != arguments.selections.end()) {
		const auto all_machines = Machine::AllMachines(Machine::Type::DoesntRequireMedia, false);

		std::cout << "Usage: " << final_path_component(argv[0]) << usage_suffix << std::endl;
		std::cout << "Use alt+enter to toggle full screen display. Use control+shift+V to paste text." << std::endl;
		std::cout << "Required machine type **and all options** are determined from the file if specified; otherwise use:" << std::endl << std::endl;
		std::cout << "\t--new={";
		bool is_first = true;
		for(const auto &name: all_machines) {
			if(!is_first) std::cout << "|";
			std::cout << name;
			is_first = false;
		}
		std::cout << "}" << std::endl << std::endl;

		std::cout << "Media is required to start the: ";
		const auto other_machines = Machine::AllMachines(Machine::Type::RequiresMedia, true);
		is_first = true;
		for(const auto &name: other_machines) {
			if(!is_first) std::cout << ", ";
			std::cout << name;
			is_first = false;
		}
		std::cout << "." << std::endl << std::endl;

		std::cout << "Further machine options:" << std::endl << std::endl;;

		const auto targets = Machine::TargetsByMachineName(false);
		const auto runtime_options = Machine::AllOptionsByMachineName();
		const auto machine_names = Machine::AllMachines(Machine::Type::Any, true);
		for(const auto &machine: machine_names) {
			const auto target = targets.find(machine);
			const auto options = runtime_options.find(machine);

			const auto target_reflectable = dynamic_cast<Reflection::Struct *>(target != targets.end() ? target->second.get() : nullptr);
			const auto options_reflectable = dynamic_cast<Reflection::Struct *>(options != runtime_options.end() ? options->second.get() : nullptr);

			// Don't print a section for this machine if it has no construction and no runtime options objects.
			if(!target_reflectable && !options_reflectable) continue;

			const auto target_keys = target_reflectable ? target_reflectable->all_keys() : std::vector<std::string>();
			const auto options_keys = options_reflectable ? options_reflectable->all_keys() : std::vector<std::string>();

			// Don't print a section for this machine if it doesn't actually have any options.
			if(target_keys.empty() && options_keys.empty()) {
				continue;
			}

			std::cout << machine << ":" << std::endl;

			// Join the two lists of properties and sort the result.
			std::vector<std::string> all_options = options_keys;
			all_options.insert(all_options.end(), target_keys.begin(), target_keys.end());
			std::sort(all_options.begin(), all_options.end());

			for(const auto &option: all_options) {
				// Replace any underscores with hyphens, better to conform to command-line norms.
				std::string mapped_option;
				std::transform(option.begin(), option.end(), std::back_inserter(mapped_option), [](char c) { return c == '_' ? '-' : c; });
				std::cout << '\t' << "--" << mapped_option;

				auto source = target_reflectable;
				auto type = target_reflectable ? target_reflectable->type_of(option) : nullptr;
				if(!type) {
					source = options_reflectable;
					type = options_reflectable->type_of(option);
				}

				// Is this a registered enum? If so, list options.
				if(!Reflection::Enum::name(*type).empty()) {
					std::cout << "={";
					bool is_first = true;
					for(const auto &value: source->values_for(option)) {
						if(!is_first) std::cout << '|';
						is_first = false;

						std::cout << value;
					}
					std::cout << "}";
				}

				// The above effectively assumes that every field is either a
				// Boolean or an enum. This may need to be revisted. It also
				// assumes no name collisions, but that's kind of unavoidable.

				std::cout << std::endl;
			}

			std::cout << std::endl;
		}
		return EXIT_SUCCESS;
	}

	// Determine the machine for the supplied file, if any, or from --new.
	Analyser::Static::TargetList targets;

	const auto new_argument = arguments.selections.find("new");
	std::string long_machine_name;
	if(new_argument != arguments.selections.end() && !new_argument->second.empty()) {
		// Perform for a case-insensitive search against short names.
		const auto short_names = Machine::AllMachines(Machine::Type::DoesntRequireMedia, false);
		auto short_name = short_names.begin();
		while(short_name != short_names.end()) {
			if(std::equal(
				short_name->begin(), short_name->end(),
				new_argument->second.begin(), new_argument->second.end(),
				[](char a, char b) { return tolower(b) == tolower(a); })) {
				break;
			}
			++short_name;
		}

		// If a match was found, use the corresponding long name to look up a suitable
		// Analyser::Statuc::Target and move that to the targets list.
		if(short_name != short_names.end()) {
			long_machine_name = Machine::AllMachines(Machine::Type::DoesntRequireMedia, true)[short_name - short_names.begin()];
			auto targets_by_machine = Machine::TargetsByMachineName(false);
			std::unique_ptr<Analyser::Static::Target> tgt = std::move(targets_by_machine[long_machine_name]);
			targets.push_back(std::move(tgt));
		}
	} else if(!arguments.file_names.empty()) {
		// Take the first file name that actually implies a machine.
		auto file_name = arguments.file_names.begin();
		while(file_name != arguments.file_names.end() && targets.empty()) {
			targets = Analyser::Static::GetTargets(*file_name);
			++file_name;
		}
	}

	if(targets.empty()) {
		if(!arguments.file_names.empty()) {
			std::cerr << "Cannot open ";
			bool is_first = true;
			for(const auto &name: arguments.file_names) {
				if(!is_first) std::cerr << ", ";
				is_first = false;
				std::cerr << name;
			}
			std::cerr << "; no target machine found" << std::endl;
			return EXIT_FAILURE;
		}

		if(!new_argument->second.empty()) {
			std::cerr << "Unknown machine: " << new_argument->second << std::endl;
			return EXIT_FAILURE;
		}

		std::cerr << "Usage: " << final_path_component(argv[0]) << usage_suffix << std::endl;
		std::cerr << "Use --help to learn more about available options." << std::endl;
		return EXIT_FAILURE;
	}

	MachineRunner machine_runner;
	SpeakerDelegate speaker_delegate;

	// For vanilla SDL purposes, assume system ROMs can be found in one of:
	//
	//	/usr/local/share/CLK/[system];
	//	/usr/share/CLK/[system]; or
	//	[user-supplied path]/[system]
	std::vector<ROMMachine::ROM> requested_roms;
	ROMMachine::ROMFetcher rom_fetcher = [&requested_roms, &arguments]
		(const std::vector<ROMMachine::ROM> &roms) -> std::vector<std::unique_ptr<std::vector<uint8_t>>> {
			requested_roms.insert(requested_roms.end(), roms.begin(), roms.end());

			std::vector<std::string> paths = {
				"/usr/local/share/CLK/",
				"/usr/share/CLK/"
			};

			const auto rompath = arguments.selections.find("rompath");
			if(rompath != arguments.selections.end()) {
				if(rompath->second.back() != '/') {
					paths.push_back(rompath->second + "/");
				} else {
					paths.push_back(rompath->second);
				}
			}

			std::vector<std::unique_ptr<std::vector<uint8_t>>> results;
			for(const auto &rom: roms) {
				FILE *file = nullptr;
				for(const auto &path: paths) {
					std::string local_path = path + rom.machine_name + "/" + rom.file_name;
					file = std::fopen(local_path.c_str(), "rb");
					if(file) break;
				}

				if(!file) {
					results.emplace_back(nullptr);
					continue;
				}

				auto data = std::make_unique<std::vector<uint8_t>>();

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

	// Apply all command-line options to the targets.
	for(auto &target: targets) {
		auto reflectable_target = dynamic_cast<Reflection::Struct *>(target.get());
		if(!reflectable_target) continue;
		arguments.apply(reflectable_target);
	}

	// Create and configure a machine.
	::Machine::Error error;
	std::mutex machine_mutex;
	std::unique_ptr<::Machine::DynamicMachine> machine(::Machine::MachineForTargets(targets, rom_fetcher, error));
	if(!machine) {
		switch(error) {
			default: break;
			case ::Machine::Error::MissingROM:
				std::cerr << "Could not find system ROMs; please install to /usr/local/share/CLK/ or /usr/share/CLK/, or provide a --rompath." << std::endl;
				std::cerr << "One or more of the following was needed but not found:" << std::endl;
				for(const auto &rom: requested_roms) {
					std::cerr << rom.machine_name << '/' << rom.file_name << " (";
					if(!rom.descriptive_name.empty()) {
						std::cerr << rom.descriptive_name << "; ";
					}
					std::cerr << "usual crc32s: ";
					bool is_first = true;
					for(const auto crc32: rom.crc32s) {
						if(!is_first) std::cerr << ", ";
						is_first = false;
						std::cerr << std::hex << std::setfill('0') << std::setw(8) << crc32;
					}
					std::cerr << ")" << std::endl;
				}
			break;
		}

		return EXIT_FAILURE;
	}

	// Apply all command-line options to the machines.
	auto configurable = machine->configurable_device();
	if(configurable) {
		const auto options = configurable->get_options();
		arguments.apply(options.get());
		configurable->set_options(options);
	}

	// Apply the speed multiplier, if one was requested.
	{
		const auto speed_argument = arguments.selections.find("speed");
		if(speed_argument != arguments.selections.end()) {
			const char *speed_string = speed_argument->second.c_str();
			char *end;
			const double speed = strtod(speed_string, &end);

			if(size_t(end - speed_string) != strlen(speed_string)) {
				std::cerr << "Unable to parse speed: " << speed_string << std::endl;
			} else if(speed <= 0.0) {
				std::cerr << "Cannot run at speed " << speed_string << "; speeds must be positive." << std::endl;
			} else {
				machine_runner.set_speed_multiplier(speed);
			}
		}
	}

	// Apply the desired output volume, if requested.
	{
		const auto volume_argument = arguments.selections.find("volume");
		if(volume_argument != arguments.selections.end()) {
			const char *volume_string = volume_argument->second.c_str();
			char *end;
			const double volume = strtod(volume_string, &end);

			if(size_t(end - volume_string) != strlen(volume_string)) {
				std::cerr << "Unable to parse volume: " << volume_string << std::endl;
			} else if(volume < 0.0 || volume > 1.0) {
				std::cerr << "Cannot run with volume " << volume_string << "; volumes must be between 0.0 and 1.0." << std::endl;
			} else {
				const auto speaker = machine->audio_producer()->get_speaker();
				if(speaker) speaker->set_output_volume(volume);
			}
		}
	}

	// Check whether a 'logical' keyboard has been requested, or the machine would prefer one anyway.
	const bool logical_keyboard =
		(arguments.selections.find("logical-keyboard") != arguments.selections.end()) ||
		(machine->keyboard_machine() && machine->keyboard_machine()->prefers_logical_input());
	if(logical_keyboard) {
		SDL_StartTextInput();
	}

	// Ensure all media is inserted, if this machine accepts it.
	{
		auto media_target = machine->media_target();
		if(media_target) {
			Analyser::Static::Media media;
			for(const auto &file_name: arguments.file_names) {
				media += Analyser::Static::GetMedia(file_name);
			}
			media_target->insert_media(media);
		}
	}

	// Attempt to set up video and audio.
	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
		std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
		return EXIT_FAILURE;
	}

	// Ask for no depth buffer, a core profile and vsync-aligned rendering.
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
	SDL_GL_SetSwapInterval(1);

	window = SDL_CreateWindow(	long_machine_name.empty() ? final_path_component(arguments.file_names.front()).c_str() : long_machine_name.c_str(),
								SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
								400, 300,
								SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

	DynamicWindowTitler window_titler(window);

	SDL_GLContext gl_context = nullptr;
	if(window) {
		gl_context = SDL_GL_CreateContext(window);
	}
	if(!window || !gl_context) {
		std::cerr << "Could not create " << (window ? "OpenGL context" : "window");
		std::cerr << "; reported error: \"" << SDL_GetError() << "\"" << std::endl;
		return EXIT_FAILURE;
	}

	SDL_GL_MakeCurrent(window, gl_context);

	GLint target_framebuffer = 0;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &target_framebuffer);

	// Setup output, assuming a CRT machine for now, and prepare a best-effort updater.
	Outputs::Display::OpenGL::ScanTarget scan_target(target_framebuffer);
	std::unique_ptr<ActivityObserver> activity_observer;
	bool uses_mouse;
	std::vector<SDLJoystick> joysticks;

	machine_runner.machine_mutex = &machine_mutex;
	const auto setup_machine_input_output = [&scan_target, &machine, &speaker_delegate, &activity_observer, &joysticks, &uses_mouse, &machine_runner] {
		// Wire up the best-effort updater, its delegate, and the speaker delegate.
		machine_runner.machine = machine.get();

		machine->scan_producer()->set_scan_target(&scan_target);

		// For now, lie about audio output intentions.
		auto speaker = machine->audio_producer()->get_speaker();
		if(speaker) {
			// Create an audio pipe.
			SDL_AudioSpec desired_audio_spec;
			SDL_AudioSpec obtained_audio_spec;

			SDL_zero(desired_audio_spec);
			desired_audio_spec.freq = 48000;	// TODO: how can I get SDL to reveal the output rate of this machine?
			desired_audio_spec.format = AUDIO_S16;
			desired_audio_spec.channels = 1 + int(speaker->get_is_stereo());
			desired_audio_spec.samples = Uint16(SpeakerDelegate::buffered_samples);
			desired_audio_spec.callback = SpeakerDelegate::SDL_audio_callback;
			desired_audio_spec.userdata = &speaker_delegate;

			speaker_delegate.audio_device = SDL_OpenAudioDevice(nullptr, 0, &desired_audio_spec, &obtained_audio_spec, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);

			speaker->set_output_rate(obtained_audio_spec.freq, desired_audio_spec.samples, obtained_audio_spec.channels == 2);
			speaker_delegate.is_stereo = obtained_audio_spec.channels == 2;
			speaker->set_delegate(&speaker_delegate);
			SDL_PauseAudioDevice(speaker_delegate.audio_device, 0);
		}

		/*
			If the machine offers anything for activity observation,
			create and register an activity observer.
		*/
		Activity::Source *const activity_source = machine->activity_source();
		if(activity_source) {
			activity_observer = std::make_unique<ActivityObserver>(activity_source, 4.0f / 3.0f);
		} else {
			activity_observer = nullptr;
		}

		// If this is a joystick machine, check for and open attached joysticks.
		const auto joystick_machine = machine->joystick_machine();
		if(joystick_machine) {
			SDL_InitSubSystem(SDL_INIT_JOYSTICK);
			for(int c = 0; c < SDL_NumJoysticks(); ++c) {
				joysticks.emplace_back(SDL_JoystickOpen(c));
			}
		} else {
			joysticks.clear();
		}

		// Keep a record of whether mouse events can be forwarded.
		uses_mouse = !!machine->mouse_machine();
	};
	setup_machine_input_output();

	int window_width, window_height;
	SDL_GetWindowSize(window, &window_width, &window_height);

	// SDL 2.x delivers key up/down events and text inputs separately even when they're correlated;
	// this struct and map is used to correlate them by time.
	struct KeyPress {
		uint32_t timestamp = 0;
		std::string input;
		SDL_Scancode scancode = SDL_SCANCODE_UNKNOWN;
		SDL_Keycode keycode = SDLK_UNKNOWN;
		bool is_down = true;

		KeyPress(uint32_t timestamp, const char *text) : timestamp(timestamp), input(text) {}
		KeyPress(uint32_t timestamp, SDL_Scancode scancode, SDL_Keycode keycode, bool is_down) : timestamp(timestamp), scancode(scancode), keycode(keycode), is_down(is_down) {}
		KeyPress() {}
	};
	std::vector<KeyPress> keypresses;

	// Run the main event loop until the OS tells us to quit.
	bool should_quit = false;
	Uint32 fullscreen_mode = 0;
	machine_runner.start();
	while(!should_quit) {
		// Draw a new frame, indicating completion of the draw to the machine runner.
		scan_target.update(int(window_width), int(window_height));
		scan_target.draw(int(window_width), int(window_height));
		if(activity_observer) activity_observer->draw();
		machine_runner.signal_did_draw();

		// Wait for presentation of that frame, posting a vsync.
		SDL_GL_SwapWindow(window);
		machine_runner.signal_vsync();

		// NB: machine_mutex is *not* currently locked, therefore it shouldn't
		// be 'most' of the time — assuming most of the time is spent waiting
		// on vsync, anyway.

		// Grab the machine lock and process all pending events.
		std::lock_guard lock_guard(machine_mutex);
		const auto keyboard_machine = machine->keyboard_machine();
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
							if(activity_observer) activity_observer->set_aspect_ratio(float(window_width) / float(window_height));
						} break;

						default: break;
					}
				break;

				case SDL_DROPFILE: {
					const Analyser::Static::Media media = Analyser::Static::GetMedia(event.drop.file);

					// If the new file is only media, insert it; if it is a state snapshot then
					// tear down the entire machine and replace it.
					if(!media.empty()) {
						machine->media_target()->insert_media(media);
						break;
					}

					targets = Analyser::Static::GetTargets(event.drop.file);
					if(targets.empty()) break;

					::Machine::Error error;
					std::unique_ptr<::Machine::DynamicMachine> new_machine(::Machine::MachineForTargets(targets, rom_fetcher, error));
					if(error != Machine::Error::None) break;

					machine = std::move(new_machine);
					static_cast<Outputs::Display::ScanTarget *>(&scan_target)->will_change_owner();
					setup_machine_input_output();
					window_titler.set_file_name(final_path_component(event.drop.file));
				} break;

				case SDL_TEXTINPUT:
					keypresses.emplace_back(event.text.timestamp, event.text.text);
				break;

				case SDL_KEYDOWN:
				case SDL_KEYUP: {
					if(event.type == SDL_KEYDOWN) {
						// Syphon off the key-press if it's control+shift+V (paste).
						if(event.key.keysym.sym == SDLK_v && (SDL_GetModState()&KMOD_CTRL) && (SDL_GetModState()&KMOD_SHIFT)) {
							if(keyboard_machine) {
								keyboard_machine->type_string(SDL_GetClipboardText());
								break;
							}
						}

						// Use ctrl+escape to release the mouse (if captured).
						if(event.key.keysym.sym == SDLK_ESCAPE && (SDL_GetModState()&KMOD_CTRL)) {
							SDL_SetRelativeMouseMode(SDL_FALSE);
							window_titler.set_mouse_is_captured(false);
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
					}

					// Syphon off alt+enter (toggle full-screen) upon key up only; this was previously a key down action,
					// but the SDL_KEYDOWN announcement was found to be reposted after changing graphics mode on some
					// systems, causing a loop of changes, so key up is safer.
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

					keypresses.emplace_back(event.text.timestamp, event.key.keysym.scancode, event.key.keysym.sym, event.type == SDL_KEYDOWN);
				} break;

				case SDL_MOUSEBUTTONDOWN:
				case SDL_MOUSEBUTTONUP: {
					if(uses_mouse && event.type == SDL_MOUSEBUTTONDOWN && !SDL_GetRelativeMouseMode()) {
						SDL_SetRelativeMouseMode(SDL_TRUE);
						window_titler.set_mouse_is_captured(true);
						break;
					}

					const auto mouse_machine = machine->mouse_machine();
					if(mouse_machine) {
						mouse_machine->get_mouse().set_button_pressed(
							event.button.button % mouse_machine->get_mouse().get_number_of_buttons(),
							event.type == SDL_MOUSEBUTTONDOWN);
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

		std::vector<KeyPress> matched_keypresses;
		if(logical_keyboard) {
			// Look for potential keypress merges; SDL doesn't in any capacity guarantee that keypresses that produce
			// symbols will be delivered with the same timestamp. So look for any pairs of recorded kepresses that are
			// close together temporally and otherwise seem to match.
			if(keypresses.size()) {
				auto next_keypress = keypresses.begin();

				while(next_keypress != keypresses.end()) {
					auto keypress = next_keypress;
					++next_keypress;

					// If the two appear to pair off, push a combination and advance twice.
					// Otherwise, keep just the first and advance once.
					if(
						next_keypress != keypresses.end() &&
						keypress->timestamp >= next_keypress->timestamp - 5 &&
						keypress->is_down && next_keypress->is_down &&
						!keypress->input.size() != !next_keypress->input.size() &&
						(keypress->scancode != SDL_SCANCODE_UNKNOWN) != (next_keypress->scancode != SDL_SCANCODE_UNKNOWN)) {

						KeyPress combined_keypress;

						if(keypress->scancode != SDL_SCANCODE_UNKNOWN) {
							combined_keypress.scancode = keypress->scancode;
							combined_keypress.keycode = keypress->keycode;
							combined_keypress.input = std::move(next_keypress->input);
						} else {
							combined_keypress.scancode = next_keypress->scancode;
							combined_keypress.keycode = next_keypress->keycode;
							combined_keypress.input = std::move(keypress->input);
						};
						++next_keypress;

						matched_keypresses.push_back(combined_keypress);
					} else {
						matched_keypresses.push_back(*keypress);
					}
				}
			}
		}

		// Handle accumulated key states.
		const auto joystick_machine = machine->joystick_machine();
		for (const auto &keypress: logical_keyboard ? matched_keypresses : keypresses) {
			// Try to set this key on the keyboard first, if there is one.
			if(keyboard_machine) {
				Inputs::Keyboard::Key key = Inputs::Keyboard::Key::Space;
				if(KeyboardKeyForSDLScancode(keypress.scancode, key)) {
					// In principle there's no need for a conditional here; in practice logical_keyboard mode
					// is sufficiently untested on SDL, and somewhat too reliant on empirical timestamp behaviour,
					// for it to be trustworthy enough otherwise to expose.
					if(logical_keyboard) {
						if(keyboard_machine->apply_key(key, keypress.input.size() ? keypress.input[0] : 0, keypress.is_down, logical_keyboard)) {
							continue;
						}
					} else {
						// This is a slightly terrible way of obtaining a symbol for the key, e.g. for letters it will always return
						// the capital letter version, at least empirically. But it'll have to do for now.
						const char *key_name = SDL_GetKeyName(keypress.keycode);
						if(keyboard_machine->get_keyboard().set_key_pressed(key, (strlen(key_name) == 1) ? key_name[0] : 0, keypress.is_down)) {
							continue;
						}
					}
				}
			}

			// Having failed that, try converting it into a joystick action.
			if(joystick_machine) {
				auto &joysticks = joystick_machine->get_joysticks();
				if(!joysticks.empty()) {
					const bool is_pressed = keypress.is_down;
					switch(keypress.scancode) {
						case SDL_SCANCODE_LEFT:		joysticks[0]->set_input(Inputs::Joystick::Input::Left, is_pressed);		break;
						case SDL_SCANCODE_RIGHT:	joysticks[0]->set_input(Inputs::Joystick::Input::Right, is_pressed);	break;
						case SDL_SCANCODE_UP:		joysticks[0]->set_input(Inputs::Joystick::Input::Up, is_pressed);		break;
						case SDL_SCANCODE_DOWN:		joysticks[0]->set_input(Inputs::Joystick::Input::Down, is_pressed);		break;
						case SDL_SCANCODE_SPACE:	joysticks[0]->set_input(Inputs::Joystick::Input::Fire, is_pressed);		break;
						case SDL_SCANCODE_A:		joysticks[0]->set_input(Inputs::Joystick::Input(Inputs::Joystick::Input::Fire, 0), is_pressed);	break;
						case SDL_SCANCODE_S:		joysticks[0]->set_input(Inputs::Joystick::Input(Inputs::Joystick::Input::Fire, 1), is_pressed);	break;
						case SDL_SCANCODE_D:		joysticks[0]->set_input(Inputs::Joystick::Input(Inputs::Joystick::Input::Fire, 2), is_pressed);	break;
						case SDL_SCANCODE_F:		joysticks[0]->set_input(Inputs::Joystick::Input(Inputs::Joystick::Input::Fire, 3), is_pressed);	break;
						default: {
							if(keypress.input.size()) {
								joysticks[0]->set_input(Inputs::Joystick::Input(keypress.input[0]), is_pressed);
							}
						} break;
					}
				}
			}
		}
		keypresses.clear();

		// Push new joystick state, if any.
		if(joystick_machine) {
			auto &machine_joysticks = joystick_machine->get_joysticks();
			for(size_t c = 0; c < joysticks.size(); ++c) {
				size_t target = c % machine_joysticks.size();

				// Post the first two analogue axes presented by the controller as horizontal and vertical inputs,
				// unless the user seems to be using a hat.
				// SDL will return a value in the range [-32768, 32767], so map from that to [0, 1.0]
				if(!joysticks[c].hat_values()) {
					const float x_axis = float(SDL_JoystickGetAxis(joysticks[c].get(), 0) + 32768) / 65535.0f;
					const float y_axis = float(SDL_JoystickGetAxis(joysticks[c].get(), 1) + 32768) / 65535.0f;
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
	}

	// Clean up.
	machine_runner.stop();	// Ensure no further updates will occur.
	joysticks.clear();
	SDL_DestroyWindow( window );
	SDL_Quit();

	return EXIT_SUCCESS;
}
