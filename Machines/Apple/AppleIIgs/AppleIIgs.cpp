//
//  AppleIIgs.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/10/2020.
//  Copyright 2020 Thomas Harte. All rights reserved.
//

#include "AppleIIgs.hpp"

#include "../../../Activity/Source.hpp"
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
#include "../../../Components/DiskII/DiskIIDrive.hpp"

#include "../../../Outputs/Speaker/Implementation/CompoundSource.hpp"
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
	public Activity::Source,
	public Apple::IIgs::Machine,
	public MachineTypes::AudioProducer,
	public MachineTypes::MediaTarget,
	public MachineTypes::ScanProducer,
	public MachineTypes::TimedMachine,
	public CPU::MOS6502Esque::BusHandler<uint32_t> {

	public:
		ConcreteMachine(const Analyser::Static::AppleIIgs::Target &target, const ROMMachine::ROMFetcher &rom_fetcher) :
			m65816_(*this),
			iwm_(CLOCK_RATE / 2),
			drives35_{
		 		{CLOCK_RATE / 2, true},
		 		{CLOCK_RATE / 2, true}
			},
			drives525_{
				{CLOCK_RATE / 2},
				{CLOCK_RATE / 2}
			},
			sound_glu_(audio_queue_),
			audio_toggle_(audio_queue_),
			mixer_(sound_glu_, audio_toggle_),
			speaker_(mixer_) {

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
			iwm_->set_drive(0, &drives35_[0]);
			iwm_->set_drive(1, &drives35_[1]);

			// Randomise RAM contents.
			Memory::Fuzz(ram_);

			// Sync up initial values.
			memory_.set_speed_register(speed_register_);

			insert_media(target.media);
		}

		~ConcreteMachine() {
			audio_queue_.flush();
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

		// MARK: MediaTarget.
		bool insert_media(const Analyser::Static::Media &media) final {
			if(!media.disks.empty()) {
				const auto disk = media.disks[0];
				if(disk->get_maximum_head_position().as_int() > 35) {
					drives35_[0].set_disk(media.disks[0]);
				} else {
					drives525_[0].set_disk(media.disks[0]);
				}
			}
			return true;
		}

		// MARK: Activity::Source
		void set_activity_observer(Activity::Observer *observer) final {
			drives35_[0].set_activity_observer(observer, "First 3.5\" Drive", true);
			drives35_[1].set_activity_observer(observer, "Second 3.5\" Drive", true);
			drives525_[0].set_activity_observer(observer, "First 5.25\" Drive", true);
			drives525_[1].set_activity_observer(observer, "Second 5.25\" Drive", true);
		}

		// MARK: BusHandler.
		forceinline Cycles perform_bus_operation(const CPU::WDC65816::BusOperation operation, const uint32_t address, uint8_t *const value) {
			const auto &region = MemoryMapRegion(memory_, address);
			static bool log = false;
			static uint64_t total = 0;
			bool is_1Mhz = (region.flags & MemoryMap::Region::Is1Mhz) || !(speed_register_ & 0x80) || (speed_register_ & motor_flags_);

			if(region.flags & MemoryMap::Region::IsIO) {
				// Ensure classic auxiliary and language card accesses have effect.
				const bool is_read = isReadOperation(operation);
				memory_.access(uint16_t(address), is_read);

				// TODO: which of these are actually 2.8Mhz?

				const auto address_suffix = address & 0xffff;
#define ReadWrite(x)	(x) | (is_read * 0x10000)
#define Read(x)			(x) | 0x10000
#define Write(x)		(x)
				switch(ReadWrite(address_suffix)) {

					// New video register.
					case Read(0xc029):
						*value = video_->get_new_video();;
					break;
					case Write(0xc029):
						video_->set_new_video(*value);

						// TODO: I think bits 7 and 0 might also affect the memory map.
						// The descripton isn't especially clear — P.90 of the Hardware Reference.
						// Revisit if necessary.
					break;

					// Video [and clock] interrupt register.
					case Read(0xc023):
						*value = video_->get_interrupt_register();
					break;
					case Write(0xc023):
						video_->set_interrupt_register(*value);
					break;

					// Video interrupt-clear register.
					case Write(0xc032):
						video_->clear_interrupts(*value);
					break;

					// Shadow register.
					case Read(0xc035):
						*value = memory_.get_shadow_register();
					break;
					case Write(0xc035):
						memory_.set_shadow_register(*value);
					break;

					// Clock data.
					case Read(0xc033):
						*value = clock_.get_data();
					break;
					case Write(0xc033):
						clock_.set_data(*value);
					break;

					// Clock and border control.
					case Read(0xc034):
						*value = clock_.get_control();
					break;
					case Write(0xc034):
						clock_.set_control(*value);
						video_->set_border_colour(*value);
					break;

					// Colour text control.
					case Write(0xc022):
						video_->set_text_colour(*value);
					break;

					// Speed register.
					case Read(0xc036):
						*value = speed_register_;
					break;
					case Write(0xc036):
						// b7: 1 => operate at 2.8Mhz; 0 => 1Mhz.
						// b6: power-on status; 1 => system has been turned on by the power switch (TODO: what clears this?)
						// b5: reserved
						// b4: [bank shadowing bit; cf. the memory map]
						// b0–3: motor on-off speed detectors;
						//		1 => switch to 1Mhz if motor is on; 0 => don't;
						//		b0 = slot 4 (i.e. watches addresses c0c9, c0c8)
						//		b1 = slot 5 (i.e. c0d9, c0d8)
						//		b2 = slot 6 (i.e. c0e9, c0e8)
						//		b3 = slot 7 (i.e. c0f9, c0f8)
						memory_.set_speed_register(*value);
						speed_register_ = *value;
					break;

					// [Memory] State register.
					case Read(0xc068):
						*value = memory_.get_state_register();
					break;
					case Write(0xc068):
						memory_.set_state_register(*value);
					break;

					// Various independent memory switch reads [TODO: does the IIe-style keyboard provide the low seven?].
#define SwitchRead(s) *value = memory_.s ? 0x80 : 0x00
#define LanguageRead(s) SwitchRead(language_card_switches().state().s)
#define AuxiliaryRead(s) SwitchRead(auxiliary_switches().switches().s)
#define VideoRead(s) *value = video_.last_valid()->s ? 0x80 : 0x00
					case Read(0xc011):	LanguageRead(bank1);						break;
					case Read(0xc012):	LanguageRead(read);							break;
					case Read(0xc013):	AuxiliaryRead(read_auxiliary_memory);		break;
					case Read(0xc014):	AuxiliaryRead(write_auxiliary_memory);		break;
					case Read(0xc015):	AuxiliaryRead(internal_CX_rom);				break;
					case Read(0xc016):	AuxiliaryRead(alternative_zero_page);		break;
					case Read(0xc017):	AuxiliaryRead(slot_C3_rom);					break;
					case Read(0xc018):	VideoRead(get_80_store());					break;
					case Read(0xc019):
						VideoRead(get_is_vertical_blank(video_.time_since_flush()));
					break;
					case Read(0xc01a):	VideoRead(get_text());						break;
					case Read(0xc01b):	VideoRead(get_mixed());						break;
					case Read(0xc01c):	VideoRead(get_page2());						break;
					case Read(0xc01d):	VideoRead(get_high_resolution());			break;
					case Read(0xc01e):	VideoRead(get_alternative_character_set());	break;
					case Read(0xc01f):	VideoRead(get_80_columns());				break;
					case Read(0xc046):	VideoRead(get_annunciator_3());				break;
#undef VideoRead
#undef AuxiliaryRead
#undef LanguageRead
#undef SwitchRead

					// Video switches (and annunciators).
					case Read(0xc050): case Read(0xc051):
					case Write(0xc050): case Write(0xc051):
						video_->set_text(address & 1);
					break;
					case Read(0xc052): case Read(0xc053):
					case Write(0xc052): case Write(0xc053):
						video_->set_mixed(address & 1);
					break;
					case Read(0xc054): case Read(0xc055):
					case Write(0xc054): case Write(0xc055):
						video_->set_page2(address&1);
					break;
					case Read(0xc056): case Read(0xc057):
					case Write(0xc056): case Write(0xc057):
						video_->set_high_resolution(address&1);
					break;
					case Read(0xc058): case Read(0xc059):
					case Write(0xc058): case Write(0xc059):
					case Read(0xc05a): case Read(0xc05b):
					case Write(0xc05a): case Write(0xc05b):
					case Read(0xc05c): case Read(0xc05d):
					case Write(0xc05c): case Write(0xc05d):
						// Annunciators 0, 1 and 2.
					break;
					case Read(0xc05e): case Read(0xc05f):
					case Write(0xc05e): case Write(0xc05f):
						video_->set_annunciator_3(!(address&1));
					break;
					case Write(0xc000): case Write(0xc001):
						video_->set_80_store(address & 1);
					break;
					case Write(0xc00c): case Write(0xc00d):
						video_->set_80_columns(address & 1);
					break;
					case Write(0xc00e): case Write(0xc00f):
						video_->set_alternative_character_set(address & 1);
					break;

					// ADB and keyboard.
					case Read(0xc000):
						*value = adb_glu_.get_keyboard_data();
					break;
					case Read(0xc010):
						*value = adb_glu_.get_any_key_down() ? 0x80 : 0x00;
						[[fallthrough]];
					case Write(0xc010):
						adb_glu_.clear_key_strobe();
					break;

					case Read(0xc024):
						*value = adb_glu_.get_mouse_data();
					break;
					case Read(0xc025):
						*value = adb_glu_.get_modifier_status();
					break;
					case Read(0xc026):
						*value = adb_glu_.get_data();
					break;
					case Write(0xc026):
						adb_glu_.set_command(*value);
					break;
					case Read(0xc027):
						*value = adb_glu_.get_status();
					break;
					case Write(0xc027):
						adb_glu_.set_status(*value);
					break;

					// The SCC.
					case Read(0xc038): case Read(0xc039): case Read(0xc03a): case Read(0xc03b):
						*value = scc_.read(int(address));
					break;
					case Write(0xc038): case Write(0xc039): case Write(0xc03a): case Write(0xc03b):
						scc_.write(int(address), *value);
					break;

					// The audio GLU.
					case Read(0xc03c):
						update_audio();
						*value = sound_glu_.get_control();
					break;
					case Write(0xc03c):
						update_audio();
						sound_glu_.set_control(*value);
					break;
					case Read(0xc03d):
						update_audio();
						*value = sound_glu_.get_data();
					break;
					case Write(0xc03d):
						update_audio();
						sound_glu_.set_data(*value);
					break;
					case Read(0xc03e):
						update_audio();
						*value = sound_glu_.get_address_low();
					break;
					case Write(0xc03e):
						update_audio();
						sound_glu_.set_address_low(*value);
					break;
					case Read(0xc03f):
						update_audio();
						*value = sound_glu_.get_address_high();
					break;
					case Write(0xc03f):
						update_audio();
						sound_glu_.set_address_high(*value);
					break;


					// These were all dealt with by the call to memory_.access.
					// TODO: subject to read data? Does vapour lock apply?
					case 0xc002: case 0xc003: case 0xc004: case 0xc005:
					case 0xc006: case 0xc007: case 0xc008: case 0xc009: case 0xc00a: case 0xc00b:
					break;

					// Interrupt ROM addresses; Cf. P25 of the Hardware Reference.
					case Read(0xc071): case Read(0xc072): case Read(0xc073):
					case Read(0xc074): case Read(0xc075): case Read(0xc076): case Read(0xc077):
					case Read(0xc078): case Read(0xc079): case Read(0xc07a): case Read(0xc07b):
					case Read(0xc07c): case Read(0xc07d): case Read(0xc07e): case Read(0xc07f):
						*value = rom_[rom_.size() - 65536 + address_suffix];
					break;

					// Analogue inputs. All TODO.
					case Read(0xc060): case Read(0xc061): case Read(0xc062): case Read(0xc063):
						// Joystick buttons (and keyboard modifiers).
						*value = 0x00;
					break;

					case Read(0xc064): case Read(0xc065): case Read(0xc066): case Read(0xc067):
						// Analogue inputs.
						*value = 0x00;
					break;

					case Read(0xc070): case Write(0xc070):
						// TODO: begin analogue channel charge.
					break;

					// Monochome/colour register.
					case Read(0xc021):
						// "Uses bit 7 to determine whether composite output is colour 9) or gray scale (1)."
						*value = video_->get_composite_is_colour() ? 0x00 : 0x80;
					break;
					case Write(0xc021):
						video_->set_composite_is_colour(!(*value & 0x80));
					break;

					// Language select. (?)
					case Read(0xc02b):
						*value = language_;
					break;
					case Write(0xc02b):
						language_ = *value;
					break;

					// Slot select.
					case Read(0xc02d):
						// b7: 0 = internal ROM code for slot 7;
						// b6: 0 = internal ROM code for slot 6;
						// b5: 0 = internal ROM code for slot 5;
						// b4: 0 = internal ROM code for slot 4;
						// b3: reserved;
						// b2: internal ROM code for slot 2;
						// b1: internal ROM code for slot 1;
						// b0: reserved.
						*value = card_mask_;
					break;
					case Write(0xc02d):
						card_mask_ = *value;
					break;

					case Read(0xc030): case Write(0xc030):
						update_audio();
						audio_toggle_.set_output(!audio_toggle_.get_output());
					break;

					// Addresses that seemingly map to nothing; provided as a separate break out for now,
					// while I have an assert on unknown accesses.
					case 0xc049: case 0xc04a: case 0xc04b: case 0xc04c: case 0xc04d: case 0xc04e: case 0xc04f:
					case 0xc069: case 0xc06a: case 0xc06b: case 0xc06c:
						printf("Ignoring %04x\n", address_suffix);
//						log = true;
					break;

					// 'Test Mode', whatever that is (?)
					case Read(0xc06e): case Read(0xc06f):
					case Write(0xc06e): case Write(0xc06f):
						test_mode_ = address & 1;
					break;
					case Read(0xc06d):
						*value = test_mode_ * 0x80;
					break;

					// Disk drive controls additional to the IWM.
					case Read(0xc031):
						*value = disk_select_;
					break;
					case Write(0xc031):
						// b7: 0 = use head 0; 1 = use head 1.
						// b6: 0 = use 5.25" disks; 1 = use 3.5".
						disk_select_ = *value;
						iwm_->set_select(*value & 0x80);

						// Presumably bit 6 selects between two 5.25" drives rather than the two 3.5"?
						if(*value & 0x40) {
							iwm_->set_drive(0, &drives35_[0]);
							iwm_->set_drive(1, &drives35_[1]);
						} else {
							iwm_->set_drive(0, &drives525_[0]);
							iwm_->set_drive(1, &drives525_[1]);
						}
					break;

					// Addresses on other Apple II devices which do nothing on the GS.
					case Write(0xc07e):
					break;

					default:
						// Update motor mask bits.
						switch(address_suffix) {
							case 0xc0c8: motor_flags_ &= ~0x01;	break;
							case 0xc0c9: motor_flags_ |= 0x01;	break;
							case 0xc0d8: motor_flags_ &= ~0x02;	break;
							case 0xc0d9: motor_flags_ |= 0x02;	break;
							case 0xc0e8: motor_flags_ &= ~0x04;	break;
							case 0xc0e9: motor_flags_ |= 0x04;	break;
							case 0xc0f8: motor_flags_ &= ~0x08;	break;
							case 0xc0f9: motor_flags_ |= 0x08;	break;
						}

						// Check for a card access.
						if(address_suffix >= 0xc080 && address_suffix < 0xc800) {
							// This is an abridged version of the similar code in AppleII.cpp from
							// line 653; it would be good to factor that out and support cards here.
							// For now just either supply the internal ROM or nothing as per the
							// current card mask.

							size_t card_number = 0;
							if(address_suffix >= 0xc100) {
								/*
									Decode the area conventionally used by cards for ROMs:
										0xCn00 to 0xCnff: card n.
								*/
								card_number = (address_suffix - 0xc000) >> 8;
							} else {
								/*
									Decode the area conventionally used by cards for registers:
										C0n0 to C0nF: card n - 8.
								*/
								card_number = (address_suffix - 0xc080) >> 4;
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
										// Temporary: log _potential_ mistakes.
										if(address_suffix < 0xc100) {
											printf("Internal card-area access: %04x\n", address_suffix);
											log |= operation == CPU::WDC65816::BusOperation::ReadOpcode;
										}
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
								}
//								log = true;
							}
#undef ReadWrite
#undef Read
#undef Write
						} else {
							// Access the internal ROM.
							//
							// TODO: should probably occur only if there was a preceding access to a built-in
							// card ROM?
							if(is_read) {
								*value = rom_[rom_.size() - 65536 + address_suffix];
							}

							if(address_suffix < 0xc080) {
								// TODO: all other IO accesses.
								printf("Unhandled IO %s: %04x\n", is_read ? "read" : "write", address_suffix);
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
					// Shadowed writes also occur "at 1Mhz".
					// TODO: this is probably an approximation. I'm assuming that there's the ability asynchronously to post
					// both a 1Mhz cycle and a 2.8Mhz cycle and since the latter always fits into the former, this is sufficiently
					// descriptive. I suspect this isn't true as it wouldn't explain the speed boost that Wolfenstein and others
					// get by adding periodic NOPs within their copy-to-shadow step.
					//
					// Maybe the interaction with 2.8Mhz refresh isn't as straightforward as I think?
					is_1Mhz |= region.flags & MemoryMap::Region::IsShadowed;

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
//			if(address == 0xe115fe || address == 0xe115ff) {
//				printf("%06x %s %02x%s\n", address, isReadOperation(operation) ? "->" : "<-", *value,
//					operation == CPU::WDC65816::BusOperation::ReadOpcode ? " [*]" : "");
//			}
//			log |= (operation == CPU::WDC65816::BusOperation::ReadOpcode) && (address == 0xfc0d50);
//			log &= !((operation == CPU::WDC65816::BusOperation::ReadOpcode) && ((address >= 0xfc0d5b) || (address < 0xfc0d50)));
			if(log) {
				printf("%06x %s %02x", address, isReadOperation(operation) ? "->" : "<-", *value);
				if(operation == CPU::WDC65816::BusOperation::ReadOpcode) {
					printf(" a:%04x x:%04x y:%04x s:%04x e:%d p:%02x db:%02x pb:%02x d:%04x [tot:%llu]\n",
						m65816_.get_value_of_register(CPU::WDC65816::Register::A),
						m65816_.get_value_of_register(CPU::WDC65816::Register::X),
						m65816_.get_value_of_register(CPU::WDC65816::Register::Y),
						m65816_.get_value_of_register(CPU::WDC65816::Register::StackPointer),
						m65816_.get_value_of_register(CPU::WDC65816::Register::EmulationFlag),
						m65816_.get_value_of_register(CPU::WDC65816::Register::Flags),
						m65816_.get_value_of_register(CPU::WDC65816::Register::DataBank),
						m65816_.get_value_of_register(CPU::WDC65816::Register::ProgramBank),
						m65816_.get_value_of_register(CPU::WDC65816::Register::Direct),
						total
					);
				} else printf("\n");
			}

			// TODO: fully determine the cost of this access.
			// Below is very vague on real details. Won't do.
			Cycles duration;
			if(is_1Mhz) {
				// TODO: this is very implicitly linked to the video timing; make that overt somehow. Even if it's just with a redundant video setter at construction.
				const int current_length = 14 + 2*(slow_access_phase_ / 896);						// Length of cycle currently ongoing.
				const int phase_adjust = (current_length - slow_access_phase_%14)%current_length;	// Amount of time to expand waiting until end of cycle, if not actually at start.
				const int access_phase = (slow_access_phase_ + phase_adjust)%912;					// Phase at which access will begin.
				const int next_length = 14 + 2*(access_phase / 896);								// Length of cycle that this access will occur within.
				duration = Cycles(next_length + phase_adjust);
			} else {
				// Clues as to 'fast' refresh timing:
				//
				//	(i)		"The time required for the refresh cycles reduces the effective
				//			processor speed for programs in RAM by about 8 percent.";
				//	(ii)	"These cycles occur approximately every 3.5 microseconds"
				//
				// 3.5µs @ 14,318,180Hz => one every 50.11 cycles. Safe to assume every 10th fast cycle
				// is refresh? That feels like a lot.
				//
				// (and the IIgs is smart enough that refresh is applicable only to RAM accesses).
				const int phase_adjust = (5 - fast_access_phase_%5)%5;
				const int refresh = (fast_access_phase_ / 45) * bool(region.write) * 5;
				duration = Cycles(5 + phase_adjust + refresh);
			}
			// TODO: lookup tables to avoid the above? LCM of the two phases is 22,800 so probably 912+50 bytes plus two counters.
			fast_access_phase_ = (fast_access_phase_ + duration.as<int>()) % 50;
			slow_access_phase_ = (slow_access_phase_ + duration.as<int>()) % 912;


			// Propagate time far and wide.
			cycles_since_clock_tick_ += duration;
			auto ticks = cycles_since_clock_tick_.divide(Cycles(CLOCK_RATE)).as_integral();
			while(ticks--) {
				clock_.update();
				video_.last_valid()->notify_clock_tick();	// The video controller marshalls the one-second interrupt.
															// TODO: I think I may have made a false assumption here; does
															// the VGC have an independent 1-second interrupt?
				update_interrupts();
			}

			const bool will_flush_video = video_.will_flush(duration);
			video_ += duration;
			iwm_ += duration;
			cycles_since_audio_update_ += duration;
			total += decltype(total)(duration.as_integral());

			if(will_flush_video) {
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
		uint8_t motor_flags_ = 0x00;

		// MARK: - Memory storage.

		std::vector<uint8_t> ram_{};
		std::vector<uint8_t> rom_;

		// MARK: - Other components.

		Apple::Clock::ParallelClock clock_;
		JustInTimeActor<Apple::IIgs::Video::Video, 1, 2, Cycles> video_;	// i.e. run video at twice the 1Mhz clock.
		Apple::IIgs::ADB::GLU adb_glu_;
 		Zilog::SCC::z8530 scc_;
 		JustInTimeActor<Apple::IWM, 1, 2, Cycles> iwm_;
 		Cycles cycles_since_clock_tick_;
		Apple::Macintosh::DoubleDensityDrive drives35_[2];
		Apple::Disk::DiskIIDrive drives525_[2];

		// The audio parts.
		Concurrency::DeferringAsyncTaskQueue audio_queue_;
		Apple::IIgs::Sound::GLU sound_glu_;
		Audio::Toggle audio_toggle_;
		using AudioSource = Outputs::Speaker::CompoundSource<Apple::IIgs::Sound::GLU, Audio::Toggle>;
		AudioSource mixer_;
		Outputs::Speaker::LowpassSpeaker<AudioSource> speaker_;
		Cycles cycles_since_audio_update_;
		static constexpr int audio_divider = 16;
		void update_audio() {
			const auto divided_cycles = cycles_since_audio_update_.divide(Cycles(audio_divider));
			sound_glu_.run_for(divided_cycles);
			speaker_.run_for(audio_queue_, divided_cycles);
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
