//
//  AppleII.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/04/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#include "AppleII.hpp"

#include "../../Activity/Source.hpp"
#include "../ConfigurationTarget.hpp"
#include "../CRTMachine.hpp"
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

#include <algorithm>
#include <array>
#include <memory>

namespace {

class ConcreteMachine:
	public CRTMachine::Machine,
	public ConfigurationTarget::Machine,
	public KeyboardMachine::Machine,
	public CPU::MOS6502::BusHandler,
	public Inputs::Keyboard,
	public AppleII::Machine,
	public Activity::Source,
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
		std::vector<uint8_t> apple2_rom_, apple2plus_rom_, rom_;
		std::vector<uint8_t> character_rom_;
		uint8_t keyboard_input_ = 0x00;

		Concurrency::DeferringAsyncTaskQueue audio_queue_;
		Audio::Toggle audio_toggle_;
		Outputs::Speaker::LowpassSpeaker<Audio::Toggle> speaker_;
		Cycles cycles_since_audio_update_;

		ROMMachine::ROMFetcher rom_fetcher_;

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

	public:
		ConcreteMachine():
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

		Cycles perform_bus_operation(CPU::MOS6502::BusOperation operation, uint16_t address, uint8_t *value) {
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
			MemoryBlock *block = nullptr;
			if(address < 0x200) block = &memory_blocks_[0];
			else if(address < 0xc000) {
				if(address < 0x6000 && !isReadOperation(operation)) update_video();
				block = &memory_blocks_[1];
				address -= 0x200;
			}
			else if(address < 0xd000) block = nullptr;
			else if(address < 0xe000) {block = &memory_blocks_[2]; address -= 0xd000; }
			else { block = &memory_blocks_[3]; address -= 0xe000; }

			bool has_updated_cards = false;
			if(block) {
				if(isReadOperation(operation)) *value = block->read_pointer[address];
				else if(block->write_pointer) block->write_pointer[address] = *value;
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
							}
						} else {
							// Write-only switches.
						}
					break;

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

			return Cycles(1);
		}

		void flush() {
			update_video();
			update_audio();
			update_just_in_time_cards();
			audio_queue_.perform();
		}

		bool set_rom_fetcher(const ROMMachine::ROMFetcher &roms_with_names) override {
			auto roms = roms_with_names(
				"AppleII",
				{
					"apple2o.rom",
					"apple2.rom",
					"apple2-character.rom"
				});

			if(!roms[0] || !roms[1] || !roms[2]) return false;

			apple2_rom_ = std::move(*roms[0]);
			apple2plus_rom_ = std::move(*roms[1]);

			character_rom_ = std::move(*roms[2]);

			rom_fetcher_ = roms_with_names;

			return true;
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

		// MARK: ConfigurationTarget
		void configure_as_target(const Analyser::Static::Target *target) override {
			using Target = Analyser::Static::AppleII::Target;
			auto *const apple_target = dynamic_cast<const Target *>(target);

			if(apple_target->disk_controller != Target::DiskController::None) {
				// Apple recommended slot 6 for the (first) Disk II.
				install_card(6, new AppleII::DiskIICard(rom_fetcher_, apple_target->disk_controller == Target::DiskController::SixteenSector));
			}

			rom_ = (apple_target->model == Target::Model::II) ? apple2_rom_ : apple2plus_rom_;
			if(rom_.size() > 12*1024) {
				rom_.erase(rom_.begin(), rom_.begin() + static_cast<off_t>(rom_.size()) - 12*1024);
			}

			// Set up the default memory blocks.
			memory_blocks_[0].read_pointer = memory_blocks_[0].write_pointer = ram_;
			memory_blocks_[1].read_pointer = memory_blocks_[1].write_pointer = &ram_[0x200];
			set_language_card_paging();

			insert_media(apple_target->media);
		}

		bool insert_media(const Analyser::Static::Media &media) override {
			if(!media.disks.empty() && cards_[5]) {
				dynamic_cast<AppleII::DiskIICard *>(cards_[5].get())->set_disk(media.disks[0], 0);
			}
			return true;
		}

		// MARK: Activity::Source
		void set_activity_observer(Activity::Observer *observer) override {
			for(const auto &card: cards_) {
				if(card) card->set_activity_observer(observer);
			}
		}
};

}

using namespace AppleII;

Machine *Machine::AppleII() {
	return new ConcreteMachine;
}

Machine::~Machine() {}
