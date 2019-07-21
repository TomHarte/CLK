//
//  Electron.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/01/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "Electron.hpp"

#include "../../Activity/Source.hpp"
#include "../MediaTarget.hpp"
#include "../CRTMachine.hpp"
#include "../KeyboardMachine.hpp"

#include "../../ClockReceiver/ClockReceiver.hpp"
#include "../../ClockReceiver/ForceInline.hpp"
#include "../../Configurable/StandardOptions.hpp"
#include "../../Outputs/Speaker/Implementation/LowpassSpeaker.hpp"
#include "../../Processors/6502/6502.hpp"
#include "../../Storage/Tape/Tape.hpp"

#include "../Utility/Typer.hpp"
#include "../../Analyser/Static/Acorn/Target.hpp"

#include "Interrupts.hpp"
#include "Keyboard.hpp"
#include "Plus3.hpp"
#include "SoundGenerator.hpp"
#include "Tape.hpp"
#include "Video.hpp"

namespace Electron {

std::vector<std::unique_ptr<Configurable::Option>> get_options() {
	return Configurable::standard_options(
		static_cast<Configurable::StandardOptions>(Configurable::DisplayRGB | Configurable::DisplayCompositeColour | Configurable::QuickLoadTape)
	);
}

class ConcreteMachine:
	public Machine,
	public CRTMachine::Machine,
	public MediaTarget::Machine,
	public KeyboardMachine::MappedMachine,
	public Configurable::Device,
	public CPU::MOS6502::BusHandler,
	public Tape::Delegate,
	public Utility::TypeRecipient,
	public Activity::Source {
	public:
		ConcreteMachine(const Analyser::Static::Acorn::Target &target, const ROMMachine::ROMFetcher &rom_fetcher) :
				m6502_(*this),
				video_output_(ram_),
				sound_generator_(audio_queue_),
				speaker_(sound_generator_) {
			memset(key_states_, 0, sizeof(key_states_));
			for(int c = 0; c < 16; c++)
				memset(roms_[c], 0xff, 16384);

			tape_.set_delegate(this);
			set_clock_rate(2000000);

			speaker_.set_input_rate(2000000 / SoundGenerator::clock_rate_divider);
			speaker_.set_high_frequency_cutoff(7000);

			std::vector<ROMMachine::ROM> required_roms = {
				{"the Acorn BASIC II ROM", "basic.rom", 16*1024, 0x79434781},
				{"the Electron MOS ROM", "os.rom", 16*1024, 0xbf63fb1f}
			};
			if(target.has_adfs) {
				required_roms.emplace_back("the E00 ADFS ROM, first slot", "ADFS-E00_1.rom", 16*1024, 0x51523993);
				required_roms.emplace_back("the E00 ADFS ROM, second slot", "ADFS-E00_2.rom", 16*1024, 0x8d17de0e);
			}
			const size_t dfs_rom_position = required_roms.size();
			if(target.has_dfs) {
				required_roms.emplace_back("the 1770 DFS ROM", "DFS-1770-2.20.rom", 16*1024, 0xf3dc9bc5);
			}
			const auto roms = rom_fetcher("Electron", required_roms);

			for(const auto &rom: roms) {
				if(!rom) {
					throw ROMMachine::Error::MissingROMs;
				}
			}
			set_rom(ROM::BASIC, *roms[0], false);
			set_rom(ROM::OS, *roms[1], false);

			if(target.has_dfs || target.has_adfs) {
				plus3_.reset(new Plus3);

				if(target.has_dfs) {
					set_rom(ROM::Slot0, *roms[dfs_rom_position], true);
				}
				if(target.has_adfs) {
					set_rom(ROM::Slot4, *roms[2], true);
					set_rom(ROM::Slot5, *roms[3], true);
				}
			}

			insert_media(target.media);

			if(!target.loading_command.empty()) {
				type_string(target.loading_command);
			}

			if(target.should_shift_restart) {
				shift_restart_counter_ = 1000000;
			}
		}

		~ConcreteMachine() {
			audio_queue_.flush();
		}

		void set_key_state(uint16_t key, bool isPressed) override final {
			if(key == KeyBreak) {
				m6502_.set_reset_line(isPressed);
			} else {
				if(isPressed)
					key_states_[key >> 4] |= key&0xf;
				else
					key_states_[key >> 4] &= ~(key&0xf);
			}
		}

		void clear_all_keys() override final {
			memset(key_states_, 0, sizeof(key_states_));
			if(is_holding_shift_) set_key_state(KeyShift, true);
		}

		bool insert_media(const Analyser::Static::Media &media) override final {
			if(!media.tapes.empty()) {
				tape_.set_tape(media.tapes.front());
			}
			set_use_fast_tape_hack();

			if(!media.disks.empty() && plus3_) {
				plus3_->set_disk(media.disks.front(), 0);
			}

			ROM slot = ROM::Slot12;
			for(std::shared_ptr<Storage::Cartridge::Cartridge> cartridge : media.cartridges) {
				const ROM first_slot_tried = slot;
				while(rom_inserted_[static_cast<int>(slot)]) {
					slot = static_cast<ROM>((static_cast<int>(slot) + 1) & 15);
					if(slot == first_slot_tried) return false;
				}
				set_rom(slot, cartridge->get_segments().front().data, false);
			}

			return !media.tapes.empty() || !media.disks.empty() || !media.cartridges.empty();
		}

		forceinline Cycles perform_bus_operation(CPU::MOS6502::BusOperation operation, uint16_t address, uint8_t *value) {
			unsigned int cycles = 1;

			if(address < 0x8000) {
				if(isReadOperation(operation)) {
					*value = ram_[address];
				} else {
					if(address >= video_access_range_.low_address && address <= video_access_range_.high_address) update_display();
					ram_[address] = *value;
				}

				// for the entire frame, RAM is accessible only on odd cycles; in modes below 4
				// it's also accessible only outside of the pixel regions
				cycles += video_output_.get_cycles_until_next_ram_availability(cycles_since_display_update_.as_int() + 1);
			} else {
				switch(address & 0xff0f) {
					case 0xfe00:
						if(isReadOperation(operation)) {
							*value = interrupt_status_;
							interrupt_status_ &= ~PowerOnReset;
						} else {
							interrupt_control_ = (*value) & ~1;
							evaluate_interrupts();
						}
					break;
					case 0xfe07:
						if(!isReadOperation(operation)) {
							// update speaker mode
							bool new_speaker_is_enabled = (*value & 6) == 2;
							if(new_speaker_is_enabled != speaker_is_enabled_) {
								update_audio();
								sound_generator_.set_is_enabled(new_speaker_is_enabled);
								speaker_is_enabled_ = new_speaker_is_enabled;
							}

							tape_.set_is_enabled((*value & 6) != 6);
							tape_.set_is_in_input_mode((*value & 6) == 0);
							tape_.set_is_running((*value & 0x40) ? true : false);

							caps_led_state_ = !!(*value & 0x80);
							if(activity_observer_)
								activity_observer_->set_led_status(caps_led, caps_led_state_);
						}

					// deliberate fallthrough; fe07 contains the display mode.

					case 0xfe02: case 0xfe03:
					case 0xfe08: case 0xfe09: case 0xfe0a: case 0xfe0b:
					case 0xfe0c: case 0xfe0d: case 0xfe0e: case 0xfe0f:
						if(!isReadOperation(operation)) {
							update_display();
							video_output_.set_register(address, *value);
							video_access_range_ = video_output_.get_memory_access_range();
							queue_next_display_interrupt();
						}
					break;
					case 0xfe04:
						if(isReadOperation(operation)) {
							*value = tape_.get_data_register();
							tape_.clear_interrupts(Interrupt::ReceiveDataFull);
						} else {
							tape_.set_data_register(*value);
							tape_.clear_interrupts(Interrupt::TransmitDataEmpty);
						}
					break;
					case 0xfe05:
						if(!isReadOperation(operation)) {
							const uint8_t interruptDisable = (*value)&0xf0;
							if( interruptDisable ) {
								if( interruptDisable&0x10 ) interrupt_status_ &= ~Interrupt::DisplayEnd;
								if( interruptDisable&0x20 ) interrupt_status_ &= ~Interrupt::RealTimeClock;
								if( interruptDisable&0x40 ) interrupt_status_ &= ~Interrupt::HighToneDetect;
								evaluate_interrupts();

								// TODO: NMI
							}

							// latch the paged ROM in case external hardware is being emulated
							active_rom_ = *value & 0xf;

							// apply the ULA's test
							if(*value & 0x08) {
								if(*value & 0x04) {
									keyboard_is_active_ = false;
									basic_is_active_ = false;
								} else {
									keyboard_is_active_ = !(*value & 0x02);
									basic_is_active_ = !keyboard_is_active_;
								}
							}
						}
					break;
					case 0xfe06:
						if(!isReadOperation(operation)) {
							update_audio();
							sound_generator_.set_divider(*value);
							tape_.set_counter(*value);
						}
					break;

					case 0xfc04: case 0xfc05: case 0xfc06: case 0xfc07:
						if(plus3_ && (address&0x00f0) == 0x00c0) {
							if(is_holding_shift_ && address == 0xfcc4) {
								is_holding_shift_ = false;
								set_key_state(KeyShift, false);
							}
							if(isReadOperation(operation))
								*value = plus3_->get_register(address);
							else
								plus3_->set_register(address, *value);
						}
					break;
					case 0xfc00:
						if(plus3_ && (address&0x00f0) == 0x00c0) {
							if(!isReadOperation(operation)) {
								plus3_->set_control_register(*value);
							} else *value = 1;
						}
					break;

					default:
						if(address >= 0xc000) {
							if(isReadOperation(operation)) {
								if(
									use_fast_tape_hack_ &&
									(operation == CPU::MOS6502::BusOperation::ReadOpcode) &&
									(
										(address == 0xf4e5) || (address == 0xf4e6) ||	// double NOPs at 0xf4e5, 0xf6de, 0xf6fa and 0xfa51
										(address == 0xf6de) || (address == 0xf6df) ||	// act to disable the normal branch into tape-handling
										(address == 0xf6fa) || (address == 0xf6fb) ||	// code, forcing the OS along the serially-accessed ROM
										(address == 0xfa51) || (address == 0xfa52) ||	// pathway.

										(address == 0xf0a8)								// 0xf0a8 is from where a service call would normally be
																						// dispatched; we can check whether it would be call 14
																						// (i.e. read byte) and, if so, whether the OS was about to
																						// issue a read byte call to a ROM despite being the tape
																						// FS being selected. If so then this is a get byte that
																						// we should service synthetically. Put the byte into Y
																						// and set A to zero to report that action was taken, then
																						// allow the PC read to return an RTS.
									)
								) {
									uint8_t service_call = static_cast<uint8_t>(m6502_.get_value_of_register(CPU::MOS6502::Register::X));
									if(address == 0xf0a8) {
										if(!ram_[0x247] && service_call == 14) {
											tape_.set_delegate(nullptr);

											int cycles_left_while_plausibly_in_data = 50;
											tape_.clear_interrupts(Interrupt::ReceiveDataFull);
											while(!tape_.get_tape()->is_at_end()) {
												tape_.run_for_input_pulse();
												cycles_left_while_plausibly_in_data--;
												if(!cycles_left_while_plausibly_in_data) fast_load_is_in_data_ = false;
												if(	(tape_.get_interrupt_status() & Interrupt::ReceiveDataFull) &&
													(fast_load_is_in_data_ || tape_.get_data_register() == 0x2a)
												) break;
											}
											tape_.set_delegate(this);
											tape_.clear_interrupts(Interrupt::ReceiveDataFull);
											interrupt_status_ |= tape_.get_interrupt_status();

											fast_load_is_in_data_ = true;
											m6502_.set_value_of_register(CPU::MOS6502::Register::A, 0);
											m6502_.set_value_of_register(CPU::MOS6502::Register::Y, tape_.get_data_register());
											*value = 0x60; // 0x60 is RTS
										}
										else *value = os_[address & 16383];
									}
									else *value = 0xea;
								} else {
									*value = os_[address & 16383];
								}
							}
						} else {
							if(isReadOperation(operation)) {
								*value = roms_[active_rom_][address & 16383];
								if(keyboard_is_active_) {
									*value &= 0xf0;
									for(int address_line = 0; address_line < 14; address_line++) {
										if(!(address&(1 << address_line))) *value |= key_states_[address_line];
									}
								}
								if(basic_is_active_) {
									*value &= roms_[static_cast<int>(ROM::BASIC)][address & 16383];
								}
							} else if(rom_write_masks_[active_rom_]) {
								roms_[active_rom_][address & 16383] = *value;
							}
						}
					break;
				}
			}

			cycles_since_display_update_ += Cycles(static_cast<int>(cycles));
			cycles_since_audio_update_ += Cycles(static_cast<int>(cycles));
			if(cycles_since_audio_update_ > Cycles(16384)) update_audio();
			tape_.run_for(Cycles(static_cast<int>(cycles)));

			cycles_until_display_interrupt_ -= cycles;
			if(cycles_until_display_interrupt_ < 0) {
				signal_interrupt(next_display_interrupt_);
				update_display();
				queue_next_display_interrupt();
			}

			if(typer_) typer_->run_for(Cycles(static_cast<int>(cycles)));
			if(plus3_) plus3_->run_for(Cycles(4*static_cast<int>(cycles)));
			if(shift_restart_counter_) {
				shift_restart_counter_ -= cycles;
				if(shift_restart_counter_ <= 0) {
					shift_restart_counter_ = 0;
					m6502_.set_power_on(true);
					set_key_state(KeyShift, true);
					is_holding_shift_ = true;
				}
			}

			return Cycles(static_cast<int>(cycles));
		}

		forceinline void flush() {
			update_display();
			update_audio();
			audio_queue_.perform();
		}

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

		void tape_did_change_interrupt_status(Tape *tape) override final {
			interrupt_status_ = (interrupt_status_ & ~(Interrupt::TransmitDataEmpty | Interrupt::ReceiveDataFull | Interrupt::HighToneDetect)) | tape_.get_interrupt_status();
			evaluate_interrupts();
		}

		HalfCycles get_typer_delay() override final {
			return m6502_.get_is_resetting() ? Cycles(625*25*128) : Cycles(0);	// wait one second if resetting
		}

		HalfCycles get_typer_frequency() override final {
			return Cycles(625*128*2);	// accept a new character every two frames
		}

		void type_string(const std::string &string) override final {
			std::unique_ptr<CharacterMapper> mapper(new CharacterMapper());
			Utility::TypeRecipient::add_typer(string, std::move(mapper));
		}

		KeyboardMapper *get_keyboard_mapper() override {
			return &keyboard_mapper_;
		}

		// MARK: - Configuration options.
		std::vector<std::unique_ptr<Configurable::Option>> get_options() override {
			return Electron::get_options();
		}

		void set_selections(const Configurable::SelectionSet &selections_by_option) override {
			bool quickload;
			if(Configurable::get_quick_load_tape(selections_by_option, quickload)) {
				allow_fast_tape_hack_ = quickload;
				set_use_fast_tape_hack();
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

		// MARK: - Activity Source
		void set_activity_observer(Activity::Observer *observer) override {
			activity_observer_ = observer;
			if(activity_observer_) {
				activity_observer_->register_led(caps_led);
				activity_observer_->set_led_status(caps_led, caps_led_state_);

				if(plus3_) {
					plus3_->set_activity_observer(observer);
				}
			}
		}

	private:
		enum class ROM {
			Slot0 = 0,
			Slot1,	Slot2,	Slot3,
			Slot4,	Slot5,	Slot6,	Slot7,

			Keyboard = 8,	Slot9,
			BASIC = 10,		Slot11,

			Slot12,	Slot13,	Slot14,	Slot15,

			OS,		DFS,
			ADFS1,	ADFS2
		};

		/*!
			Sets the contents of @c slot to @c data. If @c is_writeable is @c true then writing to the slot
			is enabled: it acts as if it were sideways RAM. Otherwise the slot is modelled as containing ROM.
		*/
		void set_rom(ROM slot, const std::vector<uint8_t> &data, bool is_writeable) {
			uint8_t *target = nullptr;
			switch(slot) {
				case ROM::DFS:		dfs_ = data;			return;
				case ROM::ADFS1:	adfs1_ = data;			return;
				case ROM::ADFS2:	adfs2_ = data;			return;

				case ROM::OS:		target = os_;			break;
				default:
					target = roms_[static_cast<int>(slot)];
					rom_write_masks_[static_cast<int>(slot)] = is_writeable;
				break;
			}

			// Copy in, with mirroring.
			std::size_t rom_ptr = 0;
			while(rom_ptr < 16384) {
				std::size_t size_to_copy = std::min(16384 - rom_ptr, data.size());
				std::memcpy(&target[rom_ptr], data.data(), size_to_copy);
				rom_ptr += size_to_copy;
			}

			if(static_cast<int>(slot) < 16)
				rom_inserted_[static_cast<int>(slot)] = true;
		}

		// MARK: - Work deferral updates.
		inline void update_display() {
			if(cycles_since_display_update_ > 0) {
				video_output_.run_for(cycles_since_display_update_.flush());
			}
		}

		inline void queue_next_display_interrupt() {
			VideoOutput::Interrupt next_interrupt = video_output_.get_next_interrupt();
			cycles_until_display_interrupt_ = next_interrupt.cycles;
			next_display_interrupt_ = next_interrupt.interrupt;
		}

		inline void update_audio() {
			speaker_.run_for(audio_queue_, cycles_since_audio_update_.divide(Cycles(SoundGenerator::clock_rate_divider)));
		}

		inline void signal_interrupt(Interrupt interrupt) {
			interrupt_status_ |= interrupt;
			evaluate_interrupts();
		}

		inline void clear_interrupt(Interrupt interrupt) {
			interrupt_status_ &= ~interrupt;
			evaluate_interrupts();
		}

		inline void evaluate_interrupts() {
			if(interrupt_status_ & interrupt_control_) {
				interrupt_status_ |= 1;
			} else {
				interrupt_status_ &= ~1;
			}
			m6502_.set_irq_line(interrupt_status_ & 1);
		}

		CPU::MOS6502::Processor<CPU::MOS6502::Personality::P6502, ConcreteMachine, false> m6502_;

		// Things that directly constitute the memory map.
		uint8_t roms_[16][16384];
		bool rom_inserted_[16] = {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false};
		bool rom_write_masks_[16] = {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false};
		uint8_t os_[16384], ram_[32768];
		std::vector<uint8_t> dfs_, adfs1_, adfs2_;

		// Paging
		int active_rom_ = static_cast<int>(ROM::Slot0);
		bool keyboard_is_active_ = false;
		bool basic_is_active_ = false;

		// Interrupt and keyboard state
		uint8_t interrupt_status_ = Interrupt::PowerOnReset | Interrupt::TransmitDataEmpty | 0x80;
		uint8_t interrupt_control_ = 0;
		uint8_t key_states_[14];
		Electron::KeyboardMapper keyboard_mapper_;

		// Counters related to simultaneous subsystems
		Cycles cycles_since_display_update_ = 0;
		Cycles cycles_since_audio_update_ = 0;
		int cycles_until_display_interrupt_ = 0;
		Interrupt next_display_interrupt_ = Interrupt::RealTimeClock;
		VideoOutput::Range video_access_range_ = {0, 0xffff};

		// Tape
		Tape tape_;
		bool use_fast_tape_hack_ = false;
		bool allow_fast_tape_hack_ = false;
		void set_use_fast_tape_hack() {
			use_fast_tape_hack_ = allow_fast_tape_hack_ && tape_.has_tape();
		}
		bool fast_load_is_in_data_ = false;

		// Disk
		std::unique_ptr<Plus3> plus3_;
		bool is_holding_shift_ = false;
		int shift_restart_counter_ = 0;

		// Outputs
		VideoOutput video_output_;

		Concurrency::DeferringAsyncTaskQueue audio_queue_;
		SoundGenerator sound_generator_;
		Outputs::Speaker::LowpassSpeaker<SoundGenerator> speaker_;

		bool speaker_is_enabled_ = false;

		// MARK: - Caps Lock status and the activity observer.
		const std::string caps_led = "CAPS";
		bool caps_led_state_ = false;
		Activity::Observer *activity_observer_ = nullptr;
};

}

using namespace Electron;

Machine *Machine::Electron(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	using Target = Analyser::Static::Acorn::Target;
	const Target *const acorn_target = dynamic_cast<const Target *>(target);
	return new Electron::ConcreteMachine(*acorn_target, rom_fetcher);
}

Machine::~Machine() {}
