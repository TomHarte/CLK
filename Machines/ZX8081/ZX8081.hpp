//
//  ZX8081.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef ZX8081_hpp
#define ZX8081_hpp

#include "../ConfigurationTarget.hpp"
#include "../CRTMachine.hpp"

#include "../../Processors/Z80/Z80.hpp"
#include "../../Storage/Tape/Tape.hpp"
#include "../../Storage/Tape/Parsers/ZX8081.hpp"

#include "Video.hpp"

#include <cstdint>
#include <vector>

namespace ZX8081 {

enum ROMType: uint8_t {
	ZX80, ZX81
};

enum Key: uint16_t {
	KeyShift	= 0x0000 | 0x01,	KeyZ	= 0x0000 | 0x02,	KeyX = 0x0000 | 0x04,	KeyC = 0x0000 | 0x08,	KeyV = 0x0000 | 0x10,
	KeyA		= 0x0100 | 0x01,	KeyS	= 0x0100 | 0x02,	KeyD = 0x0100 | 0x04,	KeyF = 0x0100 | 0x08,	KeyG = 0x0100 | 0x10,
	KeyQ		= 0x0200 | 0x01,	KeyW	= 0x0200 | 0x02,	KeyE = 0x0200 | 0x04,	KeyR = 0x0200 | 0x08,	KeyT = 0x0200 | 0x10,
	Key1		= 0x0300 | 0x01,	Key2	= 0x0300 | 0x02,	Key3 = 0x0300 | 0x04,	Key4 = 0x0300 | 0x08,	Key5 = 0x0300 | 0x10,
	Key0		= 0x0400 | 0x01,	Key9	= 0x0400 | 0x02,	Key8 = 0x0400 | 0x04,	Key7 = 0x0400 | 0x08,	Key6 = 0x0400 | 0x10,
	KeyP		= 0x0500 | 0x01,	KeyO	= 0x0500 | 0x02,	KeyI = 0x0500 | 0x04,	KeyU = 0x0500 | 0x08,	KeyY = 0x0500 | 0x10,
	KeyEnter	= 0x0600 | 0x01,	KeyL	= 0x0600 | 0x02,	KeyK = 0x0600 | 0x04,	KeyJ = 0x0600 | 0x08,	KeyH = 0x0600 | 0x10,
	KeySpace	= 0x0700 | 0x01,	KeyDot	= 0x0700 | 0x02,	KeyM = 0x0700 | 0x04,	KeyN = 0x0700 | 0x08,	KeyB = 0x0700 | 0x10,
};

class Machine:
	public CPU::Z80::Processor<Machine>,
	public CRTMachine::Machine,
	public ConfigurationTarget::Machine {
	public:
		Machine();

		int perform_machine_cycle(const CPU::Z80::PartialMachineCycle &cycle);
		void flush();

		void setup_output(float aspect_ratio);
		void close_output();

		std::shared_ptr<Outputs::CRT::CRT> get_crt();
		std::shared_ptr<Outputs::Speaker> get_speaker();

		void run_for_cycles(int number_of_cycles);

		void configure_as_target(const StaticAnalyser::Target &target);

		void set_rom(ROMType type, std::vector<uint8_t> data);
		void set_key_state(uint16_t key, bool isPressed);
		void clear_all_keys();

		inline void set_use_fast_tape_hack(bool activate) { use_fast_tape_hack_ = activate; }
		inline void set_use_automatic_tape_motor_control(bool enabled) {
			use_automatic_tape_motor_control_ = enabled;
			if(!enabled) tape_is_automatically_playing_ = false;
		}
		inline void set_tape_is_playing(bool is_playing) { tape_is_playing_ = is_playing; }

	private:
		std::shared_ptr<Video> video_;
		std::vector<uint8_t> zx81_rom_, zx80_rom_;

		uint16_t tape_trap_address_, tape_return_address_;
		uint16_t automatic_tape_motor_start_address_, automatic_tape_motor_end_address_;

		std::vector<uint8_t> ram_;
		uint16_t ram_mask_, ram_base_;

		std::vector<uint8_t> rom_;
		uint16_t rom_mask_;

		bool vsync_, hsync_;
		int line_counter_;

		uint8_t key_states_[8];

		void set_vsync(bool sync);
		void set_hsync(bool sync);
		void update_sync();

		Storage::Tape::BinaryTapePlayer tape_player_;
		Storage::Tape::ZX8081::Parser parser_;

		int horizontal_counter_;
		bool is_zx81_;
		bool nmi_is_enabled_;
		int vsync_start_cycle_, vsync_end_cycle_;

		uint8_t latched_video_byte_;
		bool has_latched_video_byte_;

		bool use_fast_tape_hack_;
		bool use_automatic_tape_motor_control_;
		bool tape_is_playing_, tape_is_automatically_playing_;
		int tape_advance_delay_;
};

}

#endif /* ZX8081_hpp */
