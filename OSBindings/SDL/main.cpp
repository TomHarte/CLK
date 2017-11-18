//
//  main.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/11/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include <iostream>
#include <memory>
#include <cstdio>

#include <SDL2/SDL.h>

#include "../../StaticAnalyser/StaticAnalyser.hpp"
#include "../../Machines/Utility/MachineForTarget.hpp"

#include "../../Machines/ConfigurationTarget.hpp"
#include "../../Machines/CRTMachine.hpp"

#include "../../Concurrency/BestEffortUpdater.hpp"

namespace {

struct CRTMachineDelegate: public CRTMachine::Machine::Delegate {
	void machine_did_change_clock_rate(CRTMachine::Machine *machine) {
		best_effort_updater->set_clock_rate(machine->get_clock_rate());
	}

	void machine_did_change_clock_is_unlimited(CRTMachine::Machine *machine) {
	}

	Concurrency::BestEffortUpdater *best_effort_updater;
};

struct BestEffortUpdaterDelegate: public Concurrency::BestEffortUpdater::Delegate {
	void update(Concurrency::BestEffortUpdater *updater, int cycles, bool did_skip_previous_update) {
		machine->crt_machine()->run_for(Cycles(cycles));
	}

	Machine::DynamicMachine *machine;
};

// This is set to a relatively large number for now.
const int AudioBufferSize = 1024;

struct SpeakerDelegate: public Outputs::Speaker::Delegate {
	void speaker_did_complete_samples(Outputs::Speaker *speaker, const std::vector<int16_t> &buffer) {
		if(SDL_GetQueuedAudioSize(audio_device) < AudioBufferSize*3)
			SDL_QueueAudio(audio_device, reinterpret_cast<const void *>(buffer.data()), static_cast<Uint32>(buffer.size() * sizeof(uint16_t)));
		updater->update();
	}

	SDL_AudioDeviceID audio_device;
	Concurrency::BestEffortUpdater *updater;
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

}

int main(int argc, char *argv[]) {
	SDL_Window *window = nullptr;

	// Perform a sanity check on arguments.
	if(argc < 2) {
		std::cerr << "Usage: " << argv[0] << " [file]" << std::endl;
		return -1;
	}

	// Determine the machine for the supplied file.
	std::list<StaticAnalyser::Target> targets = StaticAnalyser::GetTargets(argv[1]);
	if(targets.empty()) {
		std::cerr << "Cannot open " << argv[1] << std::endl;
		return -1;
	}

	Concurrency::BestEffortUpdater updater;
	BestEffortUpdaterDelegate best_effort_updater_delegate;
	CRTMachineDelegate crt_delegate;
	SpeakerDelegate speaker_delegate;

	// Create and configure a machine.
	std::unique_ptr<::Machine::DynamicMachine> machine(::Machine::MachineForTarget(targets.front()));

	updater.set_clock_rate(machine->crt_machine()->get_clock_rate());
	crt_delegate.best_effort_updater = &updater;
	best_effort_updater_delegate.machine = machine.get();
	speaker_delegate.updater = &updater;

	machine->crt_machine()->set_delegate(&crt_delegate);
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

	window = SDL_CreateWindow(	"Clock Signal",
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
	std::cout << "Target framebuffer has ID " << target_framebuffer << std::endl;

	// For vanilla SDL purposes, assume system ROMs can be found in one of:
	//
	//	/usr/local/share/CLK/[system]; or
	//	/usr/share/CLK/[system]
	bool roms_loaded = machine->crt_machine()->set_rom_fetcher( [] (const std::string &machine, const std::vector<std::string> &names) -> std::vector<std::unique_ptr<std::vector<uint8_t>>> {
		std::vector<std::unique_ptr<std::vector<uint8_t>>> results;
		for(auto &name: names) {
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
	});

	if(!roms_loaded) {
		std::cerr << "Could not find system ROMs; please install to /usr/local/share/CLK/ or /usr/share/CLK/" << std::endl;
		return -1;
	}

	machine->configuration_target()->configure_as_target(targets.front());

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
		desired_audio_spec.samples = AudioBufferSize;

		speaker_delegate.audio_device = SDL_OpenAudioDevice(nullptr, 0, &desired_audio_spec, &obtained_audio_spec, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);

		speaker->set_output_rate(obtained_audio_spec.freq, obtained_audio_spec.samples);
		speaker->set_delegate(&speaker_delegate);
		SDL_PauseAudioDevice(speaker_delegate.audio_device, 0);
	}

	int window_width, window_height;
	SDL_GetWindowSize(window, &window_width, &window_height);
	std::cout << "Window size is " << window_width << ", " << window_height << std::endl;

	// Establish user-friendly options by default.
	Configurable::Device *configurable_device = machine->configurable_device();
	if(configurable_device) {
		configurable_device->set_selections(configurable_device->get_user_friendly_selections());
	}

	// Run the main event loop until the OS tells us to quit.
	bool should_quit = false;
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

							std::cout << "Resized; framebuffer is " << target_framebuffer << ", and window size is " << window_width << ", " << window_height << std::endl;
						} break;

						default: break;
					}
				break;

				case SDL_KEYDOWN:
				case SDL_KEYUP: {
					KeyboardMachine::Machine *keyboard_machine = machine->keyboard_machine();
					if(!keyboard_machine) break;

					Inputs::Keyboard::Key key = Inputs::Keyboard::Key::Space;
					if(!KeyboardKeyForSDLScancode(event.key.keysym.scancode, key)) break;
					keyboard_machine->get_keyboard().set_key_pressed(key, event.type == SDL_KEYDOWN);
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
