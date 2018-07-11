//
//  AppleII.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/04/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#include "AppleII.hpp"

#include "../../Activity/Source.hpp"
#include "../MediaTarget.hpp"
#include "../CRTMachine.hpp"
#include "../JoystickMachine.hpp"
#include "../KeyboardMachine.hpp"
#include "../Utility/MemoryFuzzer.hpp"
#include "../Utility/StringSerialiser.hpp"

#include "../../Processors/6502/6502.hpp"
#include "../../Components/AudioToggle/AudioToggle.hpp"

#include "../../Outputs/Speaker/Implementation/LowpassSpeaker.hpp"

#include "Card.hpp"
#include "DiskIICard.hpp"
#include "Video.hpp"

#include "../../Analyser/Static/AppleII/Target.hpp"
#include "../../ClockReceiver/ForceInline.hpp"
#include "../../Configurable/Configurable.hpp"
#include "../../Storage/Disk/Track/TrackSerialiser.hpp"
#include "../../Storage/Disk/Encodings/AppleGCR/SegmentParser.hpp"

#include <algorithm>
#include <array>
#include <memory>

std::vector<std::unique_ptr<Configurable::Option>> AppleII::get_options() {
	std::vector<std::unique_ptr<Configurable::Option>> options;
	options.emplace_back(new Configurable::BooleanOption("Accelerate DOS 3.3", "quickload"));
	return options;
}

namespace {

class ConcreteMachine:
	public CRTMachine::Machine,
	public MediaTarget::Machine,
	public KeyboardMachine::Machine,
	public Configurable::Device,
	public CPU::MOS6502::BusHandler,
	public Inputs::Keyboard,
	public AppleII::Machine,
	public Activity::Source,
	public JoystickMachine::Machine,
	public AppleII::Card::Delegate {
	private:
		struct VideoBusHandler : public AppleII::Video::BusHandler {
			public:
				VideoBusHandler(uint8_t *ram) : ram_(ram) {}

				uint8_t perform_read(uint16_t address) {
					return ram_[address];
				}

			private:
				uint8_t *ram_;
		};

		CPU::MOS6502::Processor<ConcreteMachine, false> m6502_;
		VideoBusHandler video_bus_handler_;
		std::unique_ptr<AppleII::Video::Video<VideoBusHandler>> video_;
		int cycles_into_current_line_ = 0;
		Cycles cycles_since_video_update_;

		void update_video() {
			video_->run_for(cycles_since_video_update_.flush());
		}
		static const int audio_divider = 8;
		void update_audio() {
			speaker_.run_for(audio_queue_, cycles_since_audio_update_.divide(Cycles(audio_divider)));
		}
		void update_just_in_time_cards() {
			for(const auto &card : just_in_time_cards_) {
				card->run_for(cycles_since_card_update_, stretched_cycles_since_card_update_);
			}
			cycles_since_card_update_ = 0;
			stretched_cycles_since_card_update_ = 0;
		}

		uint8_t ram_[65536], aux_ram_[65536];
		std::vector<uint8_t> rom_;
		std::vector<uint8_t> character_rom_;
		uint8_t keyboard_input_ = 0x00;

		Concurrency::DeferringAsyncTaskQueue audio_queue_;
		Audio::Toggle audio_toggle_;
		Outputs::Speaker::LowpassSpeaker<Audio::Toggle> speaker_;
		Cycles cycles_since_audio_update_;

		// MARK: - Cards
		std::array<std::unique_ptr<AppleII::Card>, 7> cards_;
		Cycles cycles_since_card_update_;
		std::vector<AppleII::Card *> every_cycle_cards_;
		std::vector<AppleII::Card *> just_in_time_cards_;

		int stretched_cycles_since_card_update_ = 0;

		void install_card(std::size_t slot, AppleII::Card *card) {
			assert(slot >= 1 && slot < 8);
			cards_[slot - 1].reset(card);
			card->set_delegate(this);
			pick_card_messaging_group(card);
		}

		bool is_every_cycle_card(AppleII::Card *card) {
			return !card->get_select_constraints();
		}

		void pick_card_messaging_group(AppleII::Card *card) {
			const bool is_every_cycle = is_every_cycle_card(card);
			std::vector<AppleII::Card *> &intended = is_every_cycle ? every_cycle_cards_ : just_in_time_cards_;
		 	std::vector<AppleII::Card *> &undesired = is_every_cycle ? just_in_time_cards_ : every_cycle_cards_;

			if(std::find(intended.begin(), intended.end(), card) != intended.end()) return;
			auto old_membership = std::find(undesired.begin(), undesired.end(), card);
			if(old_membership != undesired.end()) undesired.erase(old_membership);
			intended.push_back(card);
		}

		void card_did_change_select_constraints(AppleII::Card *card) override {
			pick_card_messaging_group(card);
		}

		AppleII::DiskIICard *diskii_card() {
			return dynamic_cast<AppleII::DiskIICard *>(cards_[5].get());
		}

		// MARK: - Memory Map
		struct MemoryBlock {
			uint8_t *read_pointer = nullptr;
			uint8_t *write_pointer = nullptr;
		} memory_blocks_[4];	// The IO page isn't included.

		// MARK: - The language card.
		struct {
			bool bank1 = false;
			bool read = false;
			bool pre_write = false;
			bool write = false;
		} language_card_;
		bool has_language_card_ = true;
		void set_language_card_paging() {
			if(has_language_card_ && !language_card_.write) {
				memory_blocks_[2].write_pointer = &ram_[48*1024 + (language_card_.bank1 ? 0x1000 : 0x0000)];
				memory_blocks_[3].write_pointer = &ram_[56*1024];
			} else {
				memory_blocks_[2].write_pointer = memory_blocks_[3].write_pointer = nullptr;
			}

			if(has_language_card_ && language_card_.read) {
				memory_blocks_[2].read_pointer = &ram_[48*1024 + (language_card_.bank1 ? 0x1000 : 0x0000)];
				memory_blocks_[3].read_pointer = &ram_[56*1024];
			} else {
				memory_blocks_[2].read_pointer = rom_.data();
				memory_blocks_[3].read_pointer = rom_.data() + 0x1000;
			}
		}

		// MARK - typing
		std::unique_ptr<Utility::StringSerialiser> string_serialiser_;

		// MARK - quick loading
		bool should_load_quickly_ = false;

		// MARK - joysticks
		class Joystick: public Inputs::ConcreteJoystick {
			public:
				Joystick() :
					ConcreteJoystick({
						Input(Input::Horizontal),
						Input(Input::Vertical),

						// The Apple II offers three buttons between two joysticks;
						// this emulator puts three buttons on each joystick and
						// combines them.
						Input(Input::Fire, 0),
						Input(Input::Fire, 1),
						Input(Input::Fire, 2),
					}) {}

					void did_set_input(const Input &input, float value) override {
						if(!input.info.control.index && (input.type == Input::Type::Horizontal || input.type == Input::Type::Vertical))
							axes[(input.type == Input::Type::Horizontal) ? 0 : 1] = 1.0f - value;
					}

					void did_set_input(const Input &input, bool value) override {
						if(input.type == Input::Type::Fire && input.info.control.index < 3) {
							buttons[input.info.control.index] = value;
						}
					}

				bool buttons[3] = {false, false, false};
				float axes[2] = {0.5f, 0.5f};
		};

		// On an Apple II, the programmer strobes 0xc070 and that causes each analogue input
		// to begin a charge and discharge cycle **if they are not already charging**.
		// The greater the analogue input, the faster they will charge and therefore the sooner
		// they will discharge.
		//
		// This emulator models that with analogue_charge_ being essentially the amount of time,
		// in charge threshold units, since 0xc070 was last strobed. But if any of the analogue
		// inputs were already partially charged then they gain a bias in analogue_biases_.
		//
		// It's a little indirect, but it means only having to increment the one value in the
		// main loop.
		float analogue_charge_ = 0.0f;
		float analogue_biases_[4] = {0.0f, 0.0f, 0.0f, 0.0f};

		std::vector<std::unique_ptr<Inputs::Joystick>> joysticks_;
		bool analogue_channel_is_discharged(size_t channel) {
			return static_cast<Joystick *>(joysticks_[channel >> 1].get())->axes[channel & 1] < analogue_charge_ + analogue_biases_[channel];
		}

	public:
		ConcreteMachine(const Analyser::Static::AppleII::Target &target, const ROMMachine::ROMFetcher &rom_fetcher):
		 	m6502_(*this),
		 	video_bus_handler_(ram_),
		 	audio_toggle_(audio_queue_),
		 	speaker_(audio_toggle_) {
		 	// The system's master clock rate.
		 	const float master_clock = 14318180.0;

		 	// This is where things get slightly convoluted: establish the machine as having a clock rate
		 	// equal to the number of cycles of work the 6502 will actually achieve. Which is less than
		 	// the master clock rate divided by 14 because every 65th cycle is extended by one seventh.
			set_clock_rate((master_clock / 14.0) * 65.0 / (65.0 + 1.0 / 7.0));

			// The speaker, however, should think it is clocked at half the master clock, per a general
			// decision to sample it at seven times the CPU clock (plus stretches).
			speaker_.set_input_rate(static_cast<float>(master_clock / (2.0 * static_cast<float>(audio_divider))));

			// Apply a 6Khz low-pass filter. This was picked by ear and by an attempt to understand the
			// Apple II schematic but, well, I don't claim much insight on the latter. This is definitely
			// something to review in the future.
			speaker_.set_high_frequency_cutoff(6000);

			// Also, start with randomised memory contents.
			Memory::Fuzz(ram_, sizeof(ram_));

			// Add a couple of joysticks.
		 	joysticks_.emplace_back(new Joystick);
		 	joysticks_.emplace_back(new Joystick);

			// Pick the required ROMs.
			using Target = Analyser::Static::AppleII::Target;
			std::vector<std::string> rom_names = {"apple2-character.rom"};
			switch(target.model) {
				default:
					rom_names.push_back("apple2o.rom");
				break;
				case Target::Model::IIplus:
					rom_names.push_back("apple2.rom");
				break;
			}
			const auto roms = rom_fetcher("AppleII", rom_names);

			if(!roms[0] || !roms[1]) {
				throw ROMMachine::Error::MissingROMs;
			}

			character_rom_ = std::move(*roms[0]);
			rom_ = std::move(*roms[1]);
			if(rom_.size() > 12*1024) {
				rom_.erase(rom_.begin(), rom_.begin() + static_cast<off_t>(rom_.size()) - 12*1024);
			}

			if(target.disk_controller != Target::DiskController::None) {
				// Apple recommended slot 6 for the (first) Disk II.
				install_card(6, new AppleII::DiskIICard(rom_fetcher, target.disk_controller == Target::DiskController::SixteenSector));
			}

			// Set up the default memory blocks.
			memory_blocks_[0].read_pointer = memory_blocks_[0].write_pointer = ram_;
			memory_blocks_[1].read_pointer = memory_blocks_[1].write_pointer = &ram_[0x200];
			set_language_card_paging();

			insert_media(target.media);
		}

		~ConcreteMachine() {
			audio_queue_.flush();
		}

		void setup_output(float aspect_ratio) override {
			video_.reset(new AppleII::Video::Video<VideoBusHandler>(video_bus_handler_));
			video_->set_character_rom(character_rom_);
		}

		void close_output() override {
			video_.reset();
		}

		Outputs::CRT::CRT *get_crt() override {
			return video_->get_crt();
		}

		Outputs::Speaker::Speaker *get_speaker() override {
			return &speaker_;
		}

		forceinline Cycles perform_bus_operation(const CPU::MOS6502::BusOperation operation, const uint16_t address, uint8_t *const value) {
			++ cycles_since_video_update_;
			++ cycles_since_card_update_;
			cycles_since_audio_update_ += Cycles(7);

			// The Apple II has a slightly weird timing pattern: every 65th CPU cycle is stretched
			// by an extra 1/7th. That's because one cycle lasts 3.5 NTSC colour clocks, so after
			// 65 cycles a full line of 227.5 colour clocks have passed. But the high-rate binary
			// signal approximation that produces colour needs to be in phase, so a stretch of exactly
			// 0.5 further colour cycles is added. The video class handles that implicitly, but it
			// needs to be accumulated here for the audio.
			cycles_into_current_line_ = (cycles_into_current_line_ + 1) % 65;
			const bool is_stretched_cycle = !cycles_into_current_line_;
			if(is_stretched_cycle) {
				++ cycles_since_audio_update_;
				++ stretched_cycles_since_card_update_;
			}

			/*
				There are five distinct zones of memory on an Apple II:

				0000 to 0200	:	the zero and stack pages, which can be paged independently on a IIe
				0200 to c000	:	the main block of RAM, which can be paged on a IIe
				c000 to d000	:	the IO area, including card ROMs
				d000 to e000	:	the low ROM area, which can contain indepdently-paged RAM with a language card
				e000 onward		:	the rest of ROM, also potentially replaced with RAM by a language card
			*/
			uint16_t accessed_address = address;
			MemoryBlock *block = nullptr;
			if(address < 0x200) block = &memory_blocks_[0];
			else if(address < 0xc000) {
				if(address < 0x6000 && !isReadOperation(operation)) update_video();
				block = &memory_blocks_[1];
				accessed_address -= 0x200;
			}
			else if(address < 0xd000) block = nullptr;
			else if(address < 0xe000) {block = &memory_blocks_[2]; accessed_address -= 0xd000; }
			else { block = &memory_blocks_[3]; accessed_address -= 0xe000; }

			bool has_updated_cards = false;
			if(block) {
				if(isReadOperation(operation)) *value = block->read_pointer[accessed_address];
				else if(block->write_pointer) block->write_pointer[accessed_address] = *value;

				if(should_load_quickly_) {
					// Check for a prima facie entry into RWTS.
					if(operation == CPU::MOS6502::BusOperation::ReadOpcode && address == 0xb7b5) {
						// Grab the IO control block address for inspection.
						uint16_t io_control_block_address =
							static_cast<uint16_t>(
								(m6502_.get_value_of_register(CPU::MOS6502::Register::A) << 8) |
								m6502_.get_value_of_register(CPU::MOS6502::Register::Y)
							);

						// Verify that this is table type one, for execution on card six,
						// against drive 1 or 2, and that the command is either a seek or a sector read.
						if(
							ram_[io_control_block_address+0x00] == 0x01 &&
							ram_[io_control_block_address+0x01] == 0x60 &&
							ram_[io_control_block_address+0x02] > 0 && ram_[io_control_block_address+0x02] < 3 &&
							ram_[io_control_block_address+0x0c] < 2
						) {
							const uint8_t iob_track = ram_[io_control_block_address+4];
							const uint8_t iob_sector = ram_[io_control_block_address+5];
							const uint8_t iob_drive = ram_[io_control_block_address+2] - 1;

							// Get the track identified and store the new head position.
							auto track = diskii_card()->get_drive(iob_drive).step_to(Storage::Disk::HeadPosition(iob_track));

							// DOS 3.3 keeps the current track (unspecified drive) in 0x478; the current track for drive 1 and drive 2
							// is also kept in that Disk II card's screen hole.
							ram_[0x478] = iob_track;
							if(ram_[io_control_block_address+0x02] == 1) {
								ram_[0x47e] = iob_track;
							} else {
								ram_[0x4fe] = iob_track;
							}

							// Check whether this is a read, not merely a seek.
							if(ram_[io_control_block_address+0x0c] == 1) {
								// Apple the DOS 3.3 formula to map the requested logical sector to a physical sector.
								const int physical_sector = (iob_sector == 15) ? 15 : ((iob_sector * 13) % 15);

								// Parse the entire track. TODO: cache these.
								auto sector_map = Storage::Encodings::AppleGCR::sectors_from_segment(
									Storage::Disk::track_serialisation(*track, Storage::Time(1, 50000)));

								bool found_sector = false;
								for(const auto &pair: sector_map) {
									if(pair.second.address.sector == physical_sector) {
										found_sector = true;

										// Copy the sector contents to their destination.
										uint16_t target = static_cast<uint16_t>(
											ram_[io_control_block_address+8] |
											(ram_[io_control_block_address+9] << 8)
										);

										for(size_t c = 0; c < 256; ++c) {
											ram_[target] = pair.second.data[c];
											++target;
										}

										// Set no error encountered.
										ram_[io_control_block_address + 0xd] = 0;
										break;
									}
								}

								if(found_sector) {
									// Set no error in the flags register too, and RTS.
									m6502_.set_value_of_register(CPU::MOS6502::Register::Flags, m6502_.get_value_of_register(CPU::MOS6502::Register::Flags) & ~1);
									*value = 0x60;
								}
							} else {
								// No error encountered; RTS.
								m6502_.set_value_of_register(CPU::MOS6502::Register::Flags, m6502_.get_value_of_register(CPU::MOS6502::Register::Flags) & ~1);
								*value = 0x60;
							}
						}
					}
				}
			} else {
				// Assume a vapour read unless it turns out otherwise; this is a little
				// wasteful but works for now.
				//
				// Longer version: like many other machines, when the Apple II reads from
				// an address at which no hardware loads the data bus, through a process of
				// practical analogue effects it'll end up receiving whatever was last on
				// the bus. Which will always be whatever the video circuit fetched because
				// that fetches in between every instruction.
				//
				// So this code assumes that'll happen unless it later determines that it
				// doesn't. The call into the video isn't free because it's a just-in-time
				// actor, but this will actually be the result most of the time so it's not
				// too terrible.
				if(isReadOperation(operation) && address != 0xc000) {
					*value = video_->get_last_read_value(cycles_since_video_update_);
				}

				switch(address) {
					default:
						if(isReadOperation(operation)) {
							// Read-only switches.
							switch(address) {
								default: break;

								case 0xc000:
									if(string_serialiser_) {
										*value = string_serialiser_->head() | 0x80;
									} else {
										*value = keyboard_input_;
									}
								break;

								case 0xc061:	// Switch input 0.
									*value &= 0x7f;
									if(static_cast<Joystick *>(joysticks_[0].get())->buttons[0] || static_cast<Joystick *>(joysticks_[1].get())->buttons[2])
										*value |= 0x80;
								break;
								case 0xc062:	// Switch input 1.
									*value &= 0x7f;
									if(static_cast<Joystick *>(joysticks_[0].get())->buttons[1] || static_cast<Joystick *>(joysticks_[1].get())->buttons[1])
										*value |= 0x80;
								break;
								case 0xc063:	// Switch input 2.
									*value &= 0x7f;
									if(static_cast<Joystick *>(joysticks_[0].get())->buttons[2] || static_cast<Joystick *>(joysticks_[1].get())->buttons[0])
										*value |= 0x80;
								break;

								case 0xc064:	// Analogue input 0.
								case 0xc065:	// Analogue input 1.
								case 0xc066:	// Analogue input 2.
								case 0xc067: {	// Analogue input 3.
									const size_t input = address - 0xc064;
									*value &= 0x7f;
									if(analogue_channel_is_discharged(input)) {
										*value |= 0x80;
									}
								} break;
							}
						} else {
							// Write-only switches.
						}
					break;

					case 0xc070: {	// Permit analogue inputs that are currently discharged to begin a charge cycle.
									// Ensure those that were still charging retain that state.
						for(size_t c = 0; c < 4; ++c) {
							if(analogue_channel_is_discharged(c)) {
								analogue_biases_[c] = 0.0f;
							} else {
								analogue_biases_[c] += analogue_charge_;
							}
						}
						analogue_charge_ = 0.0f;
					} break;

					/* Read-write switches. */
					case 0xc050:	update_video();		video_->set_graphics_mode();	break;
					case 0xc051:	update_video();		video_->set_text_mode();		break;
					case 0xc052:	update_video();		video_->set_mixed_mode(false);	break;
					case 0xc053:	update_video();		video_->set_mixed_mode(true);	break;
					case 0xc054:	update_video();		video_->set_video_page(0);		break;
					case 0xc055:	update_video();		video_->set_video_page(1);		break;
					case 0xc056:	update_video();		video_->set_low_resolution();	break;
					case 0xc057:	update_video();		video_->set_high_resolution();	break;

					case 0xc010:
						keyboard_input_ &= 0x7f;
						if(string_serialiser_) {
							if(!string_serialiser_->advance())
								string_serialiser_.reset();
						}
					break;

					case 0xc030:
						update_audio();
						audio_toggle_.set_output(!audio_toggle_.get_output());
					break;

					case 0xc080: case 0xc084: case 0xc088: case 0xc08c:
					case 0xc081: case 0xc085: case 0xc089: case 0xc08d:
					case 0xc082: case 0xc086: case 0xc08a: case 0xc08e:
					case 0xc083: case 0xc087: case 0xc08b: case 0xc08f:
						// Quotes below taken from Understanding the Apple II, p. 5-28 and 5-29.

						// "A3 controls the 4K bank selection"
						language_card_.bank1 = (address&8);

						// "Access to $C080, $C083, $C084, $0087, $C088, $C08B, $C08C, or $C08F sets the READ ENABLE flip-flop"
						// (other accesses reset it)
						language_card_.read = !(((address&2) >> 1) ^ (address&1));

						// "The WRITE ENABLE' flip-flop is reset by an odd read access to the $C08X range when the PRE-WRITE flip-flop is set."
						if(language_card_.pre_write && isReadOperation(operation) && (address&1)) language_card_.write = false;

						// "[The WRITE ENABLE' flip-flop] is set by an even access in the $C08X range."
						if(!(address&1)) language_card_.write = true;

						// ("Any other type of access causes the WRITE ENABLE' flip-flop to hold its current state.")

						// "The PRE-WRITE flip-flop is set by an odd read access in the $C08X range. It is reset by an even access or a write access."
						language_card_.pre_write = isReadOperation(operation) ? (address&1) : false;

						set_language_card_paging();
					break;
				}

				/*
					Communication with cards follows.
				*/

				if(address >= 0xc090 && address < 0xc800) {
					// If this is a card access, figure out which card is at play before determining
					// the totality of who needs messaging.
					size_t card_number = 0;
					AppleII::Card::Select select = AppleII::Card::None;

					if(address >= 0xc100) {
						/*
							Decode the area conventionally used by cards for ROMs:
								0xCn00 to 0xCnff: card n.
						*/
						card_number = (address - 0xc100) >> 8;
						select = AppleII::Card::Device;
					} else {
						/*
							Decode the area conventionally used by cards for registers:
								C0n0 to C0nF: card n - 8.
						*/
						card_number = (address - 0xc090) >> 4;
						select = AppleII::Card::IO;
					}

					// If the selected card is a just-in-time card, update the just-in-time cards,
					// and then message it specifically.
					const bool is_read = isReadOperation(operation);
					AppleII::Card *const target = cards_[card_number].get();
					if(target && !is_every_cycle_card(target)) {
						update_just_in_time_cards();
						target->perform_bus_operation(select, is_read, address, value);
					}

					// Update all the every-cycle cards regardless, but send them a ::None select if they're
					// not the one actually selected.
					for(const auto &card: every_cycle_cards_) {
						card->run_for(Cycles(1), is_stretched_cycle);
						card->perform_bus_operation(
							(card == target) ? select : AppleII::Card::None,
							is_read, address, value);
					}
					has_updated_cards = true;
				}
			}

			if(!has_updated_cards && !every_cycle_cards_.empty()) {
				// Update all every-cycle cards and give them the cycle.
				const bool is_read = isReadOperation(operation);
				for(const auto &card: every_cycle_cards_) {
					card->run_for(Cycles(1), is_stretched_cycle);
					card->perform_bus_operation(AppleII::Card::None, is_read, address, value);
				}
			}

			// Update analogue charge level.
			analogue_charge_ = std::min(analogue_charge_ + 1.0f / 2820.0f, 1.1f);

			return Cycles(1);
		}

		void flush() {
			update_video();
			update_audio();
			update_just_in_time_cards();
			audio_queue_.perform();
		}

		void run_for(const Cycles cycles) override {
			m6502_.run_for(cycles);
		}

		void set_key_pressed(Key key, char value, bool is_pressed) override {
			if(key == Key::F12) {
				m6502_.set_reset_line(is_pressed);
				return;
			}

			if(is_pressed) {
				// If no ASCII value is supplied, look for a few special cases.
				if(!value) {
					switch(key) {
						case Key::Left:		value = 8;	break;
						case Key::Right:	value = 21;	break;
						case Key::Down:		value = 10;	break;
						default: break;
					}
				}

				keyboard_input_ = static_cast<uint8_t>(toupper(value) | 0x80);
			}
		}

		Inputs::Keyboard &get_keyboard() override {
			return *this;
		}

		void type_string(const std::string &string) override {
			string_serialiser_.reset(new Utility::StringSerialiser(string, true));
		}

		// MARK: MediaTarget
		bool insert_media(const Analyser::Static::Media &media) override {
			if(!media.disks.empty()) {
				auto diskii = diskii_card();
				if(diskii) diskii->set_disk(media.disks[0], 0);
			}
			return true;
		}

		// MARK: Activity::Source
		void set_activity_observer(Activity::Observer *observer) override {
			for(const auto &card: cards_) {
				if(card) card->set_activity_observer(observer);
			}
		}

		// MARK: Options
		std::vector<std::unique_ptr<Configurable::Option>> get_options() override {
			return AppleII::get_options();
		}

		void set_selections(const Configurable::SelectionSet &selections_by_option) override {
			bool quickload;
			if(Configurable::get_quick_load_tape(selections_by_option, quickload)) {
				should_load_quickly_ = quickload;
			}
		}

		Configurable::SelectionSet get_accurate_selections() override {
			Configurable::SelectionSet selection_set;
			Configurable::append_quick_load_tape_selection(selection_set, false);
			return selection_set;
		}

		Configurable::SelectionSet get_user_friendly_selections() override {
			Configurable::SelectionSet selection_set;
			Configurable::append_quick_load_tape_selection(selection_set, true);
			return selection_set;
		}

		// MARK: JoystickMachine
		std::vector<std::unique_ptr<Inputs::Joystick>> &get_joysticks() override {
			return joysticks_;
		}
};

}

using namespace AppleII;

Machine *Machine::AppleII(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	using Target = Analyser::Static::AppleII::Target;
	const Target *const appleii_target = dynamic_cast<const Target *>(target);
	return new ConcreteMachine(*appleii_target, rom_fetcher);
}

Machine::~Machine() {}

