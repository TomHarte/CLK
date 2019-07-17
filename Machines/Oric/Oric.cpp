//
//  Oric.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/10/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "Oric.hpp"

#include "Keyboard.hpp"
#include "Microdisc.hpp"
#include "Video.hpp"

#include "../../Activity/Source.hpp"
#include "../MediaTarget.hpp"
#include "../CRTMachine.hpp"
#include "../KeyboardMachine.hpp"

#include "../Utility/MemoryFuzzer.hpp"
#include "../Utility/StringSerialiser.hpp"

#include "../../Processors/6502/6502.hpp"
#include "../../Components/6522/6522.hpp"
#include "../../Components/AY38910/AY38910.hpp"
#include "../../Components/DiskII/DiskII.hpp"

#include "../../Storage/Tape/Tape.hpp"
#include "../../Storage/Tape/Parsers/Oric.hpp"

#include "../../ClockReceiver/ForceInline.hpp"
#include "../../Configurable/StandardOptions.hpp"
#include "../../Outputs/Speaker/Implementation/LowpassSpeaker.hpp"

#include "../../Analyser/Static/Oric/Target.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace Oric {

enum ROM {
	BASIC10 = 0, BASIC11, Microdisc, Colour
};

std::vector<std::unique_ptr<Configurable::Option>> get_options() {
	return Configurable::standard_options(
		static_cast<Configurable::StandardOptions>(
			Configurable::DisplayRGB |
			Configurable::DisplayCompositeColour |
			Configurable::DisplayCompositeMonochrome |
			Configurable::QuickLoadTape)
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
		VIAPortHandler(Concurrency::DeferringAsyncTaskQueue &audio_queue, GI::AY38910::AY38910 &ay8910, Outputs::Speaker::LowpassSpeaker<GI::AY38910::AY38910> &speaker, TapePlayer &tape_player, Keyboard &keyboard) :
			audio_queue_(audio_queue), ay8910_(ay8910), speaker_(speaker), tape_player_(tape_player), keyboard_(keyboard) {}

		/*!
			Reponds to the 6522's control line output change signal; on an Oric A2 is connected to
			the AY's BDIR, and B2 is connected to the AY's A2.
		*/
		void set_control_line_output(MOS::MOS6522::Port port, MOS::MOS6522::Line line, bool value) {
			if(line) {
				if(port) ay_bdir_ = value; else ay_bc1_ = value;
				update_ay();
				ay8910_.set_control_lines( (GI::AY38910::ControlLines)((ay_bdir_ ? GI::AY38910::BDIR : 0) | (ay_bc1_ ? GI::AY38910::BC1 : 0) | GI::AY38910::BC2));
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
				ay8910_.set_data_input(value);
			}
		}

		/*!
			Provides input data for the 6522. Port B reads the keyboard, and port A reads from the AY.
		*/
		uint8_t get_port_input(MOS::MOS6522::Port port) {
			if(port) {
				uint8_t column = ay8910_.get_port_output(false) ^ 0xff;
				return keyboard_.query_column(column) ? 0x08 : 0x00;
			} else {
				return ay8910_.get_data_output();
			}
		}

		/*!
			Advances time. This class manages the AY's concept of time to permit updating-on-demand.
		*/
		inline void run_for(const HalfCycles cycles) {
			cycles_since_ay_update_ += cycles;
		}

		/// Flushes any queued behaviour (which, specifically, means on the AY).
		void flush() {
			update_ay();
			audio_queue_.perform();
		}

	private:
		void update_ay() {
			speaker_.run_for(audio_queue_, cycles_since_ay_update_.flush_cycles());
		}
		bool ay_bdir_ = false;
		bool ay_bc1_ = false;
		HalfCycles cycles_since_ay_update_;

		Concurrency::DeferringAsyncTaskQueue &audio_queue_;
		GI::AY38910::AY38910 &ay8910_;
		Outputs::Speaker::LowpassSpeaker<GI::AY38910::AY38910> &speaker_;
		TapePlayer &tape_player_;
		Keyboard &keyboard_;
};

template <Analyser::Static::Oric::Target::DiskInterface disk_interface> class ConcreteMachine:
	public CRTMachine::Machine,
	public MediaTarget::Machine,
	public KeyboardMachine::MappedMachine,
	public Configurable::Device,
	public CPU::MOS6502::BusHandler,
	public MOS::MOS6522::IRQDelegatePortHandler::Delegate,
	public Utility::TypeRecipient,
	public Storage::Tape::BinaryTapePlayer::Delegate,
	public Microdisc::Delegate,
	public ClockingHint::Observer,
	public Activity::Source,
	public Machine {

	public:
		ConcreteMachine(const Analyser::Static::Oric::Target &target, const ROMMachine::ROMFetcher &rom_fetcher) :
				m6502_(*this),
				video_output_(ram_),
				ay8910_(audio_queue_),
				speaker_(ay8910_),
				via_port_handler_(audio_queue_, ay8910_, speaker_, tape_player_, keyboard_),
				via_(via_port_handler_),
				diskii_(2000000) {
			set_clock_rate(1000000);
			speaker_.set_input_rate(1000000.0f);
			via_port_handler_.set_interrupt_delegate(this);
			tape_player_.set_delegate(this);
			Memory::Fuzz(ram_, sizeof(ram_));

			if(disk_interface == Analyser::Static::Oric::Target::DiskInterface::Pravetz) {
				diskii_.set_clocking_hint_observer(this);
			}

			std::vector<std::string> rom_names = {"colour.rom"};
			switch(target.rom) {
				case Analyser::Static::Oric::Target::ROM::BASIC10: rom_names.push_back("basic10.rom");	break;
				case Analyser::Static::Oric::Target::ROM::BASIC11: rom_names.push_back("basic11.rom");	break;
				case Analyser::Static::Oric::Target::ROM::Pravetz: rom_names.push_back("pravetz.rom");	break;
			}
			switch(disk_interface) {
				default: break;
				case Analyser::Static::Oric::Target::DiskInterface::Microdisc:	rom_names.push_back("microdisc.rom");	break;
				case Analyser::Static::Oric::Target::DiskInterface::Pravetz:	rom_names.push_back("8dos.rom");		break;
			}

			const auto roms = rom_fetcher("Oric", rom_names);

			for(std::size_t index = 0; index < roms.size(); ++index) {
				if(!roms[index]) {
					throw ROMMachine::Error::MissingROMs;
				}
			}

			video_output_.set_colour_rom(*roms[0]);
			rom_ = std::move(*roms[1]);

			switch(disk_interface) {
				default: break;
				case Analyser::Static::Oric::Target::DiskInterface::Microdisc:
					microdisc_rom_ = std::move(*roms[2]);
					microdisc_rom_.resize(8192);
				break;
				case Analyser::Static::Oric::Target::DiskInterface::Pravetz: {
					pravetz_rom_ = std::move(*roms[2]);
					pravetz_rom_.resize(512);

					auto state_machine_rom = rom_fetcher("DiskII", {"state-machine-16.rom"});
					if(!state_machine_rom[0]) {
						throw ROMMachine::Error::MissingROMs;
					}
					diskii_.set_state_machine(*state_machine_rom[0]);
				} break;
			}

			rom_.resize(16384);
			paged_rom_ = rom_.data();

			switch(target.disk_interface) {
				default: break;
				case Analyser::Static::Oric::Target::DiskInterface::Microdisc:
					microdisc_did_change_paging_flags(&microdisc_);
					microdisc_.set_delegate(this);
				break;
			}

			if(!target.loading_command.empty()) {
				type_string(target.loading_command);
			}

			switch(target.rom) {
				case Analyser::Static::Oric::Target::ROM::BASIC10:
					tape_get_byte_address_ = 0xe630;
					tape_speed_address_ = 0x67;
				break;
				case Analyser::Static::Oric::Target::ROM::BASIC11:
				case Analyser::Static::Oric::Target::ROM::Pravetz:
					tape_get_byte_address_ = 0xe6c9;
					tape_speed_address_ = 0x024d;
				break;
			}

			insert_media(target.media);
		}

		~ConcreteMachine() {
			audio_queue_.flush();
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

		void set_use_fast_tape_hack(bool activate) {
			use_fast_tape_hack_ = activate;
		}

		bool insert_media(const Analyser::Static::Media &media) override final {
			bool inserted = false;

			if(!media.tapes.empty()) {
				tape_player_.set_tape(media.tapes.front());
				inserted = true;
			}

			if(!media.disks.empty()) {
				switch(disk_interface) {
					case Analyser::Static::Oric::Target::DiskInterface::Microdisc: {
						inserted = true;
						size_t drive_index = 0;
						for(auto &disk : media.disks) {
							if(drive_index < 4) microdisc_.set_disk(disk, drive_index);
							++drive_index;
						}
					} break;
					case Analyser::Static::Oric::Target::DiskInterface::Pravetz: {
						inserted = true;
						int drive_index = 0;
						for(auto &disk : media.disks) {
							if(drive_index < 2) diskii_.set_disk(disk, drive_index);
							++drive_index;
						}
					} break;

					default: break;
				}
			}

			return inserted;
		}

		// to satisfy CPU::MOS6502::BusHandler
		forceinline Cycles perform_bus_operation(CPU::MOS6502::BusOperation operation, uint16_t address, uint8_t *value) {
			if(address > ram_top_) {
				if(isReadOperation(operation)) *value = paged_rom_[address - ram_top_ - 1];

				// 024D = 0 => fast; otherwise slow
				// E6C9 = read byte: return byte in A
				if(	address == tape_get_byte_address_ &&
					paged_rom_ == rom_.data() &&
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
					if(address < 0x0310 || (disk_interface == Analyser::Static::Oric::Target::DiskInterface::None)) {
						if(isReadOperation(operation)) *value = via_.get_register(address);
						else via_.set_register(address, *value);
					} else {
						switch(disk_interface) {
							default: break;
							case Analyser::Static::Oric::Target::DiskInterface::Microdisc:
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
							break;
							case Analyser::Static::Oric::Target::DiskInterface::Pravetz:
								if(address >= 0x0320) {
									if(isReadOperation(operation)) *value = pravetz_rom_[pravetz_rom_base_pointer_ + (address & 0xff)];
									else {
										switch(address) {
											case 0x380:	case 0x381:	case 0x382:	case 0x383:
												ram_top_ = (address&1) ? basic_invisible_ram_top_ : basic_visible_ram_top_;
												pravetz_rom_base_pointer_ = (address&2) ? 0x100 : 0x000;
											break;
										}
									}
								} else {
									flush_diskii();
									const int disk_value = diskii_.read_address(address);
									if(isReadOperation(operation) && disk_value != diskii_.DidNotLoad) *value = static_cast<uint8_t>(disk_value);
								}
							break;
						}
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

			// $02df is where the Oric ROMs; all of them, including BASIC 1.0, 1.1 and the Pravetz; have the
			// IRQ routine store an incoming keystroke in order for reading to occur later. By capturing the
			// read rather than the decode and write: (i) nothing is lost while BASIC is parsing; and
			// (ii) keyboard input is much more rapid.
			if(string_serialiser_ && address == 0x02df && operation == CPU::MOS6502::BusOperation::Read) {
				*value = string_serialiser_->head() | 0x80;
				if(!string_serialiser_->advance()) string_serialiser_.reset();
			}

			via_.run_for(Cycles(1));
			tape_player_.run_for(Cycles(1));
			switch(disk_interface) {
				default: break;
				case Analyser::Static::Oric::Target::DiskInterface::Microdisc:
					microdisc_.run_for(Cycles(8));
				break;
				case Analyser::Static::Oric::Target::DiskInterface::Pravetz:
					if(diskii_clocking_preference_ == ClockingHint::Preference::RealTime) {
						diskii_.set_data_input(*value);
						diskii_.run_for(Cycles(2));
					} else {
						cycles_since_diskii_update_ += Cycles(2);
					}
				break;
			}
			cycles_since_video_update_++;
			return Cycles(1);
		}

		forceinline void flush() {
			update_video();
			via_.flush();
			flush_diskii();
		}

		// to satisfy CRTMachine::Machine
		void set_scan_target(Outputs::Display::ScanTarget *scan_target) override final {
			video_output_.set_scan_target(scan_target);
		}

		void set_display_type(Outputs::Display::DisplayType display_type) override {
			video_output_.set_display_type(display_type);
		}

		Outputs::Speaker::Speaker *get_speaker() override final {
			return &speaker_;
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
		void type_string(const std::string &string) override final {
			string_serialiser_.reset(new Utility::StringSerialiser(string, true));
		}

		// for Microdisc::Delegate
		void microdisc_did_change_paging_flags(class Microdisc *microdisc) override final {
			int flags = microdisc->get_paging_flags();
			if(!(flags&Microdisc::PagingFlags::BASICDisable)) {
				ram_top_ = basic_visible_ram_top_;
				paged_rom_ = rom_.data();
			} else {
				if(flags&Microdisc::PagingFlags::MicrodiscDisable) {
					ram_top_ = basic_invisible_ram_top_;
				} else {
					ram_top_ = 0xdfff;
					paged_rom_ = microdisc_rom_.data();
				}
			}
		}

		void wd1770_did_change_output(WD::WD1770 *wd1770) override final {
			set_interrupt_line();
		}

		KeyboardMapper *get_keyboard_mapper() override {
			return &keyboard_mapper_;
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
				set_video_signal_configurable(display);
			}
		}

		Configurable::SelectionSet get_accurate_selections() override {
			Configurable::SelectionSet selection_set;
			Configurable::append_quick_load_tape_selection(selection_set, false);
			Configurable::append_display_selection(selection_set, Configurable::Display::CompositeColour);
			return selection_set;
		}

		Configurable::SelectionSet get_user_friendly_selections() override {
			Configurable::SelectionSet selection_set;
			Configurable::append_quick_load_tape_selection(selection_set, true);
			Configurable::append_display_selection(selection_set, Configurable::Display::RGB);
			return selection_set;
		}

		void set_activity_observer(Activity::Observer *observer) override {
			switch(disk_interface) {
				default: break;
				case Analyser::Static::Oric::Target::DiskInterface::Microdisc:
					microdisc_.set_activity_observer(observer);
				break;
				case Analyser::Static::Oric::Target::DiskInterface::Pravetz:
					diskii_.set_activity_observer(observer);
				break;
			}
		}

		void set_component_prefers_clocking(ClockingHint::Source *component, ClockingHint::Preference preference) override final {
			diskii_clocking_preference_ = diskii_.preferred_clocking();
		}

	private:
		const uint16_t basic_invisible_ram_top_ = 0xffff;
		const uint16_t basic_visible_ram_top_ = 0xbfff;

		CPU::MOS6502::Processor<CPU::MOS6502::Personality::P6502, ConcreteMachine, false> m6502_;

		// RAM and ROM
		std::vector<uint8_t> rom_, microdisc_rom_;
		uint8_t ram_[65536];
		Cycles cycles_since_video_update_;
		inline void update_video() {
			video_output_.run_for(cycles_since_video_update_.flush());
		}

		// ROM bookkeeping
		uint16_t tape_get_byte_address_ = 0, tape_speed_address_ = 0;
		int keyboard_read_count_ = 0;

		// Outputs
		VideoOutput video_output_;

		Concurrency::DeferringAsyncTaskQueue audio_queue_;
		GI::AY38910::AY38910 ay8910_;
		Outputs::Speaker::LowpassSpeaker<GI::AY38910::AY38910> speaker_;

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

		// the Pravetz/Disk II, if in use
		Apple::DiskII diskii_;
		Cycles cycles_since_diskii_update_;
		void flush_diskii() {
			diskii_.run_for(cycles_since_diskii_update_.flush());
		}
		std::vector<uint8_t> pravetz_rom_;
		std::size_t pravetz_rom_base_pointer_ = 0;
		ClockingHint::Preference diskii_clocking_preference_ = ClockingHint::Preference::RealTime;

		// Overlay RAM
		uint16_t ram_top_ = basic_visible_ram_top_;
		uint8_t *paged_rom_ = nullptr;

		// Helper to discern current IRQ state
		inline void set_interrupt_line() {
			bool irq_line = via_.get_interrupt_line();
			if(disk_interface == Analyser::Static::Oric::Target::DiskInterface::Microdisc)
				irq_line |= microdisc_.get_interrupt_request_line();
			m6502_.set_irq_line(irq_line);
		}

		// MARK - typing
		std::unique_ptr<Utility::StringSerialiser> string_serialiser_;
};

}

using namespace Oric;

Machine *Machine::Oric(const Analyser::Static::Target *target_hint, const ROMMachine::ROMFetcher &rom_fetcher) {
	auto *const oric_target = dynamic_cast<const Analyser::Static::Oric::Target *>(target_hint);
	using DiskInterface = Analyser::Static::Oric::Target::DiskInterface;
	switch(oric_target->disk_interface) {
		default:						return new ConcreteMachine<DiskInterface::None>(*oric_target, rom_fetcher);
		case DiskInterface::Microdisc:	return new ConcreteMachine<DiskInterface::Microdisc>(*oric_target, rom_fetcher);
		case DiskInterface::Pravetz:	return new ConcreteMachine<DiskInterface::Pravetz>(*oric_target, rom_fetcher);
	}
}

Machine::~Machine() {}
