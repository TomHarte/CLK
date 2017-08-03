//
//  Oric.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/10/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Oric_hpp
#define Oric_hpp

#include "../ConfigurationTarget.hpp"
#include "../CRTMachine.hpp"
#include "../Typer.hpp"

#include "../../Processors/6502/6502.hpp"
#include "../../Components/6522/6522.hpp"
#include "../../Components/AY38910/AY38910.hpp"
#include "../../Storage/Tape/Parsers/Oric.hpp"

#include "Video.hpp"
#include "Microdisc.hpp"

#include "../../Storage/Tape/Tape.hpp"

#include <cstdint>
#include <vector>
#include <memory>

namespace Oric {

enum Key: uint16_t {
	Key3			= 0x0000 | 0x80,	KeyX			= 0x0000 | 0x40,	Key1			= 0x0000 | 0x20,
	KeyV			= 0x0000 | 0x08,	Key5			= 0x0000 | 0x04,	KeyN			= 0x0000 | 0x02,	Key7			= 0x0000 | 0x01,
	KeyD			= 0x0100 | 0x80,	KeyQ			= 0x0100 | 0x40,	KeyEscape		= 0x0100 | 0x20,
	KeyF			= 0x0100 | 0x08,	KeyR			= 0x0100 | 0x04,	KeyT			= 0x0100 | 0x02,	KeyJ			= 0x0100 | 0x01,
	KeyC			= 0x0200 | 0x80,	Key2			= 0x0200 | 0x40,	KeyZ			= 0x0200 | 0x20,	KeyControl		= 0x0200 | 0x10,
	Key4			= 0x0200 | 0x08,	KeyB			= 0x0200 | 0x04,	Key6			= 0x0200 | 0x02,	KeyM			= 0x0200 | 0x01,
	KeyQuote		= 0x0300 | 0x80,	KeyBackSlash	= 0x0300 | 0x40,
	KeyMinus		= 0x0300 | 0x08,	KeySemiColon	= 0x0300 | 0x04,	Key9			= 0x0300 | 0x02,	KeyK			= 0x0300 | 0x01,
	KeyRight		= 0x0400 | 0x80,	KeyDown			= 0x0400 | 0x40,	KeyLeft			= 0x0400 | 0x20,	KeyLeftShift	= 0x0400 | 0x10,
	KeyUp			= 0x0400 | 0x08,	KeyFullStop		= 0x0400 | 0x04,	KeyComma		= 0x0400 | 0x02,	KeySpace		= 0x0400 | 0x01,
	KeyOpenSquare	= 0x0500 | 0x80,	KeyCloseSquare	= 0x0500 | 0x40,	KeyDelete		= 0x0500 | 0x20,	KeyFunction		= 0x0500 | 0x10,
	KeyP			= 0x0500 | 0x08,	KeyO			= 0x0500 | 0x04,	KeyI			= 0x0500 | 0x02,	KeyU			= 0x0500 | 0x01,
	KeyW			= 0x0600 | 0x80,	KeyS			= 0x0600 | 0x40,	KeyA			= 0x0600 | 0x20,
	KeyE			= 0x0600 | 0x08,	KeyG			= 0x0600 | 0x04,	KeyH			= 0x0600 | 0x02,	KeyY			= 0x0600 | 0x01,
	KeyEquals		= 0x0700 | 0x80,										KeyReturn		= 0x0700 | 0x20,	KeyRightShift	= 0x0700 | 0x10,
	KeyForwardSlash	= 0x0700 | 0x08,	Key0			= 0x0700 | 0x04,	KeyL			= 0x0700 | 0x02,	Key8			= 0x0700 | 0x01,

	KeyNMI			= 0xfffd,
};

enum ROM {
	BASIC10, BASIC11, Microdisc, Colour
};

class Machine:
	public CPU::MOS6502::Processor<Machine>,
	public CRTMachine::Machine,
	public ConfigurationTarget::Machine,
	public MOS::MOS6522IRQDelegate::Delegate,
	public Utility::TypeRecipient,
	public Storage::Tape::BinaryTapePlayer::Delegate,
	public Microdisc::Delegate {

	public:
		Machine();

		void set_rom(ROM rom, const std::vector<uint8_t> &data);
		void set_key_state(uint16_t key, bool isPressed);
		void clear_all_keys();

		void set_use_fast_tape_hack(bool activate);
		void set_output_device(Outputs::CRT::OutputDevice output_device);

		// to satisfy ConfigurationTarget::Machine
		void configure_as_target(const StaticAnalyser::Target &target);

		// to satisfy CPU::MOS6502::Processor
		Cycles perform_bus_operation(CPU::MOS6502::BusOperation operation, uint16_t address, uint8_t *value);
		void flush();

		// to satisfy CRTMachine::Machine
		virtual void setup_output(float aspect_ratio);
		virtual void close_output();
		virtual std::shared_ptr<Outputs::CRT::CRT> get_crt();
		virtual std::shared_ptr<Outputs::Speaker> get_speaker();
		virtual void run_for(const Cycles cycles);

		// to satisfy MOS::MOS6522IRQDelegate::Delegate
		void mos6522_did_change_interrupt_status(void *mos6522);

		// to satisfy Storage::Tape::BinaryTapePlayer::Delegate
		void tape_did_change_input(Storage::Tape::BinaryTapePlayer *tape_player);

		// for Utility::TypeRecipient::Delegate
		void set_typer_for_string(const char *string);

		// for Microdisc::Delegate
		void microdisc_did_change_paging_flags(class Microdisc *microdisc);
		void wd1770_did_change_output(WD::WD1770 *wd1770);

	private:
		// RAM and ROM
		std::vector<uint8_t> basic11_rom_, basic10_rom_, microdisc_rom_, colour_rom_;
		uint8_t ram_[65536], rom_[16384];
		Cycles cycles_since_video_update_;
		inline void update_video();

		// ROM bookkeeping
		bool is_using_basic11_;
		uint16_t tape_get_byte_address_, scan_keyboard_address_, tape_speed_address_;
		int keyboard_read_count_;

		// Outputs
		std::unique_ptr<VideoOutput> video_output_;

		// Keyboard
		class Keyboard {
			public:
				uint8_t row;
				uint8_t rows[8];
		};
		int typer_delay_;

		// The tape
		class TapePlayer: public Storage::Tape::BinaryTapePlayer {
			public:
				TapePlayer();
				uint8_t get_next_byte(bool fast);

			private:
				Storage::Tape::Oric::Parser parser_;
		};
		bool use_fast_tape_hack_;

		// VIA (which owns the tape and the AY)
		class VIA: public MOS::MOS6522<VIA>, public MOS::MOS6522IRQDelegate {
			public:
				VIA();
				using MOS6522IRQDelegate::set_interrupt_status;

				void set_control_line_output(Port port, Line line, bool value);
				void set_port_output(Port port, uint8_t value, uint8_t direction_mask);
				uint8_t get_port_input(Port port);
				inline void run_for(const Cycles cycles);

				std::shared_ptr<GI::AY38910> ay8910;
				std::unique_ptr<TapePlayer> tape;
				std::shared_ptr<Keyboard> keyboard;

				void flush();

			private:
				void update_ay();
				bool ay_bdir_, ay_bc1_;
				Cycles cycles_since_ay_update_;
		};
		VIA via_;
		std::shared_ptr<Keyboard> keyboard_;

		// the Microdisc, if in use
		class Microdisc microdisc_;
		bool microdisc_is_enabled_;
		uint16_t ram_top_;
		uint8_t *paged_rom_;

		inline void set_interrupt_line();
};

}
#endif /* Oric_hpp */
