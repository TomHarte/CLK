//
//  Oric.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/10/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "Oric.hpp"

#include "BD500.hpp"
#include "Jasmin.hpp"
#include "Keyboard.hpp"
#include "Microdisc.hpp"
#include "Video.hpp"

#include "../../Activity/Source.hpp"
#include "../MachineTypes.hpp"

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

using DiskInterface = Analyser::Static::Oric::Target::DiskInterface;
using AY = GI::AY38910::AY38910<false>;
using Speaker = Outputs::Speaker::LowpassSpeaker<AY>;

enum ROM {
	BASIC10 = 0, BASIC11, Microdisc, Colour
};

/*!
	Models the Oric's keyboard: eight key rows, containing a bitfield of keys set.

	Active line is selected through a port on the Oric's VIA, and a column mask is
	selected via a port on the AY, returning a single Boolean representation of the
	logical OR of every key selected by the column mask on the active row.
*/
class Keyboard {
	public:
		struct SpecialKeyHandler {
			virtual void perform_special_key(Oric::Key key) = 0;
		};

		Keyboard(SpecialKeyHandler *handler) : special_key_handler_(handler) {
			clear_all_keys();
		}

		/// Sets whether @c key is or is not pressed, per @c is_pressed.
		void set_key_state(uint16_t key, bool is_pressed) {
			switch(key) {
				default: {
					const uint8_t mask = key & 0xff;
					const int line = key >> 8;

					if(is_pressed)	rows_[line] |= mask;
					else			rows_[line] &= ~mask;
				} break;

				case KeyNMI:
				case KeyJasminReset:
					if(is_pressed) {
						special_key_handler_->perform_special_key(Oric::Key(key));
					}
				break;
			}
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
		SpecialKeyHandler *const special_key_handler_;
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
		VIAPortHandler(Concurrency::DeferringAsyncTaskQueue &audio_queue, AY &ay8910, Speaker &speaker, TapePlayer &tape_player, Keyboard &keyboard) :
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
			speaker_.run_for(audio_queue_, cycles_since_ay_update_.flush<Cycles>());
		}
		bool ay_bdir_ = false;
		bool ay_bc1_ = false;
		HalfCycles cycles_since_ay_update_;

		Concurrency::DeferringAsyncTaskQueue &audio_queue_;
		AY &ay8910_;
		Speaker &speaker_;
		TapePlayer &tape_player_;
		Keyboard &keyboard_;
};

template <Analyser::Static::Oric::Target::DiskInterface disk_interface> class ConcreteMachine:
	public MachineTypes::TimedMachine,
	public MachineTypes::ScanProducer,
	public MachineTypes::AudioProducer,
	public MachineTypes::MediaTarget,
	public MachineTypes::MappedKeyboardMachine,
	public Configurable::Device,
	public CPU::MOS6502::BusHandler,
	public MOS::MOS6522::IRQDelegatePortHandler::Delegate,
	public Storage::Tape::BinaryTapePlayer::Delegate,
	public DiskController::Delegate,
	public ClockingHint::Observer,
	public Activity::Source,
	public Machine,
	public Keyboard::SpecialKeyHandler {

	public:
		ConcreteMachine(const Analyser::Static::Oric::Target &target, const ROMMachine::ROMFetcher &rom_fetcher) :
				m6502_(*this),
				video_output_(ram_),
				ay8910_(GI::AY38910::Personality::AY38910, audio_queue_),
				speaker_(ay8910_),
				via_port_handler_(audio_queue_, ay8910_, speaker_, tape_player_, keyboard_),
				via_(via_port_handler_),
				keyboard_(this),
				diskii_(2000000) {
			set_clock_rate(1000000);
			speaker_.set_input_rate(1000000.0f);
			via_port_handler_.set_interrupt_delegate(this);
			tape_player_.set_delegate(this);

			// Slight hack here: I'm unclear what RAM should look like at startup.
			// Actually, I think completely random might be right since the Microdisc
			// sort of assumes it, but also the BD-500 never explicitly sets PAL mode
			// so I can't have any switch-to-NTSC bytes in the display area. Hence:
			// disallow all atributes.
			Memory::Fuzz(ram_, sizeof(ram_));
			for(size_t c = 0; c < sizeof(ram_); ++c) {
				ram_[c] |= 0x40;
			}

			if constexpr (disk_interface == DiskInterface::Pravetz) {
				diskii_.set_clocking_hint_observer(this);
			}

			const std::string machine_name = "Oric";
			std::vector<ROMMachine::ROM> rom_names = { {machine_name, "the Oric colour ROM", "colour.rom", 128, 0xd50fca65} };
			switch(target.rom) {
				case Analyser::Static::Oric::Target::ROM::BASIC10:
					rom_names.emplace_back(machine_name, "Oric BASIC 1.0", "basic10.rom", 16*1024, 0xf18710b4);
				break;
				case Analyser::Static::Oric::Target::ROM::BASIC11:
					rom_names.emplace_back(machine_name, "Oric BASIC 1.1", "basic11.rom", 16*1024, 0xc3a92bef);
				break;
				case Analyser::Static::Oric::Target::ROM::Pravetz:
					rom_names.emplace_back(machine_name, "Pravetz BASIC", "pravetz.rom", 16*1024, 0x58079502);
				break;
			}
			size_t diskii_state_machine_index = 0;
			switch(disk_interface) {
				default: break;
				case DiskInterface::BD500:
					rom_names.emplace_back(machine_name, "the ORIC Byte Drive 500 ROM", "bd500.rom", 8*1024, 0x61952e34);
				break;
				case DiskInterface::Jasmin:
					rom_names.emplace_back(machine_name, "the ORIC Jasmin ROM", "jasmin.rom", 2*1024, 0x37220e89);
				break;
				case DiskInterface::Microdisc:
					rom_names.emplace_back(machine_name, "the ORIC Microdisc ROM", "microdisc.rom", 8*1024, 0xa9664a9c);
				break;
				case DiskInterface::Pravetz:
					rom_names.emplace_back(machine_name, "the 8DOS boot ROM", "8dos.rom", 512, 0x49a74c06);
					// These ROM details are coupled with those in the DiskIICard.
					diskii_state_machine_index = rom_names.size();
					rom_names.push_back({"DiskII", "the Disk II 16-sector state machine ROM", "state-machine-16.rom", 256, { 0x9796a238, 0xb72a2c70 }});
				break;
			}

			const auto roms = rom_fetcher(rom_names);

			for(std::size_t index = 0; index < roms.size(); ++index) {
				if(!roms[index]) {
					throw ROMMachine::Error::MissingROMs;
				}
			}

			video_output_.set_colour_rom(*roms[0]);
			rom_ = std::move(*roms[1]);

			switch(disk_interface) {
				default: break;
				case DiskInterface::BD500:
					disk_rom_ = std::move(*roms[2]);
					disk_rom_.resize(8192);
				break;
				case DiskInterface::Jasmin:
					disk_rom_ = std::move(*roms[2]);
					disk_rom_.resize(2048);
				break;
				case DiskInterface::Microdisc:
					disk_rom_ = std::move(*roms[2]);
					disk_rom_.resize(8192);
				break;
				case DiskInterface::Pravetz: {
					pravetz_rom_ = std::move(*roms[2]);
					pravetz_rom_.resize(512);

					diskii_.set_state_machine(*roms[diskii_state_machine_index]);
				} break;
			}

			rom_.resize(16384);
			paged_rom_ = rom_.data();

			switch(target.disk_interface) {
				default: break;
				case DiskInterface::BD500:
					bd500_.set_delegate(this);
				break;
				case DiskInterface::Jasmin:
					jasmin_.set_delegate(this);
				break;
				case DiskInterface::Microdisc:
					microdisc_.set_delegate(this);
				break;
			}

			if(!target.loading_command.empty()) {
				type_string(target.loading_command);
			}

			if(target.should_start_jasmin) {
				// If Jasmin autostart is requested then plan to do so in 3 seconds; empirically long enough
				// for the Oric to boot normally, before the Jasmin intercedes.
				jasmin_reset_counter_ = 3000000;
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

		void set_key_state(uint16_t key, bool is_pressed) final {
			if(key == KeyNMI) {
				m6502_.set_nmi_line(is_pressed);
			} else {
				keyboard_.set_key_state(key, is_pressed);
			}
		}

		void clear_all_keys() final {
			keyboard_.clear_all_keys();
		}

		void set_use_fast_tape_hack(bool activate) {
			use_fast_tape_hack_ = activate;
		}

		template <typename DiskInterface> bool insert_disks(const Analyser::Static::Media &media, DiskInterface &interface, int num_drives) {
			int drive_index = 0;
			for(auto &disk : media.disks) {
				interface.set_disk(disk, drive_index);
				++drive_index;
				if(drive_index == num_drives) break;
			}

			return true;
		}

		bool insert_media(const Analyser::Static::Media &media) final {
			bool inserted = false;

			if(!media.tapes.empty()) {
				tape_player_.set_tape(media.tapes.front());
				inserted = true;
			}

			if(!media.disks.empty()) {
				switch(disk_interface) {
					case DiskInterface::BD500:		inserted |= insert_disks(media, bd500_, 4);		break;
					case DiskInterface::Jasmin:		inserted |= insert_disks(media, jasmin_, 4);	break;
					case DiskInterface::Microdisc:	inserted |= insert_disks(media, microdisc_, 4);	break;
					case DiskInterface::Pravetz:	inserted |= insert_disks(media, diskii_, 2);	break;
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
					if(address < 0x0310 || (disk_interface == DiskInterface::None)) {
						if(isReadOperation(operation)) *value = via_.read(address);
						else via_.write(address, *value);
					} else {
						switch(disk_interface) {
							default: break;
							case DiskInterface::BD500:
								if(isReadOperation(operation)) *value = bd500_.read(address);
								else bd500_.write(address, *value);
							break;
							case DiskInterface::Jasmin:
								if(address >= 0x3f4) {
									if(isReadOperation(operation)) *value = jasmin_.read(address);
									else jasmin_.write(address, *value);
								}
							break;
							case DiskInterface::Microdisc:
								switch(address) {
									case 0x0310: case 0x0311: case 0x0312: case 0x0313:
										if(isReadOperation(operation)) *value = microdisc_.read(address);
										else microdisc_.write(address, *value);
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
							case DiskInterface::Pravetz:
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
				case DiskInterface::BD500:
					bd500_.run_for(Cycles(9));		// i.e. effective clock rate of 9Mhz.
				break;
				case DiskInterface::Jasmin:
					jasmin_.run_for(Cycles(8));;	// i.e. effective clock rate of 8Mhz.

					// Jasmin autostart hack: wait for a period, then trigger a reset, having forced
					// the Jasmin to page its ROM in first. I assume the latter being what the Jasmin's
					// hardware boot button did.
					if(jasmin_reset_counter_) {
						--jasmin_reset_counter_;
						if(!jasmin_reset_counter_) {
							perform_special_key(KeyJasminReset);
						}
					}
				break;
				case DiskInterface::Microdisc:
					microdisc_.run_for(Cycles(8));;	// i.e. effective clock rate of 8Mhz.
				break;
				case DiskInterface::Pravetz:
					if(diskii_clocking_preference_ == ClockingHint::Preference::RealTime) {
						diskii_.set_data_input(*value);
						diskii_.run_for(Cycles(2));;	// i.e. effective clock rate of 2Mhz.
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
		void set_scan_target(Outputs::Display::ScanTarget *scan_target) final {
			video_output_.set_scan_target(scan_target);
		}

		Outputs::Display::ScanStatus get_scaled_scan_status() const final {
			return video_output_.get_scaled_scan_status();
		}

		void set_display_type(Outputs::Display::DisplayType display_type) final {
			video_output_.set_display_type(display_type);
		}

		Outputs::Display::DisplayType get_display_type() final {
			return video_output_.get_display_type();
		}

		Outputs::Speaker::Speaker *get_speaker() final {
			return &speaker_;
		}

		void run_for(const Cycles cycles) final {
			m6502_.run_for(cycles);
		}

		// to satisfy MOS::MOS6522IRQDelegate::Delegate
		void mos6522_did_change_interrupt_status(void *mos6522) final {
			set_interrupt_line();
		}

		// to satisfy Storage::Tape::BinaryTapePlayer::Delegate
		void tape_did_change_input(Storage::Tape::BinaryTapePlayer *tape_player) final {
			// set CB1
			via_.set_control_line_input(MOS::MOS6522::Port::B, MOS::MOS6522::Line::One, !tape_player->get_input());
		}

		// for Utility::TypeRecipient::Delegate
		void type_string(const std::string &string) final {
			string_serialiser_ = std::make_unique<Utility::StringSerialiser>(string, true);
		}

		bool can_type(char c) final {
			// Make an effort to type the entire printable ASCII range.
			return c >= 32 && c < 127;
		}

		// DiskController::Delegate
		void disk_controller_did_change_paged_item(DiskController *controller) final {
			switch(controller->get_paged_item()) {
				default:
					ram_top_ = basic_visible_ram_top_;
					paged_rom_ = rom_.data();
				break;

				case DiskController::PagedItem::RAM:
					ram_top_ = basic_invisible_ram_top_;
				break;

				case DiskController::PagedItem::DiskROM:
					ram_top_ = uint16_t(0xffff - disk_rom_.size());
					paged_rom_ = disk_rom_.data();
				break;
			}
		}

		// WD::WD1770::Delegate
		void wd1770_did_change_output(WD::WD1770 *wd1770) final {
			set_interrupt_line();
		}

		KeyboardMapper *get_keyboard_mapper() final {
			return &keyboard_mapper_;
		}

		// MARK: - Configuration options.
		std::unique_ptr<Reflection::Struct> get_options() final {
			auto options = std::make_unique<Options>(Configurable::OptionsType::UserFriendly);
			options->output = get_video_signal_configurable();
			options->quickload = use_fast_tape_hack_;
			return options;
		}

		void set_options(const std::unique_ptr<Reflection::Struct> &str) final {
			const auto options = dynamic_cast<Options *>(str.get());
			set_video_signal_configurable(options->output);
			set_use_fast_tape_hack(options->quickload);
		}

		void set_activity_observer(Activity::Observer *observer) final {
			switch(disk_interface) {
				default: break;
				case DiskInterface::BD500:
					bd500_.set_activity_observer(observer);
				break;
				case DiskInterface::Jasmin:
					jasmin_.set_activity_observer(observer);
				break;
				case DiskInterface::Microdisc:
					microdisc_.set_activity_observer(observer);
				break;
				case DiskInterface::Pravetz:
					diskii_.set_activity_observer(observer);
				break;
			}
		}

		void set_component_prefers_clocking(ClockingHint::Source *component, ClockingHint::Preference preference) final {
			diskii_clocking_preference_ = diskii_.preferred_clocking();
		}

	private:
		const uint16_t basic_invisible_ram_top_ = 0xffff;
		const uint16_t basic_visible_ram_top_ = 0xbfff;

		CPU::MOS6502::Processor<CPU::MOS6502::Personality::P6502, ConcreteMachine, false> m6502_;

		// RAM and ROM
		std::vector<uint8_t> rom_, disk_rom_;
		uint8_t ram_[65536];
		Cycles cycles_since_video_update_;
		inline void update_video() {
			video_output_.run_for(cycles_since_video_update_.flush<Cycles>());
		}

		// ROM bookkeeping
		uint16_t tape_get_byte_address_ = 0, tape_speed_address_ = 0;
		int keyboard_read_count_ = 0;

		// Outputs
		VideoOutput video_output_;

		Concurrency::DeferringAsyncTaskQueue audio_queue_;
		GI::AY38910::AY38910<false> ay8910_;
		Speaker speaker_;

		// Inputs
		Oric::KeyboardMapper keyboard_mapper_;

		// The tape
		TapePlayer tape_player_;
		bool use_fast_tape_hack_ = false;

		VIAPortHandler via_port_handler_;
		MOS::MOS6522::MOS6522<VIAPortHandler> via_;
		Keyboard keyboard_;

		// the Microdisc, if in use.
		class Microdisc microdisc_;

		// the Jasmin, if in use.
		Jasmin jasmin_;
		int jasmin_reset_counter_ = 0;

		// the BD-500, if in use.
		BD500 bd500_;

		// the Pravetz/Disk II, if in use.
		Apple::DiskII diskii_;
		Cycles cycles_since_diskii_update_;
		void flush_diskii() {
			diskii_.run_for(cycles_since_diskii_update_.flush<Cycles>());
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

			// The Microdisc directly provides an interrupt line.
			if constexpr (disk_interface == DiskInterface::Microdisc) {
				irq_line |= microdisc_.get_interrupt_request_line();
			}

			// The Jasmin reroutes its data request line to the processor's interrupt line.
			if constexpr (disk_interface == DiskInterface::Jasmin) {
				irq_line |= jasmin_.get_data_request_line();
			}

			m6502_.set_irq_line(irq_line);
		}

		// Keys that aren't read by polling.
		void perform_special_key(Oric::Key key) final {
			switch(key) {
				default: break;

				case KeyJasminReset:
					jasmin_.write(0x3fa, 0);
					jasmin_.write(0x3fb, 1);
					m6502_.set_power_on(true);
				break;

				case KeyNMI:
					// As luck would have it, the 6502's NMI line is edge triggered.
					// So just forcing through an edge will work here.
					m6502_.set_nmi_line(true);
					m6502_.set_nmi_line(false);
				break;
			}
		}

		// MARK - typing
		std::unique_ptr<Utility::StringSerialiser> string_serialiser_;
};

}

using namespace Oric;

Machine *Machine::Oric(const Analyser::Static::Target *target_hint, const ROMMachine::ROMFetcher &rom_fetcher) {
	auto *const oric_target = dynamic_cast<const Analyser::Static::Oric::Target *>(target_hint);
	switch(oric_target->disk_interface) {
		default:						return new ConcreteMachine<DiskInterface::None>(*oric_target, rom_fetcher);
		case DiskInterface::Microdisc:	return new ConcreteMachine<DiskInterface::Microdisc>(*oric_target, rom_fetcher);
		case DiskInterface::Pravetz:	return new ConcreteMachine<DiskInterface::Pravetz>(*oric_target, rom_fetcher);
		case DiskInterface::Jasmin:		return new ConcreteMachine<DiskInterface::Jasmin>(*oric_target, rom_fetcher);
		case DiskInterface::BD500:		return new ConcreteMachine<DiskInterface::BD500>(*oric_target, rom_fetcher);
	}
}

Machine::~Machine() {}
