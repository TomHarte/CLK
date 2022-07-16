//
//  AppleII.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/04/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#include "AppleII.hpp"

#include "../../../Activity/Source.hpp"
#include "../../MachineTypes.hpp"
#include "../../Utility/MemoryFuzzer.hpp"
#include "../../Utility/StringSerialiser.hpp"

#include "../../../Processors/6502/6502.hpp"
#include "../../../Components/AudioToggle/AudioToggle.hpp"

#include "../../../Outputs/Speaker/Implementation/LowpassSpeaker.hpp"
#include "../../../Outputs/Log.hpp"

#include "AuxiliaryMemorySwitches.hpp"
#include "Card.hpp"
#include "DiskIICard.hpp"
#include "Joystick.hpp"
#include "LanguageCardSwitches.hpp"
#include "Video.hpp"

#include "../../../Analyser/Static/AppleII/Target.hpp"
#include "../../../ClockReceiver/ForceInline.hpp"
#include "../../../Configurable/StandardOptions.hpp"

#include <algorithm>
#include <array>
#include <memory>

namespace Apple {
namespace II {

#define is_iie() ((model == Analyser::Static::AppleII::Target::Model::IIe) || (model == Analyser::Static::AppleII::Target::Model::EnhancedIIe))

template <Analyser::Static::AppleII::Target::Model model> class ConcreteMachine:
	public Apple::II::Machine,
	public MachineTypes::TimedMachine,
	public MachineTypes::ScanProducer,
	public MachineTypes::AudioProducer,
	public MachineTypes::MediaTarget,
	public MachineTypes::MappedKeyboardMachine,
	public MachineTypes::JoystickMachine,
	public CPU::MOS6502::BusHandler,
	public Configurable::Device,
	public Activity::Source,
	public Apple::II::Card::Delegate {
	private:
		struct VideoBusHandler : public Apple::II::Video::BusHandler {
			public:
				VideoBusHandler(uint8_t *ram, uint8_t *aux_ram) : ram_(ram), aux_ram_(aux_ram) {}

				void perform_read(uint16_t address, size_t count, uint8_t *base_target, uint8_t *auxiliary_target) {
					memcpy(base_target, &ram_[address], count);
					memcpy(auxiliary_target, &aux_ram_[address], count);
				}

			private:
				uint8_t *ram_, *aux_ram_;
		};

		using Processor = CPU::MOS6502::Processor<
			(model == Analyser::Static::AppleII::Target::Model::EnhancedIIe) ? CPU::MOS6502::Personality::PSynertek65C02 : CPU::MOS6502::Personality::P6502,
			ConcreteMachine,
			false>;
		Processor m6502_;
		VideoBusHandler video_bus_handler_;
		Apple::II::Video::Video<VideoBusHandler, is_iie()> video_;
		int cycles_into_current_line_ = 0;
		Cycles cycles_since_video_update_;

		void update_video() {
			video_.run_for(cycles_since_video_update_.flush<Cycles>());
		}
		static constexpr int audio_divider = 8;
		void update_audio() {
			speaker_.run_for(audio_queue_, cycles_since_audio_update_.divide(Cycles(audio_divider)));
		}
		void update_just_in_time_cards() {
			if(cycles_since_card_update_ > Cycles(0)) {
				for(const auto &card : just_in_time_cards_) {
					card->run_for(cycles_since_card_update_, stretched_cycles_since_card_update_);
				}
			}
			cycles_since_card_update_ = 0;
			stretched_cycles_since_card_update_ = 0;
		}

		uint8_t ram_[65536], aux_ram_[65536];
		std::vector<uint8_t> rom_;

		Concurrency::AsyncTaskQueue<false> audio_queue_;
		Audio::Toggle audio_toggle_;
		Outputs::Speaker::PullLowpass<Audio::Toggle> speaker_;
		Cycles cycles_since_audio_update_;

		// MARK: - Cards
		std::array<std::unique_ptr<Apple::II::Card>, 7> cards_;
		Cycles cycles_since_card_update_;
		std::vector<Apple::II::Card *> every_cycle_cards_;
		std::vector<Apple::II::Card *> just_in_time_cards_;

		int stretched_cycles_since_card_update_ = 0;

		void install_card(std::size_t slot, Apple::II::Card *card) {
			assert(slot >= 1 && slot < 8);
			cards_[slot - 1].reset(card);
			card->set_delegate(this);
			pick_card_messaging_group(card);
		}

		bool is_every_cycle_card(const Apple::II::Card *card) {
			return !card->get_select_constraints();
		}

		bool card_lists_are_dirty_ = true;
		bool card_became_just_in_time_ = false;
		void pick_card_messaging_group(Apple::II::Card *card) {
			// Simplify to a card being either just-in-time or realtime.
			// Don't worry about exactly what it's watching,
			const bool is_every_cycle = is_every_cycle_card(card);
			std::vector<Apple::II::Card *> &intended = is_every_cycle ? every_cycle_cards_ : just_in_time_cards_;

			// If the card is already in the proper group, stop.
			if(std::find(intended.begin(), intended.end(), card) != intended.end()) return;

			// Otherwise, mark the sets as dirty. It isn't safe to transition the card here,
			// as the main loop may be part way through iterating the two lists.
			card_lists_are_dirty_ = true;
			card_became_just_in_time_ |= !is_every_cycle;
		}

		void card_did_change_select_constraints(Apple::II::Card *card) final {
			pick_card_messaging_group(card);
		}

		Apple::II::DiskIICard *diskii_card() {
			return dynamic_cast<Apple::II::DiskIICard *>(cards_[5].get());
		}

		// MARK: - Memory Map.

		/*
			The Apple II's paging mechanisms are byzantine to say the least. Painful is
			another appropriate adjective.

			On a II and II+ there are five distinct zones of memory:

			0000 to c000	:	the main block of RAM
			c000 to d000	:	the IO area, including card ROMs
			d000 to e000	:	the low ROM area, which can alternatively contain either one of two 4kb blocks of RAM with a language card
			e000 onward		:	the rest of ROM, also potentially replaced with RAM by a language card

			On a IIe with auxiliary memory the following orthogonal changes also need to be factored in:

			0000 to 0200	:	can be paged independently of the rest of RAM, other than part of the language card area which pages with it
			0400 to 0800	:	the text screen, can be configured to write to auxiliary RAM
			2000 to 4000	:	the graphics screen, which can be configured to write to auxiliary RAM
			c100 to d000	:	can be used to page an additional 3.75kb of ROM, replacing the IO area
			c300 to c400	:	can contain the same 256-byte segment of the ROM as if the whole IO area were switched, but while leaving cards visible in the rest
			c800 to d000	:	can contain ROM separately from the region below c800

			If dealt with as individual blocks in the inner loop, that would therefore imply mapping
			an address to one of 13 potential pageable zones. So I've gone reductive and surrendered
			to paging every 6502 page of memory independently. It makes the paging events more expensive,
			but hopefully more clear.
		*/
		const uint8_t *read_pages_[256];	// each is a pointer to the 256-block of memory the CPU should read when accessing that page of memory
		uint8_t *write_pages_[256];	// as per read_pages_, but this is where the CPU should write. If a pointer is nullptr, don't write.
		void page(int start, int end, uint8_t *read, uint8_t *write) {
			for(int position = start; position < end; ++position) {
				read_pages_[position] = read;
				if(read) read += 256;

				write_pages_[position] = write;
				if(write) write += 256;
			}
		}

		// MARK: - The language card, auxiliary memory, and IIe-specific improvements.
		LanguageCardSwitches<ConcreteMachine> language_card_;
		AuxiliaryMemorySwitches<ConcreteMachine> auxiliary_switches_;
		friend LanguageCardSwitches<ConcreteMachine>;
		friend AuxiliaryMemorySwitches<ConcreteMachine>;

		template <int type> void set_paging() {
			if constexpr (bool(type & PagingType::ZeroPage)) {
				if(auxiliary_switches_.zero_state()) {
					write_pages_[0] = aux_ram_;
				} else {
					write_pages_[0] = ram_;
				}
				write_pages_[1] = write_pages_[0] + 256;
				read_pages_[0] = write_pages_[0];
				read_pages_[1] = write_pages_[1];
			}

			if constexpr (bool(type & (PagingType::LanguageCard | PagingType::ZeroPage))) {
				const auto language_state = language_card_.state();
				const auto zero_state = auxiliary_switches_.zero_state();

				uint8_t *const ram = zero_state ? aux_ram_ : ram_;
				uint8_t *const rom = is_iie() ? &rom_[3840] : rom_.data();

				// Which way the region here is mapped to be banks 1 and 2 is
				// arbitrary.
				page(0xd0, 0xe0,
					language_state.read ? &ram[language_state.bank2 ? 0xd000 : 0xc000] : rom,
					language_state.write ? nullptr : &ram[language_state.bank2 ? 0xd000 : 0xc000]);

				page(0xe0, 0x100,
					language_state.read ? &ram[0xe000] : &rom[0x1000],
					language_state.write ? nullptr : &ram[0xe000]);
			}

			if constexpr (bool(type & PagingType::CardArea)) {
				const auto state = auxiliary_switches_.card_state();

				page(0xc1, 0xc4, state.region_C1_C3 ? &rom_[0xc100 - 0xc100] : nullptr, nullptr);
				read_pages_[0xc3] = state.region_C3 ? &rom_[0xc300 - 0xc100] : nullptr;
				page(0xc4, 0xc8, state.region_C4_C8 ? &rom_[0xc400 - 0xc100]  : nullptr, nullptr);
				page(0xc8, 0xd0, state.region_C8_D0 ? &rom_[0xc800 - 0xc100]  : nullptr, nullptr);
			}

			if constexpr (bool(type & PagingType::Main)) {
				const auto state = auxiliary_switches_.main_state();

				page(0x02, 0x04,
					state.base.read ? &aux_ram_[0x0200] : &ram_[0x0200],
					state.base.write ? &aux_ram_[0x0200] : &ram_[0x0200]);
				page(0x08, 0x20,
					state.base.read ? &aux_ram_[0x0800] : &ram_[0x0800],
					state.base.write ? &aux_ram_[0x0800] : &ram_[0x0800]);
				page(0x40, 0xc0,
					state.base.read ? &aux_ram_[0x4000] : &ram_[0x4000],
					state.base.write ? &aux_ram_[0x4000] : &ram_[0x4000]);

				page(0x04, 0x08,
					state.region_04_08.read ? &aux_ram_[0x0400] : &ram_[0x0400],
					state.region_04_08.write ? &aux_ram_[0x0400] : &ram_[0x0400]);

				page(0x20, 0x40,
					state.region_20_40.read ? &aux_ram_[0x2000] : &ram_[0x2000],
					state.region_20_40.write ? &aux_ram_[0x2000] : &ram_[0x2000]);
			}
		}

		// MARK: - Keyboard and typing.

		struct Keyboard: public Inputs::Keyboard {
			Keyboard(Processor *m6502) : m6502_(m6502) {}

			void reset_all_keys() final {
				open_apple_is_pressed = closed_apple_is_pressed = control_is_pressed = shift_is_pressed = key_is_down = false;
			}

			bool set_key_pressed(Key key, char value, bool is_pressed) final {
				// If no ASCII value is supplied, look for a few special cases.
				switch(key) {
					case Key::Left:			value = 0x08;	break;
					case Key::Right:		value = 0x15;	break;
					case Key::Down:			value = 0x0a;	break;
					case Key::Up:			value = 0x0b;	break;
					case Key::Backspace:
						if(is_iie()) {
							value = 0x7f;
							break;
						} else {
							return false;
						}
					case Key::Enter:		value = 0x0d;	break;
					case Key::Tab:
						if (is_iie()) {
							value = '\t';
							break;
						} else {
							return false;
						}
					case Key::Escape:		value = 0x1b;	break;
					case Key::Space:		value = 0x20;	break;

					case Key::LeftOption:
					case Key::RightMeta:
						if (is_iie()) {
							open_apple_is_pressed = is_pressed;
							return true;
						} else {
							return false;
						}

					case Key::RightOption:
					case Key::LeftMeta:
						if (is_iie()) {
							closed_apple_is_pressed = is_pressed;
							return true;
						} else {
							return false;
						}

					case Key::LeftControl:
						control_is_pressed = is_pressed;
					return true;

					case Key::LeftShift:
					case Key::RightShift:
						shift_is_pressed = is_pressed;
					return true;

					case Key::F1:	case Key::F2:	case Key::F3:	case Key::F4:
					case Key::F5:	case Key::F6:	case Key::F7:	case Key::F8:
					case Key::F9:	case Key::F10:	case Key::F11:	case Key::F12:
					case Key::PrintScreen:
					case Key::ScrollLock:
					case Key::Pause:
					case Key::Insert:
					case Key::Home:
					case Key::PageUp:
					case Key::PageDown:
					case Key::End:
						// Accept a bunch non-symbolic other keys, as
						// reset, in the hope that the user can find
						// at least one usable key.
						m6502_->set_reset_line(is_pressed);
					return true;

					default:
						if(!value) {
							return false;
						}

						// Prior to the IIe, the keyboard could produce uppercase only.
						if(!is_iie()) value = char(toupper(value));

						if(control_is_pressed && isalpha(value)) value &= 0xbf;

						// TODO: properly map IIe keys
						if(!is_iie() && shift_is_pressed) {
							switch(value) {
							case 0x27: value = 0x22; break; // ' -> "
							case 0x2c: value = 0x3c; break; // , -> <
							case 0x2e: value = 0x3e; break; // . -> >
							case 0x2f: value = 0x3f; break; // / -> ?
							case 0x30: value = 0x29; break; // 0 -> )
							case 0x31: value = 0x21; break; // 1 -> !
							case 0x32: value = 0x40; break; // 2 -> @
							case 0x33: value = 0x23; break; // 3 -> #
							case 0x34: value = 0x24; break; // 4 -> $
							case 0x35: value = 0x25; break; // 5 -> %
							case 0x36: value = 0x5e; break; // 6 -> ^
							case 0x37: value = 0x26; break; // 7 -> &
							case 0x38: value = 0x2a; break; // 8 -> *
							case 0x39: value = 0x28; break; // 9 -> (
							case 0x3b: value = 0x3a; break; // ; -> :
							case 0x3d: value = 0x2b; break; // = -> +
						}
					}
					break;
				}

				if(is_pressed) {
					keyboard_input = uint8_t(value | 0x80);
					key_is_down = true;
				} else {
					if((keyboard_input & 0x3f) == value) {
						key_is_down = false;
					}
				}

				return true;
			}

			uint8_t get_keyboard_input() {
				if(string_serialiser) {
					return string_serialiser->head() | 0x80;
				} else {
					return keyboard_input;
				}
			}

			bool shift_is_pressed = false;
			bool control_is_pressed = false;
			// The IIe has three keys that are wired directly to the same input as the joystick buttons.
			bool open_apple_is_pressed = false;
			bool closed_apple_is_pressed = false;
			uint8_t keyboard_input = 0x00;
			bool key_is_down = false;
			std::unique_ptr<Utility::StringSerialiser> string_serialiser;

			private:
			Processor *const m6502_;
		};
		Keyboard keyboard_;

		// MARK: - Joysticks.
		JoystickPair joysticks_;

	public:
		ConcreteMachine(const Analyser::Static::AppleII::Target &target, const ROMMachine::ROMFetcher &rom_fetcher):
			m6502_(*this),
			video_bus_handler_(ram_, aux_ram_),
			video_(video_bus_handler_),
			audio_toggle_(audio_queue_),
			speaker_(audio_toggle_),
			language_card_(*this),
			auxiliary_switches_(*this),
			keyboard_(&m6502_) {
			// The system's master clock rate.
			constexpr float master_clock = 14318180.0;

			// This is where things get slightly convoluted: establish the machine as having a clock rate
			// equal to the number of cycles of work the 6502 will actually achieve. Which is less than
			// the master clock rate divided by 14 because every 65th cycle is extended by one seventh.
			set_clock_rate((master_clock / 14.0) * 65.0 / (65.0 + 1.0 / 7.0));

			// The speaker, however, should think it is clocked at half the master clock, per a general
			// decision to sample it at seven times the CPU clock (plus stretches).
			speaker_.set_input_rate(float(master_clock / (2.0 * float(audio_divider))));

			// Apply a 6Khz low-pass filter. This was picked by ear and by an attempt to understand the
			// Apple II schematic but, well, I don't claim much insight on the latter. This is definitely
			// something to review in the future.
			speaker_.set_high_frequency_cutoff(6000);

			// Also, start with randomised memory contents.
			Memory::Fuzz(ram_, sizeof(ram_));
			Memory::Fuzz(aux_ram_, sizeof(aux_ram_));

			// Pick the required ROMs.
			using Target = Analyser::Static::AppleII::Target;
			ROM::Name character, system;

			switch(target.model) {
				default:
					character = ROM::Name::AppleIICharacter;
					system = ROM::Name::AppleIIOriginal;
				break;
				case Target::Model::IIplus:
					character = ROM::Name::AppleIICharacter;
					system = ROM::Name::AppleIIPlus;
				break;
				case Target::Model::IIe:
					character = ROM::Name::AppleIIeCharacter;
					system = ROM::Name::AppleIIe;
				break;
				case Target::Model::EnhancedIIe:
					character = ROM::Name::AppleIIEnhancedECharacter;
					system = ROM::Name::AppleIIEnhancedE;
				break;
			}

			ROM::Request request = ROM::Request(character) && ROM::Request(system);

			// Add the necessary Disk II requests if appropriate.
			const bool has_disk_controller = target.disk_controller != Target::DiskController::None;
			const bool is_sixteen_sector = target.disk_controller == Target::DiskController::SixteenSector;
			if(has_disk_controller) {
				// Apple recommended slot 6 for the (first) Disk II.
				request = request && DiskIICard::rom_request(is_sixteen_sector);
			}

			// Request, validate and install ROMs.
			auto roms = rom_fetcher(request);
			if(!request.validate(roms)) {
				throw ROMMachine::Error::MissingROMs;
			}

			if(has_disk_controller) {
				install_card(6, new Apple::II::DiskIICard(roms, is_sixteen_sector));
			}

			rom_ = std::move(roms.find(system)->second);
			// The IIe and Enhanced IIe ROMs often distributed are oversized; trim if necessary.
			if(system == ROM::Name::AppleIIe || system == ROM::Name::AppleIIEnhancedE) {
				if(rom_.size() > 16128) {
					rom_.erase(rom_.begin(), rom_.end() - 16128);
				}
			}
			video_.set_character_rom(roms.find(character)->second);

			// Set up the default memory blocks. On a II or II+ these values will never change.
			// On a IIe they'll be affected by selection of auxiliary RAM.
			set_paging<PagingType::Main | PagingType::ZeroPage>();

			// Set the whole card area to initially backed by nothing.
			page(0xc0, 0xd0, nullptr, nullptr);

			insert_media(target.media);
		}

		~ConcreteMachine() {
			audio_queue_.flush();
		}

		void set_scan_target(Outputs::Display::ScanTarget *scan_target) final {
			video_.set_scan_target(scan_target);
		}

		Outputs::Display::ScanStatus get_scaled_scan_status() const final {
			return video_.get_scaled_scan_status();
		}

		/// Sets the type of display.
		void set_display_type(Outputs::Display::DisplayType display_type) final {
			video_.set_display_type(display_type);
		}

		Outputs::Display::DisplayType get_display_type() const final {
			return video_.get_display_type();
		}

		Outputs::Speaker::Speaker *get_speaker() final {
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

			bool has_updated_cards = false;
			if(read_pages_[address >> 8]) {
				if(isReadOperation(operation)) *value = read_pages_[address >> 8][address & 0xff];
				else {
					if(address >= 0x200 && address < 0x6000) update_video();
					if(write_pages_[address >> 8]) write_pages_[address >> 8][address & 0xff] = *value;
				}

				if(is_iie()) {
					auxiliary_switches_.access(address, isReadOperation(operation));
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
					*value = video_.get_last_read_value(cycles_since_video_update_);
				}

				switch(address) {
					default:
						if(isReadOperation(operation)) {
							// Read-only switches.
							switch(address) {
								default: break;

								case 0xc000:
									*value = keyboard_.get_keyboard_input();
								break;
								case 0xc001: case 0xc002: case 0xc003: case 0xc004: case 0xc005: case 0xc006: case 0xc007:
								case 0xc008: case 0xc009: case 0xc00a: case 0xc00b: case 0xc00c: case 0xc00d: case 0xc00e: case 0xc00f:
									*value = (*value & 0x80) | (keyboard_.get_keyboard_input() & 0x7f);
								break;

								case 0xc061:	// Switch input 0.
									*value &= 0x7f;
									if(
										joysticks_.button(0) ||
										(is_iie() && keyboard_.open_apple_is_pressed)
									)
										*value |= 0x80;
								break;
								case 0xc062:	// Switch input 1.
									*value &= 0x7f;
									if(
										joysticks_.button(1) ||
										(is_iie() && keyboard_.closed_apple_is_pressed)
									)
										*value |= 0x80;
								break;
								case 0xc063:	// Switch input 2.
									*value &= 0x7f;
									if(joysticks_.button(2))
										*value |= 0x80;
								break;

								case 0xc064:	// Analogue input 0.
								case 0xc065:	// Analogue input 1.
								case 0xc066:	// Analogue input 2.
								case 0xc067: {	// Analogue input 3.
									const size_t input = address - 0xc064;
									*value &= 0x7f;
									if(!joysticks_.analogue_channel_is_discharged(input)) {
										*value |= 0x80;
									}
								} break;

								// The IIe-only state reads follow...
#define IIeSwitchRead(s)	*value = keyboard_.get_keyboard_input(); if(is_iie()) *value = (*value & 0x7f) | (s ? 0x80 : 0x00);
								case 0xc011:	IIeSwitchRead(language_card_.state().bank2);								break;
								case 0xc012:	IIeSwitchRead(language_card_.state().read);									break;
								case 0xc013:	IIeSwitchRead(auxiliary_switches_.switches().read_auxiliary_memory);		break;
								case 0xc014:	IIeSwitchRead(auxiliary_switches_.switches().write_auxiliary_memory);		break;
								case 0xc015:	IIeSwitchRead(auxiliary_switches_.switches().internal_CX_rom);				break;
								case 0xc016:	IIeSwitchRead(auxiliary_switches_.switches().alternative_zero_page);		break;
								case 0xc017:	IIeSwitchRead(auxiliary_switches_.switches().slot_C3_rom);					break;
								case 0xc018:	IIeSwitchRead(video_.get_80_store());										break;
								case 0xc019:	IIeSwitchRead(video_.get_is_vertical_blank(cycles_since_video_update_));	break;
								case 0xc01a:	IIeSwitchRead(video_.get_text());											break;
								case 0xc01b:	IIeSwitchRead(video_.get_mixed());											break;
								case 0xc01c:	IIeSwitchRead(video_.get_page2());											break;
								case 0xc01d:	IIeSwitchRead(video_.get_high_resolution());								break;
								case 0xc01e:	IIeSwitchRead(video_.get_alternative_character_set());						break;
								case 0xc01f:	IIeSwitchRead(video_.get_80_columns());										break;
#undef IIeSwitchRead

								case 0xc07f:
									if(is_iie()) *value = (*value & 0x7f) | (video_.get_annunciator_3() ? 0x80 : 0x00);
								break;
							}
						} else {
							// Write-only switches. All IIe as currently implemented.
							if(is_iie()) {
								auxiliary_switches_.access(address, false);
								switch(address) {
									default: break;

									case 0xc000:
									case 0xc001:
										update_video();
										video_.set_80_store(!!(address&1));
									break;

									case 0xc00c:
									case 0xc00d:
										update_video();
										video_.set_80_columns(!!(address&1));
									break;

									case 0xc00e:
									case 0xc00f:
										update_video();
										video_.set_alternative_character_set(!!(address&1));
									break;
								}
							}
						}
					break;

					case 0xc070: joysticks_.access_c070(); break;

					/* Switches triggered by reading or writing. */
					case 0xc050:
					case 0xc051:
						update_video();
						video_.set_text(!!(address&1));
					break;
					case 0xc052:	update_video();		video_.set_mixed(false);		break;
					case 0xc053:	update_video();		video_.set_mixed(true);			break;
					case 0xc054:
					case 0xc055:
						update_video();
						video_.set_page2(address&1);
						auxiliary_switches_.access(address, isReadOperation(operation));
					break;
					case 0xc056:
					case 0xc057:
						update_video();
						video_.set_high_resolution(address&1);
						auxiliary_switches_.access(address, isReadOperation(operation));
					break;

					case 0xc05e:
					case 0xc05f:
						if(is_iie()) {
							update_video();
							video_.set_annunciator_3(!(address&1));
						}
					break;

					case 0xc010:
						keyboard_.keyboard_input &= 0x7f;
						if(keyboard_.string_serialiser) {
							if(!keyboard_.string_serialiser->advance())
								keyboard_.string_serialiser.reset();
						}

						// On the IIe, reading C010 returns additional key info.
						if(is_iie() && isReadOperation(operation)) {
							*value = (keyboard_.key_is_down ? 0x80 : 0x00) | (keyboard_.keyboard_input & 0x7f);
						}
					break;

					case 0xc030: case 0xc031: case 0xc032: case 0xc033: case 0xc034: case 0xc035: case 0xc036: case 0xc037:
					case 0xc038: case 0xc039: case 0xc03a: case 0xc03b: case 0xc03c: case 0xc03d: case 0xc03e: case 0xc03f:
						update_audio();
						audio_toggle_.set_output(!audio_toggle_.get_output());
					break;

					case 0xc080: case 0xc084: case 0xc088: case 0xc08c:
					case 0xc081: case 0xc085: case 0xc089: case 0xc08d:
					case 0xc082: case 0xc086: case 0xc08a: case 0xc08e:
					case 0xc083: case 0xc087: case 0xc08b: case 0xc08f:
						language_card_.access(address, isReadOperation(operation));
					break;
				}

				/*
					Communication with cards follows.
				*/

				if(!read_pages_[address >> 8] && address >= 0xc090 && address < 0xc800) {
					// If this is a card access, figure out which card is at play before determining
					// the totality of who needs messaging.
					size_t card_number = 0;
					Apple::II::Card::Select select = Apple::II::Card::None;

					if(address >= 0xc100) {
						/*
							Decode the area conventionally used by cards for ROMs:
								0xCn00 to 0xCnff: card n.
						*/
						card_number = (address - 0xc100) >> 8;
						select = Apple::II::Card::Device;
					} else {
						/*
							Decode the area conventionally used by cards for registers:
								C0n0 to C0nF: card n - 8.
						*/
						card_number = (address - 0xc090) >> 4;
						select = Apple::II::Card::IO;
					}

					// If the selected card is a just-in-time card, update the just-in-time cards,
					// and then message it specifically.
					const bool is_read = isReadOperation(operation);
					Apple::II::Card *const target = cards_[size_t(card_number)].get();
					if(target && !is_every_cycle_card(target)) {
						update_just_in_time_cards();
						target->perform_bus_operation(select, is_read, address, value);
					}

					// Update all the every-cycle cards regardless, but send them a ::None select if they're
					// not the one actually selected.
					for(const auto &card: every_cycle_cards_) {
						card->run_for(Cycles(1), is_stretched_cycle);
						card->perform_bus_operation(
							(card == target) ? select : Apple::II::Card::None,
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
					card->perform_bus_operation(Apple::II::Card::None, is_read, address, value);
				}
			}

			// Update the card lists if any mutations are due.
			if(card_lists_are_dirty_) {
				card_lists_are_dirty_ = false;

				// There's only one counter of time since update
				// for just-in-time cards. If something new is
				// transitioning, that needs to be zeroed.
				if(card_became_just_in_time_) {
					card_became_just_in_time_ = false;
					update_just_in_time_cards();
				}

				// Clear the two lists and repopulate.
				every_cycle_cards_.clear();
				just_in_time_cards_.clear();
				for(const auto &card: cards_) {
					if(!card) continue;
					if(is_every_cycle_card(card.get())) {
						every_cycle_cards_.push_back(card.get());
					} else {
						just_in_time_cards_.push_back(card.get());
					}
				}
			}

			// Update analogue charge level.
			joysticks_.update_charge();

			return Cycles(1);
		}

		void flush_output(int outputs) final {
			update_just_in_time_cards();

			if(outputs & Output::Video) {
				update_video();
			}
			if(outputs & Output::Audio) {
				update_audio();
				audio_queue_.perform();
			}
		}

		void run_for(const Cycles cycles) final {
			m6502_.run_for(cycles);
		}

		bool prefers_logical_input() final {
			return is_iie();
		}

		Inputs::Keyboard &get_keyboard() final {
			return keyboard_;
		}

		void type_string(const std::string &string) final {
			keyboard_.string_serialiser = std::make_unique<Utility::StringSerialiser>(string, true);
		}

		bool can_type(char c) const final {
			// Make an effort to type the entire printable ASCII range.
			return c >= 32 && c < 127;
		}

		// MARK:: Configuration options.
		std::unique_ptr<Reflection::Struct> get_options() final {
			auto options = std::make_unique<Options>(Configurable::OptionsType::UserFriendly);
			options->output = get_video_signal_configurable();
			options->use_square_pixels = video_.get_use_square_pixels();
			return options;
		}

		void set_options(const std::unique_ptr<Reflection::Struct> &str) {
			const auto options = dynamic_cast<Options *>(str.get());
			set_video_signal_configurable(options->output);
			video_.set_use_square_pixels(options->use_square_pixels);
		}

		// MARK: MediaTarget
		bool insert_media(const Analyser::Static::Media &media) final {
			if(!media.disks.empty()) {
				auto diskii = diskii_card();
				if(diskii) diskii->set_disk(media.disks[0], 0);
			}
			return true;
		}

		// MARK: Activity::Source
		void set_activity_observer(Activity::Observer *observer) final {
			for(const auto &card: cards_) {
				if(card) card->set_activity_observer(observer);
			}
		}

		// MARK: JoystickMachine
		const std::vector<std::unique_ptr<Inputs::Joystick>> &get_joysticks() final {
			return joysticks_.get_joysticks();
		}
};

}
}

using namespace Apple::II;

Machine *Machine::AppleII(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	using Target = Analyser::Static::AppleII::Target;
	const Target *const appleii_target = dynamic_cast<const Target *>(target);
	switch(appleii_target->model) {
		default: return nullptr;
		case Target::Model::II: return new ConcreteMachine<Target::Model::II>(*appleii_target, rom_fetcher);
		case Target::Model::IIplus: return new ConcreteMachine<Target::Model::IIplus>(*appleii_target, rom_fetcher);
		case Target::Model::IIe: return new ConcreteMachine<Target::Model::IIe>(*appleii_target, rom_fetcher);
		case Target::Model::EnhancedIIe: return new ConcreteMachine<Target::Model::EnhancedIIe>(*appleii_target, rom_fetcher);
	}
}

Machine::~Machine() {}
