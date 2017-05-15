//
//  Electron.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Electron_hpp
#define Electron_hpp

#include "../../Processors/6502/6502.hpp"
#include "../../Storage/Tape/Tape.hpp"

#include "../ConfigurationTarget.hpp"
#include "../CRTMachine.hpp"
#include "../Typer.hpp"

#include "Interrupts.hpp"
#include "Plus3.hpp"
#include "Speaker.hpp"
#include "Tape.hpp"
#include "Video.hpp"

#include <cstdint>
#include <vector>

namespace Electron {

enum ROMSlot: uint8_t {
	ROMSlot0 = 0,
	ROMSlot1,	ROMSlot2,	ROMSlot3,
	ROMSlot4,	ROMSlot5,	ROMSlot6,	ROMSlot7,

	ROMSlotKeyboard = 8,	ROMSlot9,
	ROMSlotBASIC = 10,		ROMSlot11,

	ROMSlot12,	ROMSlot13,	ROMSlot14,	ROMSlot15,

	ROMSlotOS,	ROMSlotDFS,	ROMSlotADFS
};

enum Key: uint16_t {
	KeySpace		= 0x0000 | 0x08,										KeyCopy			= 0x0000 | 0x02,	KeyRight		= 0x0000 | 0x01,
	KeyDelete		= 0x0010 | 0x08,	KeyReturn		= 0x0010 | 0x04,	KeyDown			= 0x0010 | 0x02,	KeyLeft			= 0x0010 | 0x01,
										KeyColon		= 0x0020 | 0x04,	KeyUp			= 0x0020 | 0x02,	KeyMinus		= 0x0020 | 0x01,
	KeySlash		= 0x0030 | 0x08,	KeySemiColon	= 0x0030 | 0x04,	KeyP			= 0x0030 | 0x02,	Key0			= 0x0030 | 0x01,
	KeyFullStop		= 0x0040 | 0x08,	KeyL			= 0x0040 | 0x04,	KeyO			= 0x0040 | 0x02,	Key9			= 0x0040 | 0x01,
	KeyComma		= 0x0050 | 0x08,	KeyK			= 0x0050 | 0x04,	KeyI			= 0x0050 | 0x02,	Key8			= 0x0050 | 0x01,
	KeyM			= 0x0060 | 0x08,	KeyJ			= 0x0060 | 0x04,	KeyU			= 0x0060 | 0x02,	Key7			= 0x0060 | 0x01,
	KeyN			= 0x0070 | 0x08,	KeyH			= 0x0070 | 0x04,	KeyY			= 0x0070 | 0x02,	Key6			= 0x0070 | 0x01,
	KeyB			= 0x0080 | 0x08,	KeyG			= 0x0080 | 0x04,	KeyT			= 0x0080 | 0x02,	Key5			= 0x0080 | 0x01,
	KeyV			= 0x0090 | 0x08,	KeyF			= 0x0090 | 0x04,	KeyR			= 0x0090 | 0x02,	Key4			= 0x0090 | 0x01,
	KeyC			= 0x00a0 | 0x08,	KeyD			= 0x00a0 | 0x04,	KeyE			= 0x00a0 | 0x02,	Key3			= 0x00a0 | 0x01,
	KeyX			= 0x00b0 | 0x08,	KeyS			= 0x00b0 | 0x04,	KeyW			= 0x00b0 | 0x02,	Key2			= 0x00b0 | 0x01,
	KeyZ			= 0x00c0 | 0x08,	KeyA			= 0x00c0 | 0x04,	KeyQ			= 0x00c0 | 0x02,	Key1			= 0x00c0 | 0x01,
	KeyShift		= 0x00d0 | 0x08,	KeyControl		= 0x00d0 | 0x04,	KeyFunc			= 0x00d0 | 0x02,	KeyEscape		= 0x00d0 | 0x01,

	KeyBreak		= 0xfffd,

	TerminateSequence = 0xffff, NotMapped		= 0xfffe,
};

/*!
	@abstract Represents an Acorn Electron.
	
	@discussion An instance of Electron::Machine represents the current state of an
	Acorn Electron.
*/
class Machine:
	public CPU::MOS6502::Processor<Machine>,
	public CRTMachine::Machine,
	public Tape::Delegate,
	public Utility::TypeRecipient,
	public ConfigurationTarget::Machine {

	public:
		Machine();

		void set_rom(ROMSlot slot, std::vector<uint8_t> data, bool is_writeable);

		void set_key_state(uint16_t key, bool isPressed);
		void clear_all_keys();

		inline void set_use_fast_tape_hack(bool activate) { use_fast_tape_hack_ = activate; }

		// to satisfy ConfigurationTarget::Machine
		void configure_as_target(const StaticAnalyser::Target &target);

		// to satisfy CPU::MOS6502::Processor
		unsigned int perform_bus_operation(CPU::MOS6502::BusOperation operation, uint16_t address, uint8_t *value);
		void synchronise();

		// to satisfy CRTMachine::Machine
		virtual void setup_output(float aspect_ratio);
		virtual void close_output();
		virtual std::shared_ptr<Outputs::CRT::CRT> get_crt();
		virtual std::shared_ptr<Outputs::Speaker> get_speaker();
		virtual void run_for_cycles(int number_of_cycles) { CPU::MOS6502::Processor<Machine>::run_for_cycles(number_of_cycles); }

		// to satisfy Tape::Delegate
		virtual void tape_did_change_interrupt_status(Tape *tape);

		// for Utility::TypeRecipient
		virtual int get_typer_delay();
		virtual int get_typer_frequency();
		uint16_t *sequence_for_character(Utility::Typer *typer, char character);

	private:
		inline void update_display();
		inline void queue_next_display_interrupt();
		inline void update_audio();

		inline void signal_interrupt(Interrupt interrupt);
		inline void clear_interrupt(Interrupt interrupt);
		inline void evaluate_interrupts();

		// Things that directly constitute the memory map.
		uint8_t roms_[16][16384];
		bool rom_write_masks_[16];
		uint8_t os_[16384], ram_[32768];
		std::vector<uint8_t> dfs_, adfs_;

		// Paging
		ROMSlot active_rom_;
		bool keyboard_is_active_, basic_is_active_;

		// Interrupt and keyboard state
		uint8_t interrupt_status_, interrupt_control_;
		uint8_t key_states_[14];

		// Counters related to simultaneous subsystems
		unsigned int cycles_since_display_update_;
		unsigned int cycles_since_audio_update_;
		int cycles_until_display_interrupt_;
		Interrupt next_display_interrupt_;
		VideoOutput::Range video_access_range_;

		// Tape
		Tape tape_;
		bool use_fast_tape_hack_;
		bool fast_load_is_in_data_;

		// Disk
		std::unique_ptr<Plus3> plus3_;
		bool is_holding_shift_;
		int shift_restart_counter_;

		// Outputs
		std::unique_ptr<VideoOutput> video_output_;
		std::shared_ptr<Speaker> speaker_;
		bool speaker_is_enabled_;
};

}

#endif /* Electron_hpp */
