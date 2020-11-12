//
//  AppleIIgs.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/10/2020.
//  Copyright 2020 Thomas Harte. All rights reserved.
//

#include "AppleIIgs.hpp"

#include "../../MachineTypes.hpp"
#include "../../../Processors/65816/65816.hpp"

#include "../../../Analyser/Static/AppleIIgs/Target.hpp"
#include "ADB.hpp"
#include "MemoryMap.hpp"
#include "Video.hpp"
#include "Sound.hpp"

#include "../../../Components/8530/z8530.hpp"
#include "../../../Components/AppleClock/AppleClock.hpp"
#include "../../../Components/AudioToggle/AudioToggle.hpp"
#include "../../../Components/DiskII/IWM.hpp"
#include "../../../Components/DiskII/MacintoshDoubleDensityDrive.hpp"

#include "../../../Outputs/Speaker/Implementation/LowpassSpeaker.hpp"

#include "../../Utility/MemoryFuzzer.hpp"

#include "../../../ClockReceiver/JustInTime.hpp"

#include <cassert>
#include <array>

namespace {

constexpr int CLOCK_RATE = 14318180;

}

namespace Apple {
namespace IIgs {

class ConcreteMachine:
	public Apple::IIgs::Machine,
	public MachineTypes::TimedMachine,
	public MachineTypes::ScanProducer,
	public MachineTypes::AudioProducer,
	public CPU::MOS6502Esque::BusHandler<uint32_t> {

	public:
		ConcreteMachine(const Analyser::Static::AppleIIgs::Target &target, const ROMMachine::ROMFetcher &rom_fetcher) :
			m65816_(*this),
			iwm_(CLOCK_RATE),
			drives_{
		 		{CLOCK_RATE, true},
		 		{CLOCK_RATE, true}
			},
			audio_toggle_(audio_queue_),
			speaker_(audio_toggle_) {

			set_clock_rate(double(CLOCK_RATE));
			speaker_.set_input_rate(float(CLOCK_RATE) / float(audio_divider));

			using Target = Analyser::Static::AppleIIgs::Target;
			std::vector<ROMMachine::ROM> rom_descriptions;
			const std::string machine_name = "AppleIIgs";
			switch(target.model) {
				case Target::Model::ROM00:
					/* TODO */
				case Target::Model::ROM01:
					rom_descriptions.emplace_back(machine_name, "the Apple IIgs ROM01", "apple2gs.rom", 128*1024, 0x42f124b0);
				break;

				case Target::Model::ROM03:
					rom_descriptions.emplace_back(machine_name, "the Apple IIgs ROM03", "apple2gs.rom2", 256*1024, 0xde7ddf29);
				break;
			}
			rom_descriptions.push_back(video_->rom_description(Video::VideoBase::CharacterROM::EnhancedIIe));

			const auto roms = rom_fetcher(rom_descriptions);
			if(!roms[0] || !roms[1]) {
				throw ROMMachine::Error::MissingROMs;
			}
			rom_ = *roms[0];
			video_->set_character_rom(*roms[1]);

			size_t ram_size = 0;
			switch(target.memory_model) {
				case Target::MemoryModel::TwoHundredAndFiftySixKB:
					ram_size = 256;
				break;

				case Target::MemoryModel::OneMB:
					ram_size = 128 + 1024;
				break;

				case Target::MemoryModel::EightMB:
					ram_size = 128 + 8 * 1024;
				break;
			}
			ram_.resize(ram_size * 1024);

			memory_.set_storage(ram_, rom_);
			video_->set_internal_ram(&ram_[ram_.size() - 128*1024]);

			// Select appropriate ADB behaviour.
			adb_glu_.set_is_rom03(target.model == Target::Model::ROM03);

			// Attach drives to the IWM.
			// TODO: presumably attach more, some of which are 5.25"?
			iwm_->set_drive(0, &drives_[0]);
			iwm_->set_drive(1, &drives_[1]);

			// TODO: enable once machine is otherwise sane.
//			Memory::Fuzz(ram_);

			// Sync up initial values.
			memory_.set_speed_register(speed_register_);
		}

		void run_for(const Cycles cycles) override {
			m65816_.run_for(cycles);
		}

		void flush() {
			video_.flush();
			update_audio();
			iwm_.flush();
			audio_queue_.perform();
		}

		void set_scan_target(Outputs::Display::ScanTarget *target) override {
			video_->set_scan_target(target);
		}

		Outputs::Display::ScanStatus get_scaled_scan_status() const override {
			return video_->get_scaled_scan_status() * 2.0f;	// TODO: expose multiplier and divider via the JustInTime template?
		}

		void set_display_type(Outputs::Display::DisplayType display_type) final {
			video_->set_display_type(display_type);
		}

		Outputs::Display::DisplayType get_display_type() const final {
			return video_->get_display_type();
		}

		Outputs::Speaker::Speaker *get_speaker() final {
			return &speaker_;
		}

		forceinline Cycles perform_bus_operation(const CPU::WDC65816::BusOperation operation, const uint32_t address, uint8_t *const value) {
			const auto &region = MemoryMapRegion(memory_, address);
			static bool log = false;

			if(region.flags & MemoryMap::Region::IsIO) {
				// Ensure classic auxiliary and language card accesses have effect.
				const bool is_read = isReadOperation(operation);
				memory_.access(uint16_t(address), is_read);

				const auto address_suffix = address & 0xffff;
				switch(address_suffix) {

					// New video register.
					case 0xc029:
						if(is_read) {
							*value = video_->get_new_video();;
						} else {
							video_->set_new_video(*value);

							// TODO: I think bits 7 and 0 might also affect the memory map.
							// The descripton isn't especially clear — P.90 of the Hardware Reference.
							// Revisit if necessary.
						}
					break;

					// Video [and clock] interrupt register.
					case 0xc023:
						if(is_read) {
							*value = video_->get_interrupt_register();
						} else {
							video_->set_interrupt_register(*value);
						}
					break;

					// Video onterrupt-clear register.
					case 0xc032:
						if(!is_read) {
							video_->clear_interrupts(*value);
						}
					break;

					// Shadow register.
					case 0xc035:
						if(is_read) {
							*value = memory_.get_shadow_register();
						} else {
							memory_.set_shadow_register(*value);
						}
					break;

					// Clock data.
					case 0xc033:
						if(is_read) {
							*value = clock_.get_data();
						} else {
							clock_.set_data(*value);
						}
					break;

					// Clock and border control.
					case 0xc034:
						if(is_read) {
							*value = clock_.get_control();
						} else {
							clock_.set_control(*value);
							video_->set_border_colour(*value);
						}
					break;

					// Colour text control.
					case 0xc022:
						if(!is_read) {
							video_->set_text_colour(*value);
						}
					break;

					// Speed register.
					case 0xc036:
						if(is_read) {
							*value = speed_register_;
							printf("Reading speed register: %02x\n", *value);
						} else {
							memory_.set_speed_register(*value);
							speed_register_ = *value;
							printf("[Unimplemented] most of speed register: %02x\n", *value);
						}
					break;

					// [Memory] State register.
					case 0xc068:
						if(is_read) {
							*value = memory_.get_state_register();
						} else {
							memory_.set_state_register(*value);
						}
					break;

					// Various independent memory switch reads [TODO: does the IIe-style keyboard provide the low seven?].
#define SwitchRead(s) if(is_read) *value = memory_.s ? 0x80 : 0x00
#define LanguageRead(s) SwitchRead(language_card_switches().state().s)
#define AuxiliaryRead(s) SwitchRead(auxiliary_switches().switches().s)
#define VideoRead(s) if(is_read) *value = video_->s ? 0x80 : 0x00
					case 0xc011:	LanguageRead(bank1);						break;
					case 0xc012:	LanguageRead(read);							break;
					case 0xc013:	AuxiliaryRead(read_auxiliary_memory);		break;
					case 0xc014:	AuxiliaryRead(write_auxiliary_memory);		break;
					case 0xc015:	AuxiliaryRead(internal_CX_rom);				break;
					case 0xc016:	AuxiliaryRead(alternative_zero_page);		break;
					case 0xc017:	AuxiliaryRead(slot_C3_rom);					break;
					case 0xc018:	VideoRead(get_80_store());					break;
					case 0xc019:	VideoRead(get_is_vertical_blank());			break;
					case 0xc01a:	VideoRead(get_text());						break;
					case 0xc01b:	VideoRead(get_mixed());						break;
					case 0xc01c:	VideoRead(get_page2());						break;
					case 0xc01d:	VideoRead(get_high_resolution());			break;
					case 0xc01e:	VideoRead(get_alternative_character_set());	break;
					case 0xc01f:	VideoRead(get_80_columns());				break;
					case 0xc046:	VideoRead(get_annunciator_3());				break;
#undef VideoRead
#undef AuxiliaryRead
#undef LanguageRead
#undef SwitchRead

					// Video switches (and annunciators).
					case 0xc050: case 0xc051:
						video_->set_text(address & 1);
					break;
					case 0xc052: case 0xc053:
						video_->set_mixed(address & 1);
					break;
					case 0xc054: case 0xc055:
						video_->set_page2(address&1);
					break;
					case 0xc056: case 0xc057:
						video_->set_high_resolution(address&1);
					break;
					case 0xc058: case 0xc059:
					case 0xc05a: case 0xc05b:
					case 0xc05c: case 0xc05d:
						// Annunciators 0, 1 and 2.
					break;
					case 0xc05e: case 0xc05f:
						video_->set_annunciator_3(!(address&1));
					break;
					case 0xc001:	/* 0xc000 is dealt with in the ADB section. */
						if(!is_read) video_->set_80_store(true);
					break;
					case 0xc00c: case 0xc00d:
						if(!is_read) video_->set_80_columns(address & 1);
					break;
					case 0xc00e: case 0xc00f:
						if(!is_read) video_->set_alternative_character_set(address & 1);
					break;

					// ADB and keyboard.
					case 0xc000:
						if(is_read) {
							*value = adb_glu_.get_keyboard_data();
						} else {
							video_->set_80_store(false);
						}
					break;
					case 0xc010:
						adb_glu_.clear_key_strobe();
						if(is_read) {
							*value = adb_glu_.get_any_key_down() ? 0x80 : 0x00;
						}
					break;
					case 0xc024:
						if(is_read) {
							*value = adb_glu_.get_mouse_data();
						}
					break;
					case 0xc025:
						if(is_read) {
							*value = adb_glu_.get_modifier_status();
						}
					break;
					case 0xc026:
						if(is_read) {
							*value = adb_glu_.get_data();
						} else {
							adb_glu_.set_command(*value);
						}
					break;
					case 0xc027:
						if(is_read) {
							*value = adb_glu_.get_status();
						} else {
							adb_glu_.set_status(*value);
						}
					break;

					// The SCC.
					case 0xc038: case 0xc039: case 0xc03a: case 0xc03b:
						if(is_read) {
							*value = scc_.read(int(address));
						} else {
							scc_.write(int(address), *value);
						}
					break;

					// The audio GLU.
					case 0xc03c:
						if(is_read) {
							*value = sound_glu_.get_control();
						} else {
							sound_glu_.set_control(*value);
						}
					break;
					case 0xc03d:
						if(is_read) {
							*value = sound_glu_.get_data();
						} else {
							sound_glu_.set_data(*value);
						}
					break;
					case 0xc03e:
						if(is_read) {
							*value = sound_glu_.get_address_low();
						} else {
							sound_glu_.set_address_low(*value);
						}
					break;
					case 0xc03f:
						if(is_read) {
							*value = sound_glu_.get_address_high();
						} else {
							sound_glu_.set_address_high(*value);
						}
					break;


					// These were all dealt with by the call to memory_.access.
					// TODO: subject to read data? Does vapour lock apply?
					case 0xc002: case 0xc003: case 0xc004: case 0xc005:
					case 0xc006: case 0xc007: case 0xc008: case 0xc009: case 0xc00a: case 0xc00b:
					break;

					// Interrupt ROM addresses; Cf. P25 of the Hardware Reference.
					case 0xc071: case 0xc072: case 0xc073: case 0xc074: case 0xc075: case 0xc076: case 0xc077:
					case 0xc078: case 0xc079: case 0xc07a: case 0xc07b: case 0xc07c: case 0xc07d: case 0xc07e: case 0xc07f:
						if(is_read) {
							*value = rom_[rom_.size() - 65536 + address_suffix];
						}
					break;

					// Analogue inputs. All TODO.
					case 0xc060: case 0xc061: case 0xc062: case 0xc063:
						// Joystick buttons (and keyboard modifiers).
						if(is_read) {
							*value = 0x00;
						}
					break;

					case 0xc064: case 0xc065: case 0xc066: case 0xc067:
						// Analogue inputs.
						if(is_read) {
							*value = 0x00;
						}
					break;

					case 0xc070:
						// TODO: begin analogue channel charge.
					break;

					// Monochome/colour register.
					case 0xc021:
						// "Uses bit 7 to determine whether composite output is colour 9) or gray scale (1)."
						if(is_read) {
							*value = video_->get_composite_is_colour() ? 0x00 : 0x80;
						} else {
							video_->set_composite_is_colour(!(*value & 0x80));
						}
					break;

					// Language select. (?)
					case 0xc02b:
						if(is_read) {
							*value = language_;
						} else {
							language_ = *value;
						}
					break;

					// Slot select.
					case 0xc02d:
						// b7: 0 = internal ROM code for slot 7;
						// b6: 0 = internal ROM code for slot 6;
						// b5: 0 = internal ROM code for slot 5;
						// b4: 0 = internal ROM code for slot 4;
						// b3: reserved;
						// b2: internal ROM code for slot 2;
						// b1: internal ROM code for slot 1;
						// b0: reserved.
						if(is_read) {
							*value = card_mask_;
						} else {
							card_mask_ = *value;
						}
					break;

					case 0xc030:
						update_audio();
						audio_toggle_.set_output(!audio_toggle_.get_output());
					break;

					// Addresses that seemingly map to nothing; provided as a separate break out for now,
					// while I have an assert on unknown reads.
					case 0xc049: case 0xc04a: case 0xc04b: case 0xc04c: case 0xc04d: case 0xc04e: case 0xc04f:
					case 0xc069: case 0xc06a: case 0xc06b: case 0xc06c:
						printf("Ignoring %04x\n", address_suffix);
//						log = true;
					break;

					// 'Test Mode', whatever that is (?)
					case 0xc06e: case 0xc06f:
						test_mode_ = address & 1;
					break;
					case 0xc06d:
						if(is_read) {
							*value = test_mode_ * 0x80;
						}
					break;

					// Disk drive controls additional to the IWM.
					case 0xc031:
						// b7: 0 = use head 0; 1 = use head 1.
						// b6: 0 = use 5.25" disks; 1 = use 3.5".
						if(!is_read) {
							disk_select_ = *value;
							iwm_->set_select(*value & 0x80);

							// Presumably bit 6 selects between two 5.25" drives rather than the two 3.5"?
							if(*value & 0x40) {
								iwm_->set_drive(0, &drives_[0]);
								iwm_->set_drive(1, &drives_[1]);
							} else {
								// TODO: add 5.25" drives.
								// (and any Smartport devices?)
								iwm_->set_drive(0, nullptr);
								iwm_->set_drive(1, nullptr);
							}
						} else {
							*value = disk_select_;
						}
					break;

					default:
						// Check for a card access.
						if(address_suffix >= 0xc080 && address_suffix < 0xc800) {
							// This is an abridged version of the similar code in AppleII.cpp from
							// line 653; it would be good to factor that out and support cards here.
							// For now just either supply the internal ROM or nothing as per the
							// current card mask.

							size_t card_number = 0;
							if(address >= 0xc100) {
								/*
									Decode the area conventionally used by cards for ROMs:
										0xCn00 to 0xCnff: card n.
								*/
								card_number = (address - 0xc000) >> 8;
							} else {
								/*
									Decode the area conventionally used by cards for registers:
										C0n0 to C0nF: card n - 8.
								*/
								card_number = (address - 0xc080) >> 4;
							}

							const uint8_t permitted_card_mask_ = card_mask_ & 0xf6;
							if(permitted_card_mask_ & (1 << card_number)) {
								// TODO: Access an actual card.
								if(is_read) {
									*value = 0xff;
								}
							} else {
								switch(address_suffix) {
									default:
										printf("Internal card-area access: %04x\n", address_suffix);
										if(is_read) {
											*value = rom_[rom_.size() - 65536 + address_suffix];
										}
									break;

									// IWM.
									case 0xc0e0:	case 0xc0e1:	case 0xc0e2:	case 0xc0e3:
									case 0xc0e4:	case 0xc0e5:	case 0xc0e6:	case 0xc0e7:
									case 0xc0e8:	case 0xc0e9:	case 0xc0ea:	case 0xc0eb:
									case 0xc0ec:	case 0xc0ed:	case 0xc0ee:	case 0xc0ef:
										if(is_read) {
											*value = iwm_->read(int(address_suffix));
										} else {
											iwm_->write(int(address_suffix), *value);
										}
									break;

									// TODO: 0xc0c8, 0xc0c9, 0xc0d8, 0xc0d9, 0xc0f8, 0xc0f9 drive motors.
								}
								// TODO: disk-port soft switches should be in COEx.
//								log = true;
							}
						} else {
							if(address_suffix < 0xc080) {
								// TODO: all other IO accesses.
								printf("Unhandled IO: %04x\n", address_suffix);
								assert(false);
							}
						}
				}
			} else {
				// For debugging purposes; if execution heads off into an unmapped page then
				// it's pretty certain that my 65816 still has issues.
				assert(operation != CPU::WDC65816::BusOperation::ReadOpcode || region.read);

				if(isReadOperation(operation)) {
					MemoryMapRead(region, address, value);
				} else {
					// Use a very broad test for flushing video: any write to $e0 or $e1, or any write that is shadowed.
					// TODO: at least restrict the e0/e1 test to possible video buffers!
					if((address >= 0xe00000 && address < 0xe1000000) || region.flags & MemoryMap::Region::IsShadowed) {
						video_.flush();
					}

					MemoryMapWrite(memory_, region, address, value);
				}
			}

			if(operation == CPU::WDC65816::BusOperation::ReadOpcode) {
				assert(address);
			}
//			log |= (address >= 0xff9b00) && (address < 0xff9b32);
			if(log) {
				printf("%06x %s %02x", address, isReadOperation(operation) ? "->" : "<-", *value);
				if(operation == CPU::WDC65816::BusOperation::ReadOpcode) {
					printf(" a:%04x x:%04x y:%04x s:%04x e:%d p:%02x db:%02x pb:%02x d:%04x\n",
						m65816_.get_value_of_register(CPU::WDC65816::Register::A),
						m65816_.get_value_of_register(CPU::WDC65816::Register::X),
						m65816_.get_value_of_register(CPU::WDC65816::Register::Y),
						m65816_.get_value_of_register(CPU::WDC65816::Register::StackPointer),
						m65816_.get_value_of_register(CPU::WDC65816::Register::EmulationFlag),
						m65816_.get_value_of_register(CPU::WDC65816::Register::Flags),
						m65816_.get_value_of_register(CPU::WDC65816::Register::DataBank),
						m65816_.get_value_of_register(CPU::WDC65816::Register::ProgramBank),
						m65816_.get_value_of_register(CPU::WDC65816::Register::Direct
						)
					);
				} else printf("\n");
			}

			Cycles duration = Cycles(5);

			// TODO: determine the cost of this access.
//			if((mapping.flags & BankMapping::Is1Mhz) || ((mapping.flags & BankMapping::IsShadowed) && !isReadOperation(operation))) {
//				// TODO: (i) get into phase; (ii) allow for the 1Mhz bus length being sporadically 16 rather than 14.
//				duration = Cycles(14);
//			} else {
//				// TODO: (i) get into phase; (ii) allow for collisions with the refresh cycle.
//				duration = Cycles(5);
//			}
			fast_access_phase_ = (fast_access_phase_ + duration.as<int>()) % 5;		// TODO: modulo something else, to allow for refresh.
			slow_access_phase_ = (slow_access_phase_ + duration.as<int>()) % 14;	// TODO: modulo something else, to allow for stretched cycles.


			// Propagate time far and wide.
			cycles_since_clock_tick_ += duration;
			auto ticks = cycles_since_clock_tick_.divide(Cycles(CLOCK_RATE)).as_integral();
			while(ticks--) {
				clock_.update();
				video_.last_valid()->notify_clock_tick();	// The video controller marshalls the one-second interrupt.
				update_interrupts();
			}

			video_ += duration;
			iwm_ += duration;
			cycles_since_audio_update_ += duration;

			// Ensure no more than a single line is enqueued for just-in-time video purposes.
			// TODO: as implemented, check_flush_threshold doesn't actually work. Can it be made to, or is it a bad idea?
			if(video_.check_flush_threshold<131>()) {
				update_interrupts();
			}

			return duration;
		}

		void update_interrupts() {
			// Update the interrupt line. TODO: should include the sound GLU too.
			m65816_.set_irq_line(video_.last_valid()->get_interrupt_register() & 0x80);
		}

	private:
		CPU::WDC65816::Processor<ConcreteMachine, false> m65816_;
		MemoryMap memory_;

		// MARK: - Timing.

		int fast_access_phase_ = 0;
		int slow_access_phase_ = 0;

		uint8_t speed_register_ = 0x40;	// i.e. Power-on status. (TODO: only if ROM03?)

		// MARK: - Memory storage.

		std::vector<uint8_t> ram_{};
		std::vector<uint8_t> rom_;

		// MARK: - Other components.

		Apple::Clock::ParallelClock clock_;
		JustInTimeActor<Apple::IIgs::Video::Video, 1, 2, Cycles> video_;	// i.e. run video at twice the 1Mhz clock.
		Apple::IIgs::ADB::GLU adb_glu_;
 		Zilog::SCC::z8530 scc_;
 		JustInTimeActor<Apple::IWM, 1, 1, Cycles> iwm_;
 		Cycles cycles_since_clock_tick_;
		Apple::Macintosh::DoubleDensityDrive drives_[2];

		// The audio parts.
		Apple::IIgs::Sound::GLU sound_glu_;
		Concurrency::DeferringAsyncTaskQueue audio_queue_;
		Audio::Toggle audio_toggle_;
		Outputs::Speaker::LowpassSpeaker<Audio::Toggle> speaker_;
		Cycles cycles_since_audio_update_;
		static constexpr int audio_divider = 8;
		void update_audio() {
			speaker_.run_for(audio_queue_, cycles_since_audio_update_.divide(Cycles(audio_divider)));
		}

		// MARK: - Cards.

		// TODO: most of cards.
		uint8_t card_mask_ = 0x00;

		bool test_mode_ = false;
		uint8_t language_ = 0;
		uint8_t disk_select_ = 0;
};

}
}

using namespace Apple::IIgs;

Machine *Machine::AppleIIgs(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	return new ConcreteMachine(*dynamic_cast<const Analyser::Static::AppleIIgs::Target *>(target), rom_fetcher);
}

Machine::~Machine() {}
