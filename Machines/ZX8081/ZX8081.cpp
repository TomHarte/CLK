//
//  ZX8081.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "ZX8081.hpp"

#include "../../Processors/Z80/Z80.hpp"
#include "../../Storage/Tape/Tape.hpp"
#include "../../Storage/Tape/Parsers/ZX8081.hpp"

#include "../MemoryFuzzer.hpp"
#include "../Typer.hpp"

#include "Video.hpp"

#include <cstdint>
#include <vector>

namespace {
	// The clock rate is 3.25Mhz.
	const unsigned int ZX8081ClockRate = 3250000;
}

namespace ZX8081 {

class ConcreteMachine:
	public Utility::TypeRecipient,
	public CPU::Z80::BusHandler,
	public Machine {
	public:
		ConcreteMachine() :
			z80_(*this),
			vsync_(false),
			hsync_(false),
			nmi_is_enabled_(false),
			tape_player_(ZX8081ClockRate),
			use_fast_tape_hack_(false),
			tape_advance_delay_(0),
			has_latched_video_byte_(false) {
			set_clock_rate(ZX8081ClockRate);
			clear_all_keys();
		}

		HalfCycles perform_machine_cycle(const CPU::Z80::PartialMachineCycle &cycle) {
			HalfCycles previous_counter = horizontal_counter_;
			horizontal_counter_ += cycle.length;

			if(previous_counter < vsync_start_ && horizontal_counter_ >= vsync_start_) {
				video_->run_for(vsync_start_ - previous_counter);
				set_hsync(true);
				line_counter_ = (line_counter_ + 1) & 7;
				if(nmi_is_enabled_) {
					z80_.set_non_maskable_interrupt_line(true);
				}
				video_->run_for(horizontal_counter_ - vsync_start_);
			} else if(previous_counter < vsync_end_ && horizontal_counter_ >= vsync_end_) {
				video_->run_for(vsync_end_ - previous_counter);
				set_hsync(false);
				if(nmi_is_enabled_) {
					z80_.set_non_maskable_interrupt_line(false);
					z80_.set_wait_line(false);
				}
				video_->run_for(horizontal_counter_ - vsync_end_);
			} else {
				video_->run_for(cycle.length);
			}

			if(is_zx81_) horizontal_counter_ %= HalfCycles(Cycles(207));
			if(!tape_advance_delay_) {
				tape_player_.run_for(cycle.length);
			} else {
				tape_advance_delay_ = std::max(tape_advance_delay_ - cycle.length, HalfCycles(0));
			}

			if(nmi_is_enabled_ && !z80_.get_halt_line() && z80_.get_non_maskable_interrupt_line()) {
				z80_.set_wait_line(true);
			}

			if(!cycle.is_terminal()) {
				return Cycles(0);
			}

			uint16_t address = cycle.address ? *cycle.address : 0;
			bool is_opcode_read = false;
			switch(cycle.operation) {
				case CPU::Z80::PartialMachineCycle::Output:
					if(!(address & 2)) nmi_is_enabled_ = false;
					if(!(address & 1)) nmi_is_enabled_ = is_zx81_;
					if(!nmi_is_enabled_) {
						// Line counter reset is held low while vsync is active; simulate that lazily by performing
						// an instant reset upon the transition from active to inactive.
						if(vsync_) line_counter_ = 0;
						set_vsync(false);
					}
				break;

				case CPU::Z80::PartialMachineCycle::Input: {
					uint8_t value = 0xff;
					if(!(address&1)) {
						if(!nmi_is_enabled_) set_vsync(true);

						uint16_t mask = 0x100;
						for(int c = 0; c < 8; c++) {
							if(!(address & mask)) value &= key_states_[c];
							mask <<= 1;
						}

						value &= ~(tape_player_.get_input() ? 0x00 : 0x80);
					}
					*cycle.value = value;
				} break;

				case CPU::Z80::PartialMachineCycle::Interrupt:
					// resetting event is M1 and IOREQ both simultaneously having leading edges;
					// that happens 2 cycles before the end of INTACK. So the timer was reset and
					// now has advanced twice.
					horizontal_counter_ = HalfCycles(2);

					*cycle.value = 0xff;
				break;

				case CPU::Z80::PartialMachineCycle::Refresh:
					// The ZX80 and 81 signal an interrupt while refresh is active and bit 6 of the refresh
					// address is low. The Z80 signals a refresh, providing the refresh address during the
					// final two cycles of an opcode fetch. Therefore communicate a transient signalling
					// of the IRQ line if necessary.
					if(!(address & 0x40)) {
						z80_.set_interrupt_line(true, Cycles(-2));
						z80_.set_interrupt_line(false);
					}
					if(has_latched_video_byte_) {
						size_t char_address = (size_t)((address & 0xfe00) | ((latched_video_byte_ & 0x3f) << 3) | line_counter_);
						uint8_t mask = (latched_video_byte_ & 0x80) ? 0x00 : 0xff;
						if(char_address < ram_base_) {
							latched_video_byte_ = rom_[char_address & rom_mask_] ^ mask;
						} else {
							latched_video_byte_ = ram_[address & ram_mask_] ^ mask;
						}

						video_->output_byte(latched_video_byte_);
						has_latched_video_byte_ = false;
					}
				break;

				case CPU::Z80::PartialMachineCycle::ReadOpcode:
					// Check for use of the fast tape hack.
					if(use_fast_tape_hack_ && address == tape_trap_address_ && tape_player_.has_tape()) {
						uint64_t prior_offset = tape_player_.get_tape()->get_offset();
						int next_byte = parser_.get_next_byte(tape_player_.get_tape());
						if(next_byte != -1) {
							uint16_t hl = z80_.get_value_of_register(CPU::Z80::Register::HL);
							ram_[hl & ram_mask_] = (uint8_t)next_byte;
							*cycle.value = 0x00;
							z80_.set_value_of_register(CPU::Z80::Register::ProgramCounter, tape_return_address_ - 1);

							// Assume that having read one byte quickly, we're probably going to be asked to read
							// another shortly. Therefore, temporarily disable the tape motor for 1000 cycles in order
							// to avoid fighting with real time. This is a stop-gap fix.
							tape_advance_delay_ = 1000;
							return 0;
						} else {
							tape_player_.get_tape()->set_offset(prior_offset);
						}
					}

					// Check for automatic tape control.
					if(use_automatic_tape_motor_control_) {
						tape_player_.set_motor_control((address >= automatic_tape_motor_start_address_) && (address < automatic_tape_motor_end_address_));
					}
					is_opcode_read = true;

				case CPU::Z80::PartialMachineCycle::Read:
					if(address < ram_base_) {
						*cycle.value = rom_[address & rom_mask_];
					} else {
						uint8_t value = ram_[address & ram_mask_];

						// If this is an M1 cycle reading from above the 32kb mark and HALT is not
						// currently active, latch for video output and return a NOP. Otherwise,
						// just return the value as read.
						if(is_opcode_read && address&0x8000 && !(value & 0x40) && !z80_.get_halt_line()) {
							latched_video_byte_ = value;
							has_latched_video_byte_ = true;
							*cycle.value = 0;
						} else *cycle.value = value;
					}
				break;

				case CPU::Z80::PartialMachineCycle::Write:
					if(address >= ram_base_) {
						ram_[address & ram_mask_] = *cycle.value;
					}
				break;

				default: break;
			}

			if(typer_) typer_->run_for(cycle.length);

			return HalfCycles(0);
		}

		void flush() {
			video_->flush();
		}

		void setup_output(float aspect_ratio) {
			video_.reset(new Video);
		}

		void close_output() {
			video_.reset();
		}

		std::shared_ptr<Outputs::CRT::CRT> get_crt() {
			return video_->get_crt();
		}

		std::shared_ptr<Outputs::Speaker> get_speaker() {
			return nullptr;
		}

		void run_for(const Cycles cycles) {
			z80_.run_for(cycles);
		}

		void configure_as_target(const StaticAnalyser::Target &target) {
			is_zx81_ = target.zx8081.isZX81;
			if(is_zx81_) {
				rom_ = zx81_rom_;
				tape_trap_address_ = 0x37c;
				tape_return_address_ = 0x380;
				vsync_start_ = HalfCycles(32);
				vsync_end_ = HalfCycles(64);
				automatic_tape_motor_start_address_ = 0x0340;
				automatic_tape_motor_end_address_ = 0x03c3;
			} else {
				rom_ = zx80_rom_;
				tape_trap_address_ = 0x220;
				tape_return_address_ = 0x248;
				vsync_start_ = HalfCycles(26);
				vsync_end_ = HalfCycles(66);
				automatic_tape_motor_start_address_ = 0x0206;
				automatic_tape_motor_end_address_ = 0x024d;
			}
			rom_mask_ = (uint16_t)(rom_.size() - 1);

			switch(target.zx8081.memory_model) {
				case StaticAnalyser::ZX8081MemoryModel::Unexpanded:
					ram_.resize(1024);
					ram_base_ = 16384;
					ram_mask_ = 1023;
				break;
				case StaticAnalyser::ZX8081MemoryModel::SixteenKB:
					ram_.resize(16384);
					ram_base_ = 16384;
					ram_mask_ = 16383;
				break;
				case StaticAnalyser::ZX8081MemoryModel::SixtyFourKB:
					ram_.resize(65536);
					ram_base_ = 8192;
					ram_mask_ = 65535;
				break;
			}
			Memory::Fuzz(ram_);

			if(target.tapes.size()) {
				tape_player_.set_tape(target.tapes.front());
			}

			if(target.loadingCommand.length()) {
				set_typer_for_string(target.loadingCommand.c_str());
			}
		}

		void set_rom(ROMType type, std::vector<uint8_t> data) {
			switch(type) {
				case ZX80: zx80_rom_ = data; break;
				case ZX81: zx81_rom_ = data; break;
			}
		}

#pragma mark - Keyboard

		void set_key_state(uint16_t key, bool isPressed) {
			if(isPressed)
				key_states_[key >> 8] &= (uint8_t)(~key);
			else
				key_states_[key >> 8] |= (uint8_t)key;
		}

		void clear_all_keys() {
			memset(key_states_, 0xff, 8);
		}

#pragma mark - Tape control

		void set_use_fast_tape_hack(bool activate) {
			use_fast_tape_hack_ = activate;
		}

		void set_use_automatic_tape_motor_control(bool enabled) {
			use_automatic_tape_motor_control_ = enabled;
			if(!enabled) {
				tape_player_.set_motor_control(false);
			}
		}
		void set_tape_is_playing(bool is_playing) {
			tape_player_.set_motor_control(is_playing);
		}

#pragma mark - Typer

		// for Utility::TypeRecipient::Delegate
		HalfCycles get_typer_delay() { return Cycles(7000000); }
		HalfCycles get_typer_frequency() { return Cycles(390000); }

		uint16_t *sequence_for_character(Utility::Typer *typer, char character) {
#define KEYS(...)	{__VA_ARGS__, EndSequence}
#define SHIFT(...)	{KeyShift, __VA_ARGS__, EndSequence}
#define X			{NotMapped}
			static KeySequence zx81_key_sequences[] = {
				/* NUL */	X,							/* SOH */	X,
				/* STX */	X,							/* ETX */	X,
				/* EOT */	X,							/* ENQ */	X,
				/* ACK */	X,							/* BEL */	X,
				/* BS */	SHIFT(Key0),				/* HT */	X,
				/* LF */	KEYS(KeyEnter),				/* VT */	X,
				/* FF */	X,							/* CR */	X,
				/* SO */	X,							/* SI */	X,
				/* DLE */	X,							/* DC1 */	X,
				/* DC2 */	X,							/* DC3 */	X,
				/* DC4 */	X,							/* NAK */	X,
				/* SYN */	X,							/* ETB */	X,
				/* CAN */	X,							/* EM */	X,
				/* SUB */	X,							/* ESC */	X,
				/* FS */	X,							/* GS */	X,
				/* RS */	X,							/* US */	X,
				/* space */	KEYS(KeySpace),				/* ! */		X,
				/* " */		SHIFT(KeyP),				/* # */		X,
				/* $ */		SHIFT(KeyU),				/* % */		X,
				/* & */		X,							/* ' */		X,
				/* ( */		SHIFT(KeyI),				/* ) */		SHIFT(KeyO),
				/* * */		SHIFT(KeyB),				/* + */		SHIFT(KeyK),
				/* , */		SHIFT(KeyDot),				/* - */		SHIFT(KeyJ),
				/* . */		KEYS(KeyDot),				/* / */		SHIFT(KeyV),
				/* 0 */		KEYS(Key0),					/* 1 */		KEYS(Key1),
				/* 2 */		KEYS(Key2),					/* 3 */		KEYS(Key3),
				/* 4 */		KEYS(Key4),					/* 5 */		KEYS(Key5),
				/* 6 */		KEYS(Key6),					/* 7 */		KEYS(Key7),
				/* 8 */		KEYS(Key8),					/* 9 */		KEYS(Key9),
				/* : */		SHIFT(KeyZ),				/* ; */		SHIFT(KeyX),
				/* < */		SHIFT(KeyN),				/* = */		SHIFT(KeyL),
				/* > */		SHIFT(KeyM),				/* ? */		SHIFT(KeyC),
				/* @ */		X,							/* A */		KEYS(KeyA),
				/* B */		KEYS(KeyB),					/* C */		KEYS(KeyC),
				/* D */		KEYS(KeyD),					/* E */		KEYS(KeyE),
				/* F */		KEYS(KeyF),					/* G */		KEYS(KeyG),
				/* H */		KEYS(KeyH),					/* I */		KEYS(KeyI),
				/* J */		KEYS(KeyJ),					/* K */		KEYS(KeyK),
				/* L */		KEYS(KeyL),					/* M */		KEYS(KeyM),
				/* N */		KEYS(KeyN),					/* O */		KEYS(KeyO),
				/* P */		KEYS(KeyP),					/* Q */		KEYS(KeyQ),
				/* R */		KEYS(KeyR),					/* S */		KEYS(KeyS),
				/* T */		KEYS(KeyT),					/* U */		KEYS(KeyU),
				/* V */		KEYS(KeyV),					/* W */		KEYS(KeyW),
				/* X */		KEYS(KeyX),					/* Y */		KEYS(KeyY),
				/* Z */		KEYS(KeyZ),					/* [ */		X,
				/* \ */		X,							/* ] */		X,
				/* ^ */		X,							/* _ */		X,
				/* ` */		X,							/* a */		KEYS(KeyA),
				/* b */		KEYS(KeyB),					/* c */		KEYS(KeyC),
				/* d */		KEYS(KeyD),					/* e */		KEYS(KeyE),
				/* f */		KEYS(KeyF),					/* g */		KEYS(KeyG),
				/* h */		KEYS(KeyH),					/* i */		KEYS(KeyI),
				/* j */		KEYS(KeyJ),					/* k */		KEYS(KeyK),
				/* l */		KEYS(KeyL),					/* m */		KEYS(KeyM),
				/* n */		KEYS(KeyN),					/* o */		KEYS(KeyO),
				/* p */		KEYS(KeyP),					/* q */		KEYS(KeyQ),
				/* r */		KEYS(KeyR),					/* s */		KEYS(KeyS),
				/* t */		KEYS(KeyT),					/* u */		KEYS(KeyU),
				/* v */		KEYS(KeyV),					/* w */		KEYS(KeyW),
				/* x */		KEYS(KeyX),					/* y */		KEYS(KeyY),
				/* z */		KEYS(KeyZ),					/* { */		X,
				/* | */		X,							/* } */		X,
			};

			static KeySequence zx80_key_sequences[] = {
				/* NUL */	X,							/* SOH */	X,
				/* STX */	X,							/* ETX */	X,
				/* EOT */	X,							/* ENQ */	X,
				/* ACK */	X,							/* BEL */	X,
				/* BS */	SHIFT(Key0),				/* HT */	X,
				/* LF */	KEYS(KeyEnter),				/* VT */	X,
				/* FF */	X,							/* CR */	X,
				/* SO */	X,							/* SI */	X,
				/* DLE */	X,							/* DC1 */	X,
				/* DC2 */	X,							/* DC3 */	X,
				/* DC4 */	X,							/* NAK */	X,
				/* SYN */	X,							/* ETB */	X,
				/* CAN */	X,							/* EM */	X,
				/* SUB */	X,							/* ESC */	X,
				/* FS */	X,							/* GS */	X,
				/* RS */	X,							/* US */	X,
				/* space */	KEYS(KeySpace),				/* ! */		X,
				/* " */		SHIFT(KeyY),				/* # */		X,
				/* $ */		SHIFT(KeyU),				/* % */		X,
				/* & */		X,							/* ' */		X,
				/* ( */		SHIFT(KeyI),				/* ) */		SHIFT(KeyO),
				/* * */		SHIFT(KeyP),				/* + */		SHIFT(KeyK),
				/* , */		SHIFT(KeyDot),				/* - */		SHIFT(KeyJ),
				/* . */		KEYS(KeyDot),				/* / */		SHIFT(KeyV),
				/* 0 */		KEYS(Key0),					/* 1 */		KEYS(Key1),
				/* 2 */		KEYS(Key2),					/* 3 */		KEYS(Key3),
				/* 4 */		KEYS(Key4),					/* 5 */		KEYS(Key5),
				/* 6 */		KEYS(Key6),					/* 7 */		KEYS(Key7),
				/* 8 */		KEYS(Key8),					/* 9 */		KEYS(Key9),
				/* : */		SHIFT(KeyZ),				/* ; */		SHIFT(KeyX),
				/* < */		SHIFT(KeyN),				/* = */		SHIFT(KeyL),
				/* > */		SHIFT(KeyM),				/* ? */		SHIFT(KeyC),
				/* @ */		X,							/* A */		KEYS(KeyA),
				/* B */		KEYS(KeyB),					/* C */		KEYS(KeyC),
				/* D */		KEYS(KeyD),					/* E */		KEYS(KeyE),
				/* F */		KEYS(KeyF),					/* G */		KEYS(KeyG),
				/* H */		KEYS(KeyH),					/* I */		KEYS(KeyI),
				/* J */		KEYS(KeyJ),					/* K */		KEYS(KeyK),
				/* L */		KEYS(KeyL),					/* M */		KEYS(KeyM),
				/* N */		KEYS(KeyN),					/* O */		KEYS(KeyO),
				/* P */		KEYS(KeyP),					/* Q */		KEYS(KeyQ),
				/* R */		KEYS(KeyR),					/* S */		KEYS(KeyS),
				/* T */		KEYS(KeyT),					/* U */		KEYS(KeyU),
				/* V */		KEYS(KeyV),					/* W */		KEYS(KeyW),
				/* X */		KEYS(KeyX),					/* Y */		KEYS(KeyY),
				/* Z */		KEYS(KeyZ),					/* [ */		X,
				/* \ */		X,							/* ] */		X,
				/* ^ */		X,							/* _ */		X,
				/* ` */		X,							/* a */		KEYS(KeyA),
				/* b */		KEYS(KeyB),					/* c */		KEYS(KeyC),
				/* d */		KEYS(KeyD),					/* e */		KEYS(KeyE),
				/* f */		KEYS(KeyF),					/* g */		KEYS(KeyG),
				/* h */		KEYS(KeyH),					/* i */		KEYS(KeyI),
				/* j */		KEYS(KeyJ),					/* k */		KEYS(KeyK),
				/* l */		KEYS(KeyL),					/* m */		KEYS(KeyM),
				/* n */		KEYS(KeyN),					/* o */		KEYS(KeyO),
				/* p */		KEYS(KeyP),					/* q */		KEYS(KeyQ),
				/* r */		KEYS(KeyR),					/* s */		KEYS(KeyS),
				/* t */		KEYS(KeyT),					/* u */		KEYS(KeyU),
				/* v */		KEYS(KeyV),					/* w */		KEYS(KeyW),
				/* x */		KEYS(KeyX),					/* y */		KEYS(KeyY),
				/* z */		KEYS(KeyZ),					/* { */		X,
				/* | */		X,							/* } */		X,
			};
#undef KEYS
#undef SHIFT
#undef X

			if(is_zx81_)
				return table_lookup_sequence_for_character(zx81_key_sequences, sizeof(zx81_key_sequences), character);
			else
				return table_lookup_sequence_for_character(zx80_key_sequences, sizeof(zx80_key_sequences), character);
		}

	private:
		CPU::Z80::Processor<ConcreteMachine> z80_;

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

		HalfClockReceiver<Storage::Tape::BinaryTapePlayer> tape_player_;
		Storage::Tape::ZX8081::Parser parser_;

		bool is_zx81_;
		bool nmi_is_enabled_;

		HalfCycles vsync_start_, vsync_end_;
		HalfCycles horizontal_counter_;

		uint8_t latched_video_byte_;
		bool has_latched_video_byte_;

		bool use_fast_tape_hack_;
		bool use_automatic_tape_motor_control_;
		HalfCycles tape_advance_delay_;

#pragma mark - Video

		void set_vsync(bool sync) {
			vsync_ = sync;
			update_sync();
		}

		void set_hsync(bool sync) {
			hsync_ = sync;
			update_sync();
		}

		void update_sync() {
			video_->set_sync(vsync_ || hsync_);
		}
};

}

using namespace ZX8081;

// See header; constructs and returns an instance of the ZX80/81.
Machine *Machine::ZX8081() {
	return new ZX8081::ConcreteMachine;
}
