//
//  Oric.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/10/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Oric.hpp"

#include "Keyboard.hpp"
#include "Microdisc.hpp"
#include "Video.hpp"

#include "../Utility/MemoryFuzzer.hpp"
#include "../Utility/Typer.hpp"

#include "../../Processors/6502/6502.hpp"
#include "../../Components/6522/6522.hpp"
#include "../../Components/AY38910/AY38910.hpp"

#include "../../Storage/Tape/Tape.hpp"
#include "../../Storage/Tape/Parsers/Oric.hpp"

#include "../../ClockReceiver/ForceInline.hpp"
#include "../../Configurable/StandardOptions.hpp"

#include <memory>

namespace Oric {

std::vector<std::unique_ptr<Configurable::Option>> get_options() {
	return Configurable::standard_options(
		static_cast<Configurable::StandardOptions>(Configurable::DisplayRGBComposite | Configurable::QuickLoadTape)
	);
}

/*!
	Models the Oric's keyboard: eight key rows, containing a bitfield of keys set.

	Active line is selected through a port on the Oric's VIA, and a column mask is
	selected via a port on the AY, returning a single Boolean representation of the
	logical OR of every key selected by the column mask on the active row.
*/
class Keyboard {
	public:
		Keyboard() {
			clear_all_keys();
		}

		/// Sets whether @c key is or is not pressed, per @c is_pressed.
		void set_key_state(uint16_t key, bool is_pressed) {
			uint8_t mask = key & 0xff;
			int line = key >> 8;

			if(is_pressed)	rows_[line] |= mask;
			else			rows_[line] &= ~mask;
		}

		/// Sets all keys as unpressed.
		void clear_all_keys() {
			memset(rows_, 0, sizeof(rows_));
		}

		/// Selects the active row.
		void set_active_row(uint8_t row) {
			row_ = row & 7;
		}

		/// Queries the keys on the active row specified by @c mask.
		bool query_column(uint8_t column_mask) {
			return !!(rows_[row_] & column_mask);
		}

	private:
		uint8_t row_ = 0;
		uint8_t rows_[8];
};

/*!
	Provide's the Oric's tape player: a standard binary-sampled tape which also holds
	an instance of the Oric tape parser, to provide fast-tape loading.
*/
class TapePlayer: public Storage::Tape::BinaryTapePlayer {
	public:
		TapePlayer() : Storage::Tape::BinaryTapePlayer(1000000) {}

		/*!
			Parses the incoming tape event stream to obtain the next stored byte.

			@param use_fast_encoding If set to @c true , inspects the tape as though it
			is encoded in the Oric's fast-loading scheme. Otherwise looks for a slow-encoded byte.

			@returns The next byte from the tape.
		*/
		uint8_t get_next_byte(bool use_fast_encoding) {
			return static_cast<uint8_t>(parser_.get_next_byte(get_tape(), use_fast_encoding));
		}

	private:
		Storage::Tape::Oric::Parser parser_;
};

/*!
	Implements the Oric's VIA's port handler. On the Oric the VIA's ports connect
	to the AY, the tape's motor control signal and the keyboard.
*/
class VIAPortHandler: public MOS::MOS6522::IRQDelegatePortHandler {
	public:
		VIAPortHandler(TapePlayer &tape_player, Keyboard &keyboard) : tape_player_(tape_player), keyboard_(keyboard) {}

		/*!
			Reponds to the 6522's control line output change signal; on an Oric A2 is connected to
			the AY's BDIR, and B2 is connected to the AY's A2.
		*/
		void set_control_line_output(MOS::MOS6522::Port port, MOS::MOS6522::Line line, bool value) {
			if(line) {
				if(port) ay_bdir_ = value; else ay_bc1_ = value;
				update_ay();
				ay8910_->set_control_lines( (GI::AY38910::ControlLines)((ay_bdir_ ? GI::AY38910::BDIR : 0) | (ay_bc1_ ? GI::AY38910::BC1 : 0) | GI::AY38910::BC2));
			}
		}

		/*!
			Reponds to changes in the 6522's port output. On an Oric port B sets the tape motor control
			and the keyboard's active row. Port A is connected to the AY's data bus.
		*/
		void set_port_output(MOS::MOS6522::Port port, uint8_t value, uint8_t direction_mask)  {
			if(port) {
				keyboard_.set_active_row(value);
				tape_player_.set_motor_control(value & 0x40);
			} else {
				update_ay();
				ay8910_->set_data_input(value);
			}
		}

		/*!
			Provides input data for the 6522. Port B reads the keyboard, and port A reads from the AY.
		*/
		uint8_t get_port_input(MOS::MOS6522::Port port) {
			if(port) {
				uint8_t column = ay8910_->get_port_output(false) ^ 0xff;
				return keyboard_.query_column(column) ? 0x08 : 0x00;
			} else {
				return ay8910_->get_data_output();
			}
		}

		/*!
			Advances time. This class manages the AY's concept of time to permit updating-on-demand.
		*/
		inline void run_for(const Cycles cycles) {
			cycles_since_ay_update_ += cycles;
		}

		/// Flushes any queued behaviour (which, specifically, means on the AY).
		void flush() {
			ay8910_->run_for(cycles_since_ay_update_.flush());
			ay8910_->flush();
		}

		/// Sets the AY in use by the machine the VIA that uses this port handler sits within.
		void set_ay(GI::AY38910::AY38910 *ay) {
			ay8910_ = ay;
		}

	private:
		void update_ay() {
			ay8910_->run_for(cycles_since_ay_update_.flush());
		}
		bool ay_bdir_ = false;
		bool ay_bc1_ = false;
		Cycles cycles_since_ay_update_;

		GI::AY38910::AY38910 *ay8910_ = nullptr;
		TapePlayer &tape_player_;
		Keyboard &keyboard_;
};

class ConcreteMachine:
	public CPU::MOS6502::BusHandler,
	public MOS::MOS6522::IRQDelegatePortHandler::Delegate,
	public Utility::TypeRecipient,
	public Storage::Tape::BinaryTapePlayer::Delegate,
	public Microdisc::Delegate,
	public Machine {

	public:
		ConcreteMachine() :
				m6502_(*this),
				via_port_handler_(tape_player_, keyboard_),
				via_(via_port_handler_),
				paged_rom_(rom_) {
			set_clock_rate(1000000);
			via_port_handler_.set_interrupt_delegate(this);
			tape_player_.set_delegate(this);
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

		// Obtains the system ROMs.
		bool set_rom_fetcher(const std::function<std::vector<std::unique_ptr<std::vector<uint8_t>>>(const std::string &machine, const std::vector<std::string> &names)> &roms_with_names) override {
			auto roms = roms_with_names(
				"Oric",
				{
					"basic10.rom",	"basic11.rom",
					"microdisc.rom", "colour.rom"
				});

			for(std::size_t index = 0; index < roms.size(); ++index) {
				auto &data = roms[index];
				if(!data) return false;
				set_rom(static_cast<ROM>(index), *data);
			}

			return true;
		}

		void set_key_state(uint16_t key, bool is_pressed) override final {
			if(key == KeyNMI) {
				m6502_.set_nmi_line(is_pressed);
			} else {
				keyboard_.set_key_state(key, is_pressed);
			}
		}

		void clear_all_keys() override final {
			keyboard_.clear_all_keys();
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
				std::memcpy(rom_, basic11_rom_.data(), std::min(basic11_rom_.size(), sizeof(rom_)));

				is_using_basic11_ = true;
				tape_get_byte_address_ = 0xe6c9;
				scan_keyboard_address_ = 0xf495;
				tape_speed_address_ = 0x024d;
			} else {
				std::memcpy(rom_, basic10_rom_.data(), std::min(basic10_rom_.size(), sizeof(rom_)));

				is_using_basic11_ = false;
				tape_get_byte_address_ = 0xe630;
				scan_keyboard_address_ = 0xf43c;
				tape_speed_address_ = 0x67;
			}

			insert_media(target.media);
		}

		bool insert_media(const StaticAnalyser::Media &media) override final {
			if(media.tapes.size()) {
				tape_player_.set_tape(media.tapes.front());
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
				if(	address == tape_get_byte_address_ &&
					paged_rom_ == rom_ &&
					use_fast_tape_hack_ &&
					operation == CPU::MOS6502::BusOperation::ReadOpcode &&
					tape_player_.has_tape() &&
					!tape_player_.get_tape()->is_at_end()) {

					uint8_t next_byte = tape_player_.get_next_byte(!ram_[tape_speed_address_]);
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
						if(address >= 0x9800 && address <= 0xc000) update_video();
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
			via_port_handler_.run_for(Cycles(1));
			tape_player_.run_for(Cycles(1));
			if(microdisc_is_enabled_) microdisc_.run_for(Cycles(8));
			cycles_since_video_update_++;
			return Cycles(1);
		}

		forceinline void flush() {
			update_video();
			via_port_handler_.flush();
		}

		// to satisfy CRTMachine::Machine
		void setup_output(float aspect_ratio) override final {
			ay8910_.reset(new GI::AY38910::AY38910());
			ay8910_->set_clock_rate(1000000);
			via_port_handler_.set_ay(ay8910_.get());

			video_output_.reset(new VideoOutput(ram_));
			if(!colour_rom_.empty()) video_output_->set_colour_rom(colour_rom_);
			set_output_device(Outputs::CRT::OutputDevice::Monitor);
		}

		void close_output() override final {
			video_output_.reset();
			ay8910_.reset();
			via_port_handler_.set_ay(nullptr);
		}

		std::shared_ptr<Outputs::CRT::CRT> get_crt() override final {
			return video_output_->get_crt();
		}

		std::shared_ptr<Outputs::Speaker> get_speaker() override final {
			return ay8910_;
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
			via_.set_control_line_input(MOS::MOS6522::Port::B, MOS::MOS6522::Line::One, !tape_player->get_input());
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

		KeyboardMapper &get_keyboard_mapper() override {
			return keyboard_mapper_;
		}

		// MARK: - Configuration options.
		std::vector<std::unique_ptr<Configurable::Option>> get_options() override {
			return Oric::get_options();
		}

		void set_selections(const Configurable::SelectionSet &selections_by_option) override {
			bool quickload;
			if(Configurable::get_quick_load_tape(selections_by_option, quickload)) {
				set_use_fast_tape_hack(quickload);
			}

			Configurable::Display display;
			if(Configurable::get_display(selections_by_option, display)) {
				set_output_device((display == Configurable::Display::RGB) ? Outputs::CRT::OutputDevice::Monitor : Outputs::CRT::OutputDevice::Television);
			}
		}

		Configurable::SelectionSet get_accurate_selections() override {
			Configurable::SelectionSet selection_set;
			Configurable::append_quick_load_tape_selection(selection_set, false);
			Configurable::append_display_selection(selection_set, Configurable::Display::Composite);
			return selection_set;
		}

		Configurable::SelectionSet get_user_friendly_selections() override {
			Configurable::SelectionSet selection_set;
			Configurable::append_quick_load_tape_selection(selection_set, true);
			Configurable::append_display_selection(selection_set, Configurable::Display::RGB);
			return selection_set;
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
		bool is_using_basic11_ = false;
		uint16_t tape_get_byte_address_ = 0, scan_keyboard_address_ = 0, tape_speed_address_ = 0;
		int keyboard_read_count_ = 0;

		// Outputs
		std::unique_ptr<VideoOutput> video_output_;
		std::shared_ptr<GI::AY38910::AY38910> ay8910_;

		// Inputs
		Oric::KeyboardMapper keyboard_mapper_;

		// The tape
		TapePlayer tape_player_;
		bool use_fast_tape_hack_ = false;

		VIAPortHandler via_port_handler_;
		MOS::MOS6522::MOS6522<VIAPortHandler> via_;
		Keyboard keyboard_;

		// the Microdisc, if in use
		class Microdisc microdisc_;
		bool microdisc_is_enabled_ = false;
		uint16_t ram_top_ = 0xbfff;
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
