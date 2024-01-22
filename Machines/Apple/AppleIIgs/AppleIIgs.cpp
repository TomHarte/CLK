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

#include "../../../Analyser/Static/AppleIIgs/Target.hpp"

#include "ADB.hpp"
#include "MemoryMap.hpp"
#include "Video.hpp"
#include "Sound.hpp"

#include "../../../Processors/65816/65816.hpp"
#include "../../../Components/8530/z8530.hpp"
#include "../../../Components/AppleClock/AppleClock.hpp"
#include "../../../Components/AudioToggle/AudioToggle.hpp"
#include "../../../Components/DiskII/IWM.hpp"
#include "../../../Components/DiskII/MacintoshDoubleDensityDrive.hpp"
#include "../../../Components/DiskII/DiskIIDrive.hpp"

#include "../AppleII/Joystick.hpp"

#include "../../../Outputs/Speaker/Implementation/CompoundSource.hpp"
#include "../../../Outputs/Speaker/Implementation/LowpassSpeaker.hpp"

#include "../../Utility/MemoryFuzzer.hpp"

#include "../../../ClockReceiver/JustInTime.hpp"

#include <cassert>
#include <array>

//
// HEAVY WARNING: THIS IS INCOMPLETE AND VERY PROVISIONAL.
//
// You'll notice lots of random bits of debugging code sitting around but commented out.
// Most of this will go when this machine is complete. Please look past the gross ugliness
// of this code's intermediate state if you are able.
//

namespace {

constexpr int CLOCK_RATE = 14'318'180;

// This is the first result that came up when searching for valid Apple IIgs BRAM states;
// I'm unclear on its provenance.
constexpr uint8_t default_bram[] = {
	0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x0d, 0x06, 0x02, 0x01, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x07, 0x06, 0x02, 0x01, 0x01, 0x00, 0x00, 0x00, 0x0f, 0x06, 0x06, 0x00, 0x05, 0x06,
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x03, 0x02, 0x02, 0x02,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
	0x07, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d,
	0x0e, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfd, 0x96, 0x57, 0x3c,
};

}

namespace Apple {
namespace IIgs {

class ConcreteMachine:
	public Activity::Source,
	public Apple::IIgs::Machine,
	public MachineTypes::AudioProducer,
	public MachineTypes::JoystickMachine,
	public MachineTypes::MappedKeyboardMachine,
	public MachineTypes::MediaTarget,
	public MachineTypes::MouseMachine,
	public MachineTypes::ScanProducer,
	public MachineTypes::TimedMachine,
	public CPU::MOS6502Esque::BusHandler<uint32_t> {

	public:
		ConcreteMachine(const Analyser::Static::AppleIIgs::Target &target, const ROMMachine::ROMFetcher &rom_fetcher) :
			m65816_(*this),
			memory_(target.model >= Analyser::Static::AppleIIgs::Target::Model::ROM03),
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
			clock_.ClockStorage::set_data(std::begin(default_bram), std::end(default_bram));

			using Target = Analyser::Static::AppleIIgs::Target;
			ROM::Name system;
			switch(target.model) {
				case Target::Model::ROM00:	system = ROM::Name::AppleIIgsROM00;	break;
				case Target::Model::ROM01:	system = ROM::Name::AppleIIgsROM01;	break;
				default:					system = ROM::Name::AppleIIgsROM03;	break;
			}
			constexpr ROM::Name characters = ROM::Name::AppleIIEnhancedECharacter;
			constexpr ROM::Name microcontroller = ROM::Name::AppleIIgsMicrocontrollerROM03;

			ROM::Request request = ROM::Request(system) && ROM::Request(characters) && ROM::Request(microcontroller);
			auto roms = rom_fetcher(request);
			if(!request.validate(roms)) {
				throw ROMMachine::Error::MissingROMs;
			}
			rom_ = roms.find(system)->second;
			video_->set_character_rom(roms.find(characters)->second);
			adb_glu_->set_microcontroller_rom(roms.find(microcontroller)->second);

			// Run only the currently-interesting self test.
//			rom_[0x36402] = 2;
//			rom_[0x36403] = 0x7c;	// ROM_CHECKSUM	[working, when hacks like this are removed]
//			rom_[0x36404] = 0x6c;

//			rom_[0x36403] = 0x82;	// MOVIRAM		[working]
//			rom_[0x36404] = 0x67;

//			rom_[0x36403] = 0x2c;	// SOFT_SW		[working]
//			rom_[0x36404] = 0x6a;

//			rom_[0x36403] = 0xe8;	// RAM_ADDR		[working]
//			rom_[0x36404] = 0x6f;

//			rom_[0x36403] = 0xc7;	// FPI_SPEED	[working]
//			rom_[0x36404] = 0x6a;

//			rom_[0x36403] = 0xd7;	// SER_TST		[broken]
//			rom_[0x36404] = 0x68;

//			rom_[0x36403] = 0xdc;	// CLOCK		[broken]
//			rom_[0x36404] = 0x6c;

//			rom_[0x36403] = 0x1b;	// BAT_RAM		[broken]
//			rom_[0x36404] = 0x6e;

//			rom_[0x36403] = 0x11;	// FDB (/ADB?)	[broken]
//			rom_[0x36404] = 0x6f;

//			rom_[0x36403] = 0x41;	// SHADOW_TST	[working]
//			rom_[0x36404] = 0x6d;

//			rom_[0x36403] = 0x09;	// CUSTOM_IRQ	[broken?]
//			rom_[0x36404] = 0x6b;

//			rom_[0x36403] = 0xf4;	// DOC_EXEC
//			rom_[0x36404] = 0x70;

//			rom_[0x36403] = 0xab;	// ECT_SEQ
//			rom_[0x36404] = 0x64;

//			rom_[0xfc146f] = rom_[0xfc1470] = 0xea;

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

			// Attach drives to the IWM.
			iwm_->set_drive(0, &drives35_[0]);
			iwm_->set_drive(1, &drives35_[1]);

			// Randomise RAM contents.
			Memory::Fuzz(ram_);

			// Prior to ROM03 there's no power-on bit.
			if(target.model != Target::Model::ROM03) {
				speed_register_ &= ~0x40;
			}

			// Sync up initial values.
			memory_.set_speed_register(speed_register_ ^ 0x80);

			insert_media(target.media);
		}

		~ConcreteMachine() {
			audio_queue_.flush();
		}

		void run_for(const Cycles cycles) override {
			m65816_.run_for(cycles);
		}

		void flush_output(int outputs) final {
			iwm_.flush();
			adb_glu_.flush();

			if(outputs & Output::Video) {
				video_.flush();
			}
			if(outputs & Output::Audio) {
				AudioUpdater updater(this);
				audio_queue_.perform();
			}
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
			const auto &region = memory_.region(address);
			bool is_1Mhz = false;

			if(operation == CPU::WDC65816::BusOperation::ReadVector && !(memory_.get_shadow_register()&0x40)) {
				// I think vector pulls always go to ROM?
				// That's slightly implied in the documentation, and doing so makes GS/OS boot, so...
				// TODO: but is my guess above re: not doing that if IOLC shadowing is disabled correct?
				assert(address <= 0xffff && address >= 0xffe4);
				*value = rom_[rom_.size() - 65536 + address];
			} else if(region.flags & MemoryMap::Region::IsIO) {
				// Ensure classic auxiliary and language card accesses have effect.
				const bool is_read = isReadOperation(operation);
				memory_.access(uint16_t(address), is_read);

				const auto address_suffix = address & 0xffff;
				assert(address_suffix >= 0xc000 && address_suffix < 0xd000);
#define ReadWrite(x)	(x) | (is_read * 0x10000)
#define Read(x)			(x) | 0x10000
#define Write(x)		(x)
				switch(ReadWrite(address_suffix)) {

					// New video register.
					case Read(0xc029):
						*value = video_->get_new_video();
					break;
					case Write(0xc029):
						video_->set_new_video(*value);
						assert(*value & 1);

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
					case Read(0xc032):
						// TODO: this seems to be undocumented, but used. What value is likely?
						*value = 0xff;
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
						*value = (clock_.get_control() & 0xf0) | (video_.last_valid()->get_border_colour() & 0x0f);
					break;
					case Write(0xc034):
						clock_.set_control(*value);
						video_->set_border_colour(*value);
					break;

					// Colour text control.
					case Write(0xc022):
						video_->set_text_colour(*value);
					break;
					case Read(0xc022):
						*value = video_.last_valid()->get_text_colour();
					break;

					// Speed register.
					case Read(0xc036):
						*value = speed_register_ ^ 0x80;
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
						speed_register_ = *value ^ 0x80;
					break;

					// [Memory] State register.
					case Read(0xc068):
						*value = memory_.get_state_register();
					break;
					case Write(0xc068):
						memory_.set_state_register(*value);
						video_->set_page2(*value & 0x40);
					break;

					case Read(0xc069):
					case Write(0xc069):
						// Swallow silently; often hit as a side effect of a 16-bit write to 0xc068.
					break;

					// Various independent memory switch reads [TODO: does the IIe-style keyboard provide the low seven?].
#define SwitchRead(s) *value = memory_.s ? 0x80 : 0x00; is_1Mhz = true;
#define LanguageRead(s) SwitchRead(language_card_switches().state().s)
#define AuxiliaryRead(s) SwitchRead(auxiliary_switches().switches().s)
#define VideoRead(s) *value = video_.last_valid()->s ? 0x80 : 0x00; is_1Mhz = true;
					case Read(0xc011):	LanguageRead(bank2);						break;
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
#undef VideoRead
#undef AuxiliaryRead
#undef LanguageRead
#undef SwitchRead

					// Video switches (and annunciators).
					case Read(0xc050): case Read(0xc051):
					case Write(0xc050): case Write(0xc051):
						video_->set_text(address & 1);
						is_1Mhz = true;
					break;
					case Read(0xc052): case Read(0xc053):
					case Write(0xc052): case Write(0xc053):
						video_->set_mixed(address & 1);
						is_1Mhz = true;
					break;
					case Read(0xc054): case Read(0xc055):
					case Write(0xc054): case Write(0xc055):
						video_->set_page2(address & 1);
						is_1Mhz = true;
					break;
					case Read(0xc056): case Read(0xc057):
					case Write(0xc056): case Write(0xc057):
						video_->set_high_resolution(address&1);
						is_1Mhz = true;
					break;
					case Read(0xc058): case Read(0xc059):
					case Write(0xc058): case Write(0xc059):
					case Read(0xc05a): case Read(0xc05b):
					case Write(0xc05a): case Write(0xc05b):
					case Read(0xc05c): case Read(0xc05d):
					case Write(0xc05c): case Write(0xc05d):
						// Annunciators 0, 1 and 2.
						is_1Mhz = true;
					break;
					case Read(0xc05e): case Read(0xc05f):
					case Write(0xc05e): case Write(0xc05f):
						video_->set_annunciator_3(!(address&1));
						is_1Mhz = true;
					break;
					case Write(0xc000): case Write(0xc001):
						video_->set_80_store(address & 1);
						is_1Mhz = true;
					break;
					case Write(0xc00c): case Write(0xc00d):
						video_->set_80_columns(address & 1);
						is_1Mhz = true;
					break;
					case Write(0xc00e): case Write(0xc00f):
						video_->set_alternative_character_set(address & 1);
						is_1Mhz = true;
					break;

					// ADB and keyboard.
					case Read(0xc000):
						*value = adb_glu_->get_keyboard_data();
					break;
					case Read(0xc010):
						*value = adb_glu_->get_any_key_down() ? 0x80 : 0x00;
						[[fallthrough]];
					case Write(0xc010):
						adb_glu_->clear_key_strobe();
					break;

					case Read(0xc024):
						*value = adb_glu_->get_mouse_data();
					break;
					case Read(0xc025):
						*value = adb_glu_->get_modifier_status();
					break;
					case Read(0xc026):
						*value = adb_glu_->get_data();
					break;
					case Write(0xc026):
						adb_glu_->set_command(*value);
					break;
					case Read(0xc027):
						*value = adb_glu_->get_status();
					break;
					case Write(0xc027):
						adb_glu_->set_status(*value);
					break;

					// The SCC.
					case Read(0xc038): case Read(0xc039): case Read(0xc03a): case Read(0xc03b):
						*value = scc_.read(int(address));
					break;
					case Write(0xc038): case Write(0xc039): case Write(0xc03a): case Write(0xc03b):
						scc_.write(int(address), *value);
					break;

					// The audio GLU.
					case Read(0xc03c): {
						AudioUpdater updater(this);
						*value = sound_glu_.get_control();
					} break;
					case Write(0xc03c): {
						AudioUpdater updater(this);
						sound_glu_.set_control(*value);
					} break;
					case Read(0xc03d): {
						AudioUpdater updater(this);
						*value = sound_glu_.get_data();
					} break;
					case Write(0xc03d): {
						AudioUpdater updater(this);
						sound_glu_.set_data(*value);
					} break;
					case Read(0xc03e): {
						AudioUpdater updater(this);
						*value = sound_glu_.get_address_low();
					} break;
					case Write(0xc03e): {
						AudioUpdater updater(this);
						sound_glu_.set_address_low(*value);
					} break;
					case Read(0xc03f): {
						AudioUpdater updater(this);
						*value = sound_glu_.get_address_high();
					} break;
					case Write(0xc03f): {
						AudioUpdater updater(this);
						sound_glu_.set_address_high(*value);
					} break;


					// These were all dealt with by the call to memory_.access.
					// TODO: subject to read data? Does vapour lock apply?
					case Read(0xc002): case Read(0xc003): case Read(0xc004): case Read(0xc005):
					case Read(0xc006): case Read(0xc007): case Read(0xc008): case Read(0xc009): case Read(0xc00a): case Read(0xc00b):
						*value = 0xff;
					break;
					case Write(0xc002): case Write(0xc003): case Write(0xc004): case Write(0xc005):
					case Write(0xc006): case Write(0xc007): case Write(0xc008): case Write(0xc009): case Write(0xc00a): case Write(0xc00b):
					break;

					// Interrupt ROM addresses; Cf. P25 of the Hardware Reference.
					case Read(0xc071): case Read(0xc072): case Read(0xc073):
					case Read(0xc074): case Read(0xc075): case Read(0xc076): case Read(0xc077):
					case Read(0xc078): case Read(0xc079): case Read(0xc07a): case Read(0xc07b):
					case Read(0xc07c): case Read(0xc07d): case Read(0xc07e): case Read(0xc07f):
						*value = rom_[rom_.size() - 65536 + address_suffix];
					break;

					// Analogue inputs.
					case Read(0xc061):
						*value = (adb_glu_->get_command_button() || joysticks_.button(0)) ? 0x80 : 0x00;
						is_1Mhz = true;
					break;

					case Read(0xc062):
						*value = (adb_glu_->get_option_button() || joysticks_.button(1)) ? 0x80 : 0x00;
						is_1Mhz = true;
					break;

					case Read(0xc063):
						*value = joysticks_.button(2) ? 0x80 : 0x00;
						is_1Mhz = true;
					break;

					case Read(0xc064):
					case Read(0xc065):
					case Read(0xc066):
					case Read(0xc067): {
						// Analogue inputs.
						const size_t input = address_suffix - 0xc064;
						*value = joysticks_.analogue_channel_is_discharged(input) ? 0x00 : 0x80;
						is_1Mhz = true;
					} break;

					case Read(0xc070): case Write(0xc070):
						joysticks_.access_c070();
						is_1Mhz = true;
					break;

					// Monochome/colour register.
					case Read(0xc021):
						// "Uses bit 7 to determine whether composite output is colour 9) or gray scale (1)."
						*value = video_.last_valid()->get_composite_is_colour() ? 0x00 : 0x80;
					break;
					case Write(0xc021):
						video_->set_composite_is_colour(!(*value & 0x80));
					break;

					case Read(0xc02e):
						*value = video_.last_valid()->get_vertical_counter(video_.time_since_flush());
						is_1Mhz = true;
					break;
					case Read(0xc02f):
						*value = video_.last_valid()->get_horizontal_counter(video_.time_since_flush());
						is_1Mhz = true;
					break;

					// C037 seems to be just a full-speed storage register.
					case Read(0xc037):
						*value = c037_;
					break;
					case Write(0xc037):
						c037_ = *value;
					break;

					case Read(0xc041):
						*value = megaii_interrupt_mask_;
						is_1Mhz = true;
					break;
					case Write(0xc041):
						megaii_interrupt_mask_ = *value;
						video_->set_megaii_interrupts_enabled(*value);
						is_1Mhz = true;
					break;
					case Read(0xc044):
						// MMDELTAX byte.
						*value = 0;
						is_1Mhz = true;
					break;
					case Read(0xc045):
						// MMDELTAY byte.
						*value = 0;
						is_1Mhz = true;
					break;
					case Read(0xc046):
						*value = video_->get_megaii_interrupt_status();
						is_1Mhz = true;
					break;
					case Read(0xc047): case Write(0xc047):
						video_->clear_megaii_interrupts();
						is_1Mhz = true;
					break;
					case Read(0xc048): case Write(0xc048):
						// No-op: Clear Mega II mouse interrupt flags
						is_1Mhz = true;
					break;

					// Language select.
					// b7, b6, b5: character generator language select;
					// b4: NTSC/PAL (0 = NTC);
					// b3: language select — primary or secondary.
					case Read(0xc02b):
						*value = language_;
					break;
					case Write(0xc02b):
						language_ = *value;
					break;

					// TODO: 0xc02c is "Addr for tst mode read of character ROM". So it reads... what?

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

					case Read(0xc030): case Write(0xc030): {
						AudioUpdater updater(this);
						audio_toggle_.set_output(!audio_toggle_.get_output());
					} break;

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
					case Read(0xc020): case Write(0xc020):	// Reserved for future system expansion.
					case Read(0xc028): case Write(0xc028):	// ROMBANK; "not used in Apple IIGS".
					case Read(0xc02a): case Write(0xc02a):	// Reserved for future system expansion.
					case Read(0xc040): case Write(0xc040):	// Reserved for future system expansion.
					case Read(0xc042): case Write(0xc042):	// Reserved for future system expansion.
					case Read(0xc043): case Write(0xc043):	// Reserved for future system expansion.
					case Read(0xc049): case Write(0xc049):	// Reserved for future system expansion.
					case Read(0xc04a): case Write(0xc04a):	// Reserved for future system expansion.
					case Read(0xc04b): case Write(0xc04b):	// Reserved for future system expansion.
					case Read(0xc04c): case Write(0xc04c):	// Reserved for future system expansion.
					case Read(0xc04d): case Write(0xc04d):	// Reserved for future system expansion.
					case Read(0xc04e): case Write(0xc04e):	// Reserved for future system expansion.
					case Read(0xc04f): case Write(0xc04f):	// Reserved for future system expansion.
					case Read(0xc06b): case Write(0xc06b):	// Reserved for future system expansion.
					case Read(0xc06c): case Write(0xc06c):	// Reserved for future system expansion.
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
								assert(operation != CPU::WDC65816::BusOperation::ReadOpcode);
								if(is_read) {
									*value = 0xff;
								}
							} else {
								switch(address_suffix) {
									default:
										// Temporary: log _potential_ mistakes.
										if((address_suffix < 0xc100 && address_suffix >= 0xc090) || (address_suffix < 0xc080)) {
											printf("Internal card-area access: %04x\n", address_suffix);
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
							}
						}
				}
			} else {
				// For debugging purposes; if execution heads off into an unmapped page then
				// it's pretty certain that my 65816 still has issues.
				assert(operation != CPU::WDC65816::BusOperation::ReadOpcode || region.read);
				is_1Mhz = region.flags & MemoryMap::Region::Is1Mhz;

				if(isReadOperation(operation)) {
					*value = memory_.read(region, address);
				} else {
					// Shadowed writes also occur "at 1Mhz".
					// TODO: this is probably an approximation. I'm assuming that there's the ability asynchronously to post
					// both a 1Mhz cycle and a 2.8Mhz cycle and since the latter always fits into the former, this is sufficiently
					// descriptive. I suspect this isn't true as it wouldn't explain the speed boost that Wolfenstein and others
					// get by adding periodic NOPs within their copy-to-shadow step.
					//
					// Maybe the interaction with 2.8Mhz refresh isn't as straightforward as I think?
					const bool is_shadowed = memory_.is_shadowed(region, address);
					is_1Mhz |= is_shadowed;

					// Use a very broad test for flushing video: any write to $e0 or $e1, or any write that is shadowed.
					// TODO: at least restrict the e0/e1 test to possible video buffers!
					if((address >= 0xe0'0400 && address < 0xe1'a000) || is_shadowed) {
						video_.flush();
					}

					memory_.write(region, address, *value);
				}
			}

			Cycles duration;

			// In preparation for this test: the top bit of speed_register_ has been inverted,
			// so 1 => 1Mhz, 0 => 2.8Mhz, and motor_flags_ always has that bit set.
			if(is_1Mhz || (speed_register_ & motor_flags_)) {
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

			video_ += duration;
			iwm_ += duration;
			cycles_since_audio_update_ += duration;
			adb_glu_ += duration;

			if(cycles_since_audio_update_ >= cycles_until_audio_event_) {
				AudioUpdater updater(this);
				update_interrupts();
			}
			if(video_.did_flush()) {
				update_interrupts();

				const bool is_vertical_blank = video_.last_valid()->get_is_vertical_blank(video_.time_since_flush());
				if(is_vertical_blank != adb_glu_.last_valid()->get_vertical_blank()) {
					adb_glu_->set_vertical_blank(is_vertical_blank);
				}
			}

			joysticks_.update_charge(duration.as<float>() / 14.0f);

			return duration;
		}

		void update_interrupts() {
			// Update the interrupt line.
			// TODO: add ADB controller as event source.
			m65816_.set_irq_line(video_.last_valid()->get_interrupt_line() || sound_glu_.get_interrupt_line());
		}

		// MARK: - Input.
		KeyboardMapper *get_keyboard_mapper() final {
			return &keyboard_mapper_;
		}

		void set_key_state(uint16_t key, bool is_pressed) final {
			adb_glu_.last_valid()->keyboard().set_key_pressed(Apple::ADB::Key(key), is_pressed);
		}

		void clear_all_keys() final {
			adb_glu_.last_valid()->keyboard().clear_all_keys();
		}

		Inputs::Mouse &get_mouse() final {
			return adb_glu_.last_valid()->get_mouse();
		}

		const std::vector<std::unique_ptr<Inputs::Joystick>> &get_joysticks() final {
			return joysticks_.get_joysticks();
		}

	private:
		CPU::WDC65816::Processor<ConcreteMachine, false> m65816_;
		MemoryMap memory_;

		// MARK: - Timing.

		int fast_access_phase_ = 0;
		int slow_access_phase_ = 0;

		uint8_t speed_register_ = 0x40;	// i.e. Power-on status. (TODO: only if ROM03?)
		uint8_t motor_flags_ = 0x80;

		// MARK: - Memory storage.

		std::vector<uint8_t> ram_;
		std::vector<uint8_t> rom_;
		uint8_t c037_ = 0;

		// MARK: - Other components.

		Apple::Clock::ParallelClock clock_;
		JustInTimeActor<Apple::IIgs::Video::Video, Cycles, 1, 2> video_;	// i.e. run video at 7Mhz.
		JustInTimeActor<Apple::IIgs::ADB::GLU, Cycles, 1, 4> adb_glu_;		// i.e. 3,579,545Mhz.
		Zilog::SCC::z8530 scc_;
		JustInTimeActor<Apple::IWM, Cycles, 1, 2> iwm_;
		Cycles cycles_since_clock_tick_;
		Apple::Macintosh::DoubleDensityDrive drives35_[2];
		Apple::Disk::DiskIIDrive drives525_[2];

		// The audio parts.
		Concurrency::AsyncTaskQueue<false> audio_queue_;
		Apple::IIgs::Sound::GLU sound_glu_;
		Audio::Toggle audio_toggle_;
		using AudioSource = Outputs::Speaker::CompoundSource<Apple::IIgs::Sound::GLU, Audio::Toggle>;
		AudioSource mixer_;
		Outputs::Speaker::PullLowpass<AudioSource> speaker_;
		Cycles cycles_since_audio_update_;
		Cycles cycles_until_audio_event_;
		static constexpr int audio_divider = 16;
		void update_audio() {
			const auto divided_cycles = cycles_since_audio_update_.divide(Cycles(audio_divider));
			sound_glu_.run_for(divided_cycles);
			speaker_.run_for(audio_queue_, divided_cycles);
		}
		class AudioUpdater {
			public:
				AudioUpdater(ConcreteMachine *machine) : machine_(machine) {
					machine_->update_audio();
				}
				~AudioUpdater() {
					machine_->cycles_until_audio_event_ = machine_->sound_glu_.next_sequence_point();
				}
			private:
				ConcreteMachine *machine_;
		};
		friend AudioUpdater;

		// MARK: - Keyboard and joystick.
		Apple::ADB::KeyboardMapper keyboard_mapper_;
		Apple::II::JoystickPair joysticks_;

		// MARK: - Cards.

		// TODO: most of cards.
		uint8_t card_mask_ = 0x00;

		bool test_mode_ = false;
		uint8_t language_ = 0;
		uint8_t disk_select_ = 0;

		uint8_t megaii_interrupt_mask_ = 0;
};

}
}

using namespace Apple::IIgs;

std::unique_ptr<Machine> Machine::AppleIIgs(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	return std::make_unique<ConcreteMachine>(*dynamic_cast<const Analyser::Static::AppleIIgs::Target *>(target), rom_fetcher);
}

Machine::~Machine() {}

