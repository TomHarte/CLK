//
//  Oric.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/10/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Oric.hpp"

#include "Video.hpp"
#include "Microdisc.hpp"
#include "CharacterMapper.hpp"

#include "../MemoryFuzzer.hpp"
#include "../Typer.hpp"

#include "../../Processors/6502/6502.hpp"
#include "../../Components/6522/6522.hpp"
#include "../../Components/AY38910/AY38910.hpp"

#include "../../Storage/Tape/Tape.hpp"
#include "../../Storage/Tape/Parsers/Oric.hpp"

#include "../../ClockReceiver/ForceInline.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace Oric {

class ConcreteMachine:
	public CPU::MOS6502::BusHandler,
	public MOS::MOS6522IRQDelegate::Delegate,
	public Utility::TypeRecipient,
	public Storage::Tape::BinaryTapePlayer::Delegate,
	public Microdisc::Delegate,
	public Machine {

	public:
		ConcreteMachine() :
				m6502_(*this),
				use_fast_tape_hack_(false),
				typer_delay_(2500000),
				keyboard_read_count_(0),
				keyboard_(new Keyboard),
				ram_top_(0xbfff),
				paged_rom_(rom_),
				microdisc_is_enabled_(false) {
			set_clock_rate(1000000);
			via_.set_interrupt_delegate(this);
			via_.keyboard = keyboard_;
			clear_all_keys();
			via_.tape->set_delegate(this);
			Memory::Fuzz(ram_, sizeof(ram_));
		}

		void set_rom(ROM rom, const std::vector<uint8_t> &data) override final {
			switch(rom) {
				case BASIC11:	basic11_rom_ = std::move(data);		break;
				case BASIC10:	basic10_rom_ = std::move(data);		break;
				case Microdisc:	microdisc_rom_ = std::move(data);	break;
				case Colour:
					colour_rom_ = std::move(data);
					if(video_output_) video_output_->set_colour_rom(colour_rom_);
				break;
			}
		}

		void set_key_state(uint16_t key, bool isPressed) override final {
			if(key == KeyNMI) {
				m6502_.set_nmi_line(isPressed);
			} else {
				if(isPressed)
					keyboard_->rows[key >> 8] |= (key & 0xff);
				else
					keyboard_->rows[key >> 8] &= ~(key & 0xff);
			}
		}

		void clear_all_keys() override final {
			memset(keyboard_->rows, 0, sizeof(keyboard_->rows));
		}

		void set_use_fast_tape_hack(bool activate) override final {
			use_fast_tape_hack_ = activate;
		}

		void set_output_device(Outputs::CRT::OutputDevice output_device) override final {
			video_output_->set_output_device(output_device);
		}

		// to satisfy ConfigurationTarget::Machine
		void configure_as_target(const StaticAnalyser::Target &target) override final {
			if(target.oric.has_microdisc) {
				microdisc_is_enabled_ = true;
				microdisc_did_change_paging_flags(&microdisc_);
				microdisc_.set_delegate(this);
			}

			if(target.loadingCommand.length()) {
				set_typer_for_string(target.loadingCommand.c_str());
			}

			if(target.oric.use_atmos_rom) {
				memcpy(rom_, basic11_rom_.data(), std::min(basic11_rom_.size(), sizeof(rom_)));

				is_using_basic11_ = true;
				tape_get_byte_address_ = 0xe6c9;
				scan_keyboard_address_ = 0xf495;
				tape_speed_address_ = 0x024d;
			} else {
				memcpy(rom_, basic10_rom_.data(), std::min(basic10_rom_.size(), sizeof(rom_)));

				is_using_basic11_ = false;
				tape_get_byte_address_ = 0xe630;
				scan_keyboard_address_ = 0xf43c;
				tape_speed_address_ = 0x67;
			}

			insert_media(target.media);
		}

		bool insert_media(const StaticAnalyser::Media &media) override final {
			if(media.tapes.size()) {
				via_.tape->set_tape(media.tapes.front());
			}

			int drive_index = 0;
			for(auto disk : media.disks) {
				if(drive_index < 4) microdisc_.set_disk(disk, drive_index);
				drive_index++;
			}

			return !media.tapes.empty() || (!media.disks.empty() && microdisc_is_enabled_);
		}

		// to satisfy CPU::MOS6502::BusHandler
		forceinline Cycles perform_bus_operation(CPU::MOS6502::BusOperation operation, uint16_t address, uint8_t *value) {
			if(address > ram_top_) {
				if(isReadOperation(operation)) *value = paged_rom_[address - ram_top_ - 1];

				// 024D = 0 => fast; otherwise slow
				// E6C9 = read byte: return byte in A
				if(address == tape_get_byte_address_ && paged_rom_ == rom_ && use_fast_tape_hack_ && operation == CPU::MOS6502::BusOperation::ReadOpcode && via_.tape->has_tape() && !via_.tape->get_tape()->is_at_end()) {
					uint8_t next_byte = via_.tape->get_next_byte(!ram_[tape_speed_address_]);
					m6502_.set_value_of_register(CPU::MOS6502::A, next_byte);
					m6502_.set_value_of_register(CPU::MOS6502::Flags, next_byte ? 0 : CPU::MOS6502::Flag::Zero);
					*value = 0x60; // i.e. RTS
				}
			} else {
				if((address & 0xff00) == 0x0300) {
					if(microdisc_is_enabled_ && address >= 0x0310) {
						switch(address) {
							case 0x0310: case 0x0311: case 0x0312: case 0x0313:
								if(isReadOperation(operation)) *value = microdisc_.get_register(address);
								else microdisc_.set_register(address, *value);
							break;
							case 0x314: case 0x315: case 0x316: case 0x317:
								if(isReadOperation(operation)) *value = microdisc_.get_interrupt_request_register();
								else microdisc_.set_control_register(*value);
							break;
							case 0x318: case 0x319: case 0x31a: case 0x31b:
								if(isReadOperation(operation)) *value = microdisc_.get_data_request_register();
							break;
						}
					} else {
						if(isReadOperation(operation)) *value = via_.get_register(address);
						else via_.set_register(address, *value);
					}
				} else {
					if(isReadOperation(operation))
						*value = ram_[address];
					else {
						if(address >= 0x9800 && address <= 0xc000) { update_video(); typer_delay_ = 0; }
						ram_[address] = *value;
					}
				}
			}

			if(typer_ && address == scan_keyboard_address_ && operation == CPU::MOS6502::BusOperation::ReadOpcode) {
				// the Oric 1 misses any key pressed on the very first entry into the read keyboard routine, so don't
				// do anything until at least the second, regardless of machine
				if(!keyboard_read_count_) keyboard_read_count_++;
				else if(!typer_->type_next_character()) {
					clear_all_keys();
					typer_.reset();
				}
			}

			via_.run_for(Cycles(1));
			if(microdisc_is_enabled_) microdisc_.run_for(Cycles(8));
			cycles_since_video_update_++;
			return Cycles(1);
		}

		forceinline void flush() {
			update_video();
			via_.flush();
		}

		// to satisfy CRTMachine::Machine
		void setup_output(float aspect_ratio) override final {
			via_.ay8910.reset(new GI::AY38910::AY38910());
			via_.ay8910->set_clock_rate(1000000);
			video_output_.reset(new VideoOutput(ram_));
			if(!colour_rom_.empty()) video_output_->set_colour_rom(colour_rom_);
		}

		void close_output() override final {
			video_output_.reset();
			via_.ay8910.reset();
		}

		std::shared_ptr<Outputs::CRT::CRT> get_crt() override final {
			return video_output_->get_crt();
		}

		std::shared_ptr<Outputs::Speaker> get_speaker() override final {
			return via_.ay8910;
		}

		void run_for(const Cycles cycles) override final {
			m6502_.run_for(cycles);
		}

		// to satisfy MOS::MOS6522IRQDelegate::Delegate
		void mos6522_did_change_interrupt_status(void *mos6522) override final {
			set_interrupt_line();
		}

		// to satisfy Storage::Tape::BinaryTapePlayer::Delegate
		void tape_did_change_input(Storage::Tape::BinaryTapePlayer *tape_player) override final {
			// set CB1
			via_.set_control_line_input(VIA::Port::B, VIA::Line::One, !tape_player->get_input());
		}

		// for Utility::TypeRecipient::Delegate
		void set_typer_for_string(const char *string) override final {
			std::unique_ptr<CharacterMapper> mapper(new CharacterMapper);
			Utility::TypeRecipient::set_typer_for_string(string, std::move(mapper));
		}

		// for Microdisc::Delegate
		void microdisc_did_change_paging_flags(class Microdisc *microdisc) override final {
			int flags = microdisc->get_paging_flags();
			if(!(flags&Microdisc::PagingFlags::BASICDisable)) {
				ram_top_ = 0xbfff;
				paged_rom_ = rom_;
			} else {
				if(flags&Microdisc::PagingFlags::MicrodscDisable) {
					ram_top_ = 0xffff;
				} else {
					ram_top_ = 0xdfff;
					paged_rom_ = microdisc_rom_.data();
				}
			}
		}

		void wd1770_did_change_output(WD::WD1770 *wd1770) override final {
			set_interrupt_line();
		}

	private:
		CPU::MOS6502::Processor<ConcreteMachine, false> m6502_;

		// RAM and ROM
		std::vector<uint8_t> basic11_rom_, basic10_rom_, microdisc_rom_, colour_rom_;
		uint8_t ram_[65536], rom_[16384];
		Cycles cycles_since_video_update_;
		inline void update_video() {
			video_output_->run_for(cycles_since_video_update_.flush());
		}

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
				TapePlayer() : Storage::Tape::BinaryTapePlayer(1000000) {}

				inline uint8_t get_next_byte(bool fast) {
					return (uint8_t)parser_.get_next_byte(get_tape(), fast);
				}

			private:
				Storage::Tape::Oric::Parser parser_;
		};
		bool use_fast_tape_hack_;

		// VIA (which owns the tape and the AY)
		class VIA: public MOS::MOS6522<VIA>, public MOS::MOS6522IRQDelegate {
			public:
				VIA() :
					MOS::MOS6522<VIA>(),
					tape(new TapePlayer) {}

				using MOS6522IRQDelegate::set_interrupt_status;

				void set_control_line_output(Port port, Line line, bool value) {
					if(line) {
						if(port) ay_bdir_ = value; else ay_bc1_ = value;
						update_ay();
					}
				}

				void set_port_output(Port port, uint8_t value, uint8_t direction_mask)  {
					if(port) {
						keyboard->row = value;
						tape->set_motor_control(value & 0x40);
					} else {
						ay8910->set_data_input(value);
					}
				}

				uint8_t get_port_input(Port port) {
					if(port) {
						uint8_t column = ay8910->get_port_output(false) ^ 0xff;
						return (keyboard->rows[keyboard->row & 7] & column) ? 0x08 : 0x00;
					} else {
						return ay8910->get_data_output();
					}
				}

				inline void run_for(const Cycles cycles) {
					cycles_since_ay_update_ += cycles;
					MOS::MOS6522<VIA>::run_for(cycles);
					tape->run_for(cycles);
				}

				void flush() {
					ay8910->run_for(cycles_since_ay_update_.flush());
					ay8910->flush();
				}

				std::shared_ptr<GI::AY38910::AY38910> ay8910;
				std::unique_ptr<TapePlayer> tape;
				std::shared_ptr<Keyboard> keyboard;

			private:
				void update_ay() {
					ay8910->run_for(cycles_since_ay_update_.flush());
					ay8910->set_control_lines( (GI::AY38910::ControlLines)((ay_bdir_ ? GI::AY38910::BDIR : 0) | (ay_bc1_ ? GI::AY38910::BC1 : 0) | GI::AY38910::BC2));
				}
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

		inline void set_interrupt_line() {
			m6502_.set_irq_line(
				via_.get_interrupt_line() ||
				(microdisc_is_enabled_ && microdisc_.get_interrupt_request_line()));
		}
};

}

using namespace Oric;

Machine *Machine::Oric() {
	return new ConcreteMachine;
}

Machine::~Machine() {}
