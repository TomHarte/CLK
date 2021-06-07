//
//  Electron.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/01/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "Electron.hpp"

#include "../../Activity/Source.hpp"
#include "../MachineTypes.hpp"
#include "../../Configurable/Configurable.hpp"

#include "../../ClockReceiver/ClockReceiver.hpp"
#include "../../ClockReceiver/ForceInline.hpp"
#include "../../Configurable/StandardOptions.hpp"
#include "../../Outputs/Speaker/Implementation/LowpassSpeaker.hpp"
#include "../../Processors/6502/6502.hpp"

#include "../../Storage/MassStorage/SCSI/SCSI.hpp"
#include "../../Storage/MassStorage/SCSI/DirectAccessDevice.hpp"
#include "../../Storage/Tape/Tape.hpp"

#include "../Utility/Typer.hpp"
#include "../../Analyser/Static/Acorn/Target.hpp"

#include "../../ClockReceiver/JustInTime.hpp"

#include "Interrupts.hpp"
#include "Keyboard.hpp"
#include "Plus3.hpp"
#include "SoundGenerator.hpp"
#include "Tape.hpp"
#include "Video.hpp"

namespace Electron {

template <bool has_scsi_bus> class ConcreteMachine:
	public Machine,
	public MachineTypes::TimedMachine,
	public MachineTypes::ScanProducer,
	public MachineTypes::AudioProducer,
	public MachineTypes::MediaTarget,
	public MachineTypes::MappedKeyboardMachine,
	public Configurable::Device,
	public CPU::MOS6502::BusHandler,
	public Tape::Delegate,
	public Utility::TypeRecipient<CharacterMapper>,
	public Activity::Source,
	public SCSI::Bus::Observer,
	public ClockingHint::Observer {
	public:
		ConcreteMachine(const Analyser::Static::Acorn::Target &target, const ROMMachine::ROMFetcher &rom_fetcher) :
				m6502_(*this),
				scsi_bus_(4'000'000),
				hard_drive_(scsi_bus_, 0),
				scsi_device_(scsi_bus_.add_device()),
				video_(ram_),
				sound_generator_(audio_queue_),
				speaker_(sound_generator_) {
			memset(key_states_, 0, sizeof(key_states_));
			for(int c = 0; c < 16; c++)
				memset(roms_[c], 0xff, 16384);

			tape_.set_delegate(this);
			set_clock_rate(2000000);

			speaker_.set_input_rate(2000000 / SoundGenerator::clock_rate_divider);
			speaker_.set_high_frequency_cutoff(6000);

			::ROM::Request request = ::ROM::Request(::ROM::Name::AcornBASICII) && ::ROM::Request(::ROM::Name::AcornElectronMOS100);
			if(target.has_pres_adfs) {
				request = request && ::ROM::Request(::ROM::Name::PRESADFSSlot1) && ::ROM::Request(::ROM::Name::PRESADFSSlot2);
			}
			if(target.has_acorn_adfs) {
				request = request && ::ROM::Request(::ROM::Name::AcornADFS);
			}
			if(target.has_dfs) {
				request = request && ::ROM::Request(::ROM::Name::Acorn1770DFS);
			}
			if(target.has_ap6_rom) {
				request = request && ::ROM::Request(::ROM::Name::PRESAdvancedPlus6);
			}
			auto roms = rom_fetcher(request);
			if(!request.validate(roms)) {
				throw ROMMachine::Error::MissingROMs;
			}
			set_rom(ROM::BASIC, roms.find(::ROM::Name::AcornBASICII)->second, false);
			set_rom(ROM::OS, roms.find(::ROM::Name::AcornElectronMOS100)->second, false);

			/*
				ROM slot mapping applied:

					* the keyboard and BASIC ROMs occupy slots 8, 9, 10 and 11;
					* the DFS, if in use, occupies slot 1;
					* the Pres ADFS, if in use, occupies slots 4 and 5;
					* the Acorn ADFS, if in use, occupies slot 6;
					* the AP6, if in use, occupies slot 15; and
					* if sideways RAM was asked for, all otherwise unused slots are populated with sideways RAM.
			*/
			if(target.has_dfs || target.has_acorn_adfs || target.has_pres_adfs) {
				plus3_ = std::make_unique<Plus3>();

				if(target.has_dfs) {
					set_rom(ROM::Slot0, roms.find(::ROM::Name::Acorn1770DFS)->second, true);
				}
				if(target.has_pres_adfs) {
					set_rom(ROM::Slot4, roms.find(::ROM::Name::PRESADFSSlot1)->second, true);
					set_rom(ROM::Slot5, roms.find(::ROM::Name::PRESADFSSlot2)->second, true);
				}
				if(target.has_acorn_adfs) {
					set_rom(ROM::Slot6, roms.find(::ROM::Name::AcornADFS)->second, true);
				}
			}
			if(target.has_ap6_rom) {
				set_rom(ROM::Slot15, roms.find(::ROM::Name::PRESAdvancedPlus6)->second, true);
			}

			if(target.has_sideways_ram) {
				for(int c = 0; c < 16; c++) {
					if(rom_inserted_[c]) continue;
					if(c >= int(ROM::Keyboard) && c < int(ROM::BASIC)+1) continue;
					set_sideways_ram(ROM(c));
				}
			}

			insert_media(target.media);

			if(!target.loading_command.empty()) {
				type_string(target.loading_command);
			}

			if(target.should_shift_restart) {
				shift_restart_counter_ = 1000000;
			}

			if(has_scsi_bus) {
				scsi_bus_.add_observer(this);
				scsi_bus_.set_clocking_hint_observer(this);
			}
		}

		~ConcreteMachine() {
			audio_queue_.flush();
		}

		void set_key_state(uint16_t key, bool isPressed) final {
			switch(key) {
				default:
					if(isPressed)
						key_states_[key >> 4] |= key&0xf;
					else
						key_states_[key >> 4] &= ~(key&0xf);
				break;

				case KeyBreak:
					m6502_.set_reset_line(isPressed);
				break;

#define FuncShiftedKey(source, dest)	\
				case source:	\
					set_key_state(KeyFunc, isPressed);	\
					set_key_state(dest, isPressed);	\
				break;

				FuncShiftedKey(KeyF1, Key1);
				FuncShiftedKey(KeyF2, Key2);
				FuncShiftedKey(KeyF3, Key3);
				FuncShiftedKey(KeyF4, Key4);
				FuncShiftedKey(KeyF5, Key5);
				FuncShiftedKey(KeyF6, Key6);
				FuncShiftedKey(KeyF7, Key7);
				FuncShiftedKey(KeyF8, Key8);
				FuncShiftedKey(KeyF9, Key9);
				FuncShiftedKey(KeyF0, Key0);

#undef FuncShiftedKey
			}
		}

		void clear_all_keys() final {
			memset(key_states_, 0, sizeof(key_states_));
			if(is_holding_shift_) set_key_state(KeyShift, true);
		}

		bool insert_media(const Analyser::Static::Media &media) final {
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
				while(rom_inserted_[int(slot)]) {
					slot = ROM((int(slot) + 1) & 15);
					if(slot == first_slot_tried) return false;
				}
				set_rom(slot, cartridge->get_segments().front().data, false);
			}

			// TODO: allow this only at machine startup?
			if(!media.mass_storage_devices.empty()) {
				hard_drive_->set_storage(media.mass_storage_devices.front());
			}

			return !media.empty();
		}

		forceinline Cycles perform_bus_operation(CPU::MOS6502::BusOperation operation, uint16_t address, uint8_t *value) {
			unsigned int cycles = 1;

			if(address < 0x8000) {
				if(isReadOperation(operation)) {
					*value = ram_[address];
				} else {
					if(address >= video_access_range_.low_address && address <= video_access_range_.high_address) {
						video_.flush();
					}
					ram_[address] = *value;
				}

				// For the entire frame, RAM is accessible only on odd cycles; in modes below 4
				// it's also accessible only outside of the pixel regions.
				cycles += video_.last_valid()->get_cycles_until_next_ram_availability(video_.time_since_flush().template as<int>() + 1);
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

						[[fallthrough]];	// fe07 contains the display mode.


					case 0xfe02: case 0xfe03:
					case 0xfe08: case 0xfe09: case 0xfe0a: case 0xfe0b:
					case 0xfe0c: case 0xfe0d: case 0xfe0e: case 0xfe0f:
						if(!isReadOperation(operation)) {
							video_->write(address, *value);
							video_access_range_ = video_.last_valid()->get_memory_access_range();
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
								*value = plus3_->read(address);
							else
								plus3_->write(address, *value);
						}
					break;
					case 0xfc00:
						if(plus3_ && (address&0x00f0) == 0x00c0) {
							if(!isReadOperation(operation)) {
								plus3_->set_control_register(*value);
							} else *value = 1;
						}

						if(has_scsi_bus && (address&0x00f0) == 0x0040) {
							scsi_acknowledge_ = true;
							if(!isReadOperation(operation)) {
								scsi_data_ = *value;
								push_scsi_output();
							} else {
								*value = SCSI::data_lines(scsi_bus_.get_state());
								push_scsi_output();
							}
						}
					break;
					case 0xfc03:
						if(has_scsi_bus && (address&0x00f0) == 0x0040) {
							scsi_interrupt_state_ = false;
							scsi_interrupt_mask_ = *value & 1;
							evaluate_interrupts();
						}
					break;
					case 0xfc01:
						if(has_scsi_bus && (address&0x00f0) == 0x0040 && isReadOperation(operation)) {
							// Status byte is:
							//
							//	b7:	SCSI C/D
							//	b6: SCSI I/O
							//	b5: SCSI REQ
							//	b4: interrupt flag
							//	b3:	0
							//	b2:	0
							//	b1:	SCSI BSY
							//	b0: SCSI MSG
							const auto state = scsi_bus_.get_state();
							*value =
								(state & SCSI::Line::Control ? 0x80 : 0x00) |
								(state & SCSI::Line::Input ? 0x40 : 0x00) |
								(state & SCSI::Line::Request ? 0x20 : 0x00) |
								((scsi_interrupt_state_ && scsi_interrupt_mask_) ? 0x10 : 0x00) |
								(state & SCSI::Line::Busy ? 0x02 : 0x00) |
								(state & SCSI::Line::Message ? 0x01 : 0x00);

							// Empirical guess: this is also the trigger to affect busy/request/acknowledge
							// signalling. Maybe?
							if(scsi_select_ && scsi_bus_.get_state() & SCSI::Line::Busy) {
								scsi_select_ = false;
								push_scsi_output();
							}
						}
					break;
					case 0xfc02:
						if(has_scsi_bus && (address&0x00f0) == 0x0040) {
							scsi_select_ = true;
							push_scsi_output();
						}
					break;

					// SCSI locations:
					//
					//	fc40:	data, read and write
					//	fc41:	status read
					//	fc42:	select write
					//	fc43:	interrupt latch
					//
					//
					// Interrupt latch is:
					//
					//	b0: enable or disable IRQ on REQ
					//	(and, possibly, writing to the latch acknowledges?)

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
																						// issue a read byte call to a ROM despite the tape
																						// FS being selected. If so then this is a get byte that
																						// we should service synthetically. Put the byte into Y
																						// and set A to zero to report that action was taken, then
																						// allow the PC read to return an RTS.
									)
								) {
									const auto service_call = uint8_t(m6502_.get_value_of_register(CPU::MOS6502::Register::X));
									if(address == 0xf0a8) {
										if(!ram_[0x247] && service_call == 14) {
											tape_.set_delegate(nullptr);

											int cycles_left_while_plausibly_in_data = 50;
											tape_.clear_interrupts(Interrupt::ReceiveDataFull);
											while(!tape_.get_tape()->is_at_end()) {
												tape_.run_for_input_pulse();
												--cycles_left_while_plausibly_in_data;
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
									*value &= roms_[int(ROM::BASIC)][address & 16383];
								}
							} else if(rom_write_masks_[active_rom_]) {
								roms_[active_rom_][address & 16383] = *value;
							}
						}
					break;
				}
			}

			if(video_ += Cycles(int(cycles))) {
				signal_interrupt(video_.last_valid()->get_interrupts());
			}

			cycles_since_audio_update_ += Cycles(int(cycles));
			if(cycles_since_audio_update_ > Cycles(16384)) update_audio();
			tape_.run_for(Cycles(int(cycles)));

			if(typer_) typer_->run_for(Cycles(int(cycles)));
			if(plus3_) plus3_->run_for(Cycles(4*int(cycles)));
			if(shift_restart_counter_) {
				shift_restart_counter_ -= cycles;
				if(shift_restart_counter_ <= 0) {
					shift_restart_counter_ = 0;
					m6502_.set_power_on(true);
					set_key_state(KeyShift, true);
					is_holding_shift_ = true;
				}
			}

			if constexpr (has_scsi_bus) {
				if(scsi_is_clocked_) {
					scsi_bus_.run_for(Cycles(int(cycles)));
				}
			}

			return Cycles(int(cycles));
		}

		forceinline void flush() {
			video_.flush();
			update_audio();
			audio_queue_.perform();
		}

		void set_scan_target(Outputs::Display::ScanTarget *scan_target) final {
			video_.last_valid()->set_scan_target(scan_target);
		}

		Outputs::Display::ScanStatus get_scaled_scan_status() const final {
			return video_.last_valid()->get_scaled_scan_status();
		}

		void set_display_type(Outputs::Display::DisplayType display_type) final {
			video_.last_valid()->set_display_type(display_type);
		}

		Outputs::Display::DisplayType get_display_type() const final {
			return video_.last_valid()->get_display_type();
		}

		Outputs::Speaker::Speaker *get_speaker() final {
			return &speaker_;
		}

		void run_for(const Cycles cycles) final {
			m6502_.run_for(cycles);
		}

		void scsi_bus_did_change(SCSI::Bus *, SCSI::BusState new_state, double) final {
			// Release acknowledge when request is released.
			if(scsi_acknowledge_ && !(new_state & SCSI::Line::Request)) {
				scsi_acknowledge_ = false;
				push_scsi_output();
			}

			// Output occurs only while SCSI::Line::Input is inactive; therefore a change
			// in that line affects what's on the bus.
			if(((new_state^previous_bus_state_)&SCSI::Line::Input)) {
				push_scsi_output();
			}

			scsi_interrupt_state_ |= (new_state^previous_bus_state_)&new_state & SCSI::Line::Request;
			previous_bus_state_ = new_state;
			evaluate_interrupts();
		}

		void set_component_prefers_clocking(ClockingHint::Source *, ClockingHint::Preference preference) final {
			scsi_is_clocked_ = preference != ClockingHint::Preference::None;
		}

		void tape_did_change_interrupt_status(Tape *) final {
			interrupt_status_ = (interrupt_status_ & ~(Interrupt::TransmitDataEmpty | Interrupt::ReceiveDataFull | Interrupt::HighToneDetect)) | tape_.get_interrupt_status();
			evaluate_interrupts();
		}

		HalfCycles get_typer_delay(const std::string &text) const final {
			if(!m6502_.get_is_resetting()) {
				return Cycles(0);
			}

			// Add a longer delay for a command at reset that involves pressing a modifier;
			// empirically this seems to be a requirement, in order to avoid a collision with
			// the system's built-in modifier-at-startup test (e.g. to perform shift+break).
			CharacterMapper test_mapper;
			const uint16_t *const sequence = test_mapper.sequence_for_character(text[0]);
			return is_modifier(Key(sequence[0])) ? Cycles(1'000'000) : Cycles(750'000);
		}

		HalfCycles get_typer_frequency() const final {
			return Cycles(60'000);
		}

		void type_string(const std::string &string) final {
			Utility::TypeRecipient<CharacterMapper>::add_typer(string);
		}

		bool can_type(char c) const final {
			return Utility::TypeRecipient<CharacterMapper>::can_type(c);
		}

		KeyboardMapper *get_keyboard_mapper() final {
			return &keyboard_mapper_;
		}

		// MARK: - Configuration options.
		std::unique_ptr<Reflection::Struct> get_options() final {
			auto options = std::make_unique<Options>(Configurable::OptionsType::UserFriendly);
			options->output = get_video_signal_configurable();
			options->quickload = allow_fast_tape_hack_;
			return options;
		}

		void set_options(const std::unique_ptr<Reflection::Struct> &str) final {
			const auto options = dynamic_cast<Options *>(str.get());

			set_video_signal_configurable(options->output);
			allow_fast_tape_hack_ = options->quickload;
			set_use_fast_tape_hack();
		}

		// MARK: - Activity Source
		void set_activity_observer(Activity::Observer *observer) final {
			activity_observer_ = observer;
			if(activity_observer_) {
				activity_observer_->register_led(caps_led);
				activity_observer_->set_led_status(caps_led, caps_led_state_);
			}

			if(plus3_) {
				plus3_->set_activity_observer(observer);
			}

			if(has_scsi_bus) {
				scsi_bus_.set_activity_observer(observer);
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
					target = roms_[int(slot)];
					rom_write_masks_[int(slot)] = is_writeable;
				break;
			}

			// Copy in, with mirroring.
			std::size_t rom_ptr = 0;
			while(rom_ptr < 16384) {
				std::size_t size_to_copy = std::min(16384 - rom_ptr, data.size());
				std::memcpy(&target[rom_ptr], data.data(), size_to_copy);
				rom_ptr += size_to_copy;
			}

			if(int(slot) < 16) {
				rom_inserted_[int(slot)] = true;
			}
		}

		/*!
			Enables @c slot as sideways RAM; ensures that it does not currently contain a valid ROM signature.
		*/
		void set_sideways_ram(ROM slot) {
			std::memset(roms_[int(slot)], 0xff, 16*1024);
			if(int(slot) < 16) {
				rom_inserted_[int(slot)] = true;
				rom_write_masks_[int(slot)] = true;
			}
		}

		// MARK: - Work deferral updates.
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

			if constexpr (has_scsi_bus) {
				m6502_.set_irq_line((scsi_interrupt_state_ && scsi_interrupt_mask_) | (interrupt_status_ & 1));
			} else {
				m6502_.set_irq_line(interrupt_status_ & 1);
			}
		}

		CPU::MOS6502::Processor<CPU::MOS6502::Personality::P6502, ConcreteMachine, false> m6502_;

		// Things that directly constitute the memory map.
		uint8_t roms_[16][16384];
		bool rom_inserted_[16] = {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false};
		bool rom_write_masks_[16] = {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false};
		uint8_t os_[16384], ram_[32768];
		std::vector<uint8_t> dfs_, adfs1_, adfs2_;

		// Paging
		int active_rom_ = int(ROM::Slot0);
		bool keyboard_is_active_ = false;
		bool basic_is_active_ = false;

		// Interrupt and keyboard state
		uint8_t interrupt_status_ = Interrupt::PowerOnReset | Interrupt::TransmitDataEmpty | 0x80;
		uint8_t interrupt_control_ = 0;
		uint8_t key_states_[14];
		Electron::KeyboardMapper keyboard_mapper_;

		// Counters related to simultaneous subsystems
		Cycles cycles_since_audio_update_ = 0;
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

		// Hard drive.
		SCSI::Bus scsi_bus_;
		SCSI::Target::Target<SCSI::DirectAccessDevice> hard_drive_;
		SCSI::BusState previous_bus_state_ = SCSI::DefaultBusState;
		const size_t scsi_device_ = 0;
		uint8_t scsi_data_ = 0;
		bool scsi_select_ = false;
		bool scsi_acknowledge_ = false;
		bool scsi_is_clocked_ = false;
		bool scsi_interrupt_state_ = false;
		bool scsi_interrupt_mask_ = false;
		void push_scsi_output() {
			scsi_bus_.set_device_output(scsi_device_,
				(scsi_bus_.get_state()&SCSI::Line::Input ? 0 : scsi_data_) |
				(scsi_select_ ? SCSI::Line::SelectTarget : 0) |
				(scsi_acknowledge_ ? SCSI::Line::Acknowledge : 0)
			);
		}

		// Outputs
		JustInTimeActor<VideoOutput, Cycles> video_;

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

	if(acorn_target->media.mass_storage_devices.empty()) {
		return new Electron::ConcreteMachine<false>(*acorn_target, rom_fetcher);
	} else {
		return new Electron::ConcreteMachine<true>(*acorn_target, rom_fetcher);
	}
}

Machine::~Machine() {}
