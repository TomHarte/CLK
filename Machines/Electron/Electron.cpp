//
//  Electron.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Electron.hpp"

#include "../../Processors/6502/6502.hpp"
#include "../../Storage/Tape/Tape.hpp"
#include "../../ClockReceiver/ClockReceiver.hpp"
#include "../../ClockReceiver/ForceInline.hpp"

#include "../Utility/Typer.hpp"

#include "Interrupts.hpp"
#include "Keyboard.hpp"
#include "Plus3.hpp"
#include "Speaker.hpp"
#include "Tape.hpp"
#include "Video.hpp"

namespace Electron {

class ConcreteMachine:
	public Machine,
	public CPU::MOS6502::BusHandler,
	public Tape::Delegate,
	public Utility::TypeRecipient {
	public:
		ConcreteMachine() : m6502_(*this) {
			memset(key_states_, 0, sizeof(key_states_));
			for(int c = 0; c < 16; c++)
				memset(roms_[c], 0xff, 16384);

			tape_.set_delegate(this);
			set_clock_rate(2000000);
		}

		void set_rom(ROMSlot slot, std::vector<uint8_t> data, bool is_writeable) override final {
			uint8_t *target = nullptr;
			switch(slot) {
				case ROMSlotDFS:	dfs_ = data;			return;
				case ROMSlotADFS:	adfs_ = data;			return;

				case ROMSlotOS:		target = os_;			break;
				default:
					target = roms_[slot];
					rom_write_masks_[slot] = is_writeable;
				break;
			}

			memcpy(target, &data[0], std::min((size_t)16384, data.size()));
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

		void set_use_fast_tape_hack(bool activate) override final {
			use_fast_tape_hack_ = activate;
		}

		void configure_as_target(const StaticAnalyser::Target &target) override final {
			if(target.loadingCommand.length()) {
				set_typer_for_string(target.loadingCommand.c_str());
			}

			if(target.acorn.should_shift_restart) {
				shift_restart_counter_ = 1000000;
			}

			if(target.acorn.has_dfs || target.acorn.has_adfs) {
				plus3_.reset(new Plus3);

				if(target.acorn.has_dfs) {
					set_rom(ROMSlot0, dfs_, true);
				}
				if(target.acorn.has_adfs) {
					set_rom(ROMSlot4, adfs_, true);
					set_rom(ROMSlot5, std::vector<uint8_t>(adfs_.begin() + 16384, adfs_.end()), true);
				}
			}

			insert_media(target.media);
		}

		bool insert_media(const StaticAnalyser::Media &media) override final {
			if(!media.tapes.empty()) {
				tape_.set_tape(media.tapes.front());
			}

			if(!media.disks.empty() && plus3_) {
				plus3_->set_disk(media.disks.front(), 0);
			}

			ROMSlot slot = ROMSlot12;
			for(std::shared_ptr<Storage::Cartridge::Cartridge> cartridge : media.cartridges) {
				set_rom(slot, cartridge->get_segments().front().data, false);
				slot = static_cast<ROMSlot>((static_cast<int>(slot) + 1)&15);
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
				cycles += video_output_->get_cycles_until_next_ram_availability(cycles_since_display_update_.as_int() + 1);
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
								speaker_->set_is_enabled(new_speaker_is_enabled);
								speaker_is_enabled_ = new_speaker_is_enabled;
							}

							tape_.set_is_enabled((*value & 6) != 6);
							tape_.set_is_in_input_mode((*value & 6) == 0);
							tape_.set_is_running(((*value)&0x40) ? true : false);

							// TODO: caps lock LED
						}

					// deliberate fallthrough
					case 0xfe02: case 0xfe03:
					case 0xfe08: case 0xfe09: case 0xfe0a: case 0xfe0b:
					case 0xfe0c: case 0xfe0d: case 0xfe0e: case 0xfe0f:
						if(!isReadOperation(operation)) {
							update_display();
							video_output_->set_register(address, *value);
							video_access_range_ = video_output_->get_memory_access_range();
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
							active_rom_ = (Electron::ROMSlot)(*value & 0xf);

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
							speaker_->set_divider(*value);
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
									tape_.has_tape() &&
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

											// TODO: handle tape wrap around.

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
									*value &= roms_[ROMSlotBASIC][address & 16383];
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
			speaker_->flush();
		}

		void setup_output(float aspect_ratio) override final {
			video_output_.reset(new VideoOutput(ram_));

			// The maximum output frequency is 62500Hz and all other permitted output frequencies are integral divisions of that;
			// however setting the speaker on or off can happen on any 2Mhz cycle, and probably (?) takes effect immediately. So
			// run the speaker at a 2000000Hz input rate, at least for the time being.
			speaker_.reset(new Speaker);
			speaker_->set_input_rate(2000000 / Speaker::clock_rate_divider);
		}

		void close_output() override final {
			video_output_.reset();
		}

		std::shared_ptr<Outputs::CRT::CRT> get_crt() override final {
			return video_output_->get_crt();
		}

		std::shared_ptr<Outputs::Speaker> get_speaker() override final {
			return speaker_;
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

		void set_typer_for_string(const char *string) override final {
			std::unique_ptr<CharacterMapper> mapper(new CharacterMapper());
			Utility::TypeRecipient::set_typer_for_string(string, std::move(mapper));
		}

		KeyboardMapper &get_keyboard_mapper() override {
			return keyboard_mapper_;
		}

	private:
		inline void update_display() {
			if(cycles_since_display_update_ > 0) {
				video_output_->run_for(cycles_since_display_update_.flush());
			}
		}

		inline void queue_next_display_interrupt() {
			VideoOutput::Interrupt next_interrupt = video_output_->get_next_interrupt();
			cycles_until_display_interrupt_ = next_interrupt.cycles;
			next_display_interrupt_ = next_interrupt.interrupt;
		}

		inline void update_audio() {
			if(cycles_since_audio_update_ > 0) {
				speaker_->run_for(cycles_since_audio_update_.divide(Cycles(Speaker::clock_rate_divider)));
			}
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

		CPU::MOS6502::Processor<ConcreteMachine, false> m6502_;

		// Things that directly constitute the memory map.
		uint8_t roms_[16][16384];
		bool rom_write_masks_[16] = {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false};
		uint8_t os_[16384], ram_[32768];
		std::vector<uint8_t> dfs_, adfs_;

		// Paging
		ROMSlot active_rom_ = ROMSlot::ROMSlot0;
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
		bool fast_load_is_in_data_ = false;

		// Disk
		std::unique_ptr<Plus3> plus3_;
		bool is_holding_shift_ = false;
		int shift_restart_counter_ = 0;

		// Outputs
		std::unique_ptr<VideoOutput> video_output_;
		std::shared_ptr<Speaker> speaker_;
		bool speaker_is_enabled_ = false;
};

}

using namespace Electron;

Machine *Machine::Electron() {
	return new Electron::ConcreteMachine;
}

Machine::~Machine() {}
