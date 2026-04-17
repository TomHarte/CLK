//
//  MO.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/03/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "MO.hpp"

#include "CD90-640.hpp"
#include "Keyboard.hpp"
#include "MemoryMap.hpp"
#include "Video.hpp"

#include "Activity/Source.hpp"
#include "Machines/MachineTypes.hpp"
#include "Processors/6809/6809.hpp"
#include "Components/6821/6821.hpp"
#include "ClockReceiver/JustInTime.hpp"

#include "Components/AudioToggle/AudioToggle.hpp"
#include "Outputs/Speaker/Implementation/LowpassSpeaker.hpp"
#include "Outputs/Speaker/Implementation/BufferSource.hpp"
#include "Outputs/Log.hpp"

#include "Storage/Tape/Parsers/ThomsonMO.hpp"
#include "Analyser/Static/Thomson/Target.hpp"


using namespace Thomson::MO;

namespace {

using Log = Log::Logger<Log::Source::MO5>;

static constexpr int ClockRate = 1'000'000;
static constexpr uint8_t MusicExpansionMask = 63;

using Target = Analyser::Static::Thomson::MOTarget;

template <bool has_floppy, bool is_mo6>
struct ConcreteMachine:
	public Activity::Source,
	public Configurable::Device,
	public MachineTypes::AudioProducer,
	public MachineTypes::JoystickMachine,
	public MachineTypes::MappedKeyboardMachine,
	public MachineTypes::MediaChangeObserver,
	public MachineTypes::MediaTarget,
	public MachineTypes::TimedMachine,
	public MachineTypes::ScanProducer,
	public Machine,
	public Utility::TypeRecipient<Thomson::MO::Keyboard::CharacterMapper>
{
	ConcreteMachine(const Target &target, const ROMMachine::ROMFetcher &rom_fetcher) :
		Utility::TypeRecipient<Thomson::MO::Keyboard::CharacterMapper>(is_mo6 ? Thomson::MO::Keyboard::Machine::MO6 : Thomson::MO::Keyboard::Machine::MO5),
		m6809_(*this),
		system_pia_port_handler_(*this),
		system_pia_(system_pia_port_handler_),
		sound_and_game_pia_port_handler_(*this),
		sound_and_game_pia_(sound_and_game_pia_port_handler_),
		video_(memory_.video(true), memory_.video(false)),
		tape_player_(ClockRate),
		audio_(audio_queue_, MusicExpansionMask),
		speaker_(audio_),
		keyboard_mapper_(is_mo6 ? Thomson::MO::Keyboard::Machine::MO6 : Thomson::MO::Keyboard::Machine::MO5)
	{
		set_clock_rate(ClockRate);
		speaker_.set_input_rate(ClockRate);
		construct_joysticks();

		const auto BasicROM = [](const Target::Model model) {
			switch(model) {
				using enum Target::Model;

				default:		__builtin_unreachable();
				case MO5v1:		return ROM::Name::ThomsonMO5v1;
				case MO5v11:	return ROM::Name::ThomsonMO5v11;
				case MO6v1:		return ROM::Name::ThomsonMO6v1;
				case MO6v2:		return ROM::Name::ThomsonMO6v2;
				case MO6v3:		return ROM::Name::ThomsonMO6v3;
			}
		} (target.model);

		auto request = ROM::Request(BasicROM);
		if(has_floppy) {
			request = request && ROM::Request(ROM::Name::ThomsonCD90_640);
		}

		auto roms = rom_fetcher(request);
		if(!request.validate(roms)) {
			throw ROMMachine::Error::MissingROMs;
		}

		{
			auto rom = roms.find(BasicROM)->second;
			memory_.set_rom(rom);
		}

		if(has_floppy) {
			memory_.set_floppy_rom(roms.find(ROM::Name::ThomsonCD90_640)->second);
		}

		system_pia_.refresh();

		insert_media(target.media);
		if(!target.loading_command.empty()) {
			type_string(target.loading_command);
		}
	}

	~ConcreteMachine() {
		audio_queue_.lock_flush();
	}

	template <
		CPU::M6809::BusPhase bus_phase,
		CPU::M6809::LIC lic,
		CPU::M6809::ReadWrite read_write,
		CPU::M6809::BusState bus_state,
		typename AddressT
	>
	Cycles perform(
		const AddressT address,
		CPU::M6809::data_t<read_write> value
	) {
		static constexpr auto duration = CPU::M6809::duration<Cycles>(bus_phase);
		if(video_ += duration) {
			system_pia_.template set<Motorola::MC6821::Control::CB1>(video_.last_valid()->irq());
		}
		time_since_audio_update_ += duration;
		tape_player_.run_for(duration);
		if(typer_) {
			typer_->run_for(duration);
		}
		if(has_floppy) {
			fdc_.run_for(duration * 8);	// My WD1770 has a nominal clock of 8Mhz.
		}

		//
		// Complete MO5 memory map:
		//
		//	0x0000–:	video RAM, either pixels or attributes paged.
		//	0x2000–:	RAM
		//	0xa000–:	DOS ROM, if installed
		//	0xa7c0–:	memory-mapped devices
		//	0xb000–:	cartridge, if installed
		//	0xc000–:	BASIC/ROM, if no cartridge installed
		//	0xf000–:	BASIC/ROM
		//

		if constexpr (read_write != CPU::M6809::ReadWrite::NoData) {
			if(address >= 0xa7c0 && address < 0xa800) {
				const auto unmapped = [&] {
					if constexpr (CPU::M6809::is_read(read_write)) {
						value = 0xff;
						Log::info().append("Unhandled read at %04x", +address);
					} else {
						Log::info().append("Unhandled write: %02x -> %04x", +value, +address);
					}
				};

				switch(address) {
					// TODO: Is there a simpler way to use `namespace`?
					using namespace CPU::M6809;

					case 0xa7c0:	access<0xa7c0, read_write>(system_pia_, value);				break;
					case 0xa7c1:	access<0xa7c1, read_write>(system_pia_, value);				break;
					case 0xa7c2:	access<0xa7c2, read_write>(system_pia_, value);				break;
					case 0xa7c3:	access<0xa7c3, read_write>(system_pia_, value);				break;

					case 0xa7cc:	access<0xa7cc, read_write>(sound_and_game_pia_, value);		break;
					case 0xa7cd:	access<0xa7cd, read_write>(sound_and_game_pia_, value);		break;
					case 0xa7ce:	access<0xa7ce, read_write>(sound_and_game_pia_, value);		break;
					case 0xa7cf:	access<0xa7cf, read_write>(sound_and_game_pia_, value);		break;

					case 0xa7d0:	if(has_floppy) access<0xa7d0, read_write>(fdc_, value); else unmapped();	break;
					case 0xa7d1:	if(has_floppy) access<0xa7d1, read_write>(fdc_, value);	else unmapped();	break;
					case 0xa7d2:	if(has_floppy) access<0xa7d2, read_write>(fdc_, value);	else unmapped();	break;
					case 0xa7d3:	if(has_floppy) access<0xa7d3, read_write>(fdc_, value);	else unmapped();	break;
					case 0xa7d8:	if(has_floppy) access<0xa7d8, read_write>(fdc_, value);	else unmapped();	break;

					case 0xa7e4:	if(is_mo6) access<0xa7e4, read_write>(memory_, value); else unmapped();		break;

					// TODO: consolidate below.
					case 0xa7e5:
						if constexpr (is_mo6) {
							if(memory_.access_mode() == AccessMode::System) {
								access<0xa7e5, read_write>(memory_, value);
							} else {
								if constexpr (CPU::M6809::is_read(read_write)) {
									access<0xa7e5, read_write>(video_, value);
								} else {
									memory_.template write<0xa7e5>(value);
								}
							}
						} else {
							unmapped();
						}
					break;
					case 0xa7e6:
						if constexpr (is_mo6) {
							if(memory_.access_mode() == AccessMode::System) {
								access<0xa7e6, read_write>(memory_, value);
							} else {
								if constexpr (CPU::M6809::is_read(read_write)) {
									access<0xa7e6, read_write>(video_, value);
								} else {
									memory_.template write<0xa7e6>(value);
								}
							}
						} else {
							unmapped();
						}
					break;
					case 0xa7e7:
						if constexpr (CPU::M6809::is_read(read_write)) {
							if constexpr (is_mo6) {
								value = video_->vertical_state() & memory_.template read<0xa7e7>();
							} else {
								value = video_->vertical_state();
							}
						} else {
							if constexpr (is_mo6) {
								access<0xa7e7, read_write>(video_, value);
								access<0xa7e7, read_write>(memory_, value);
							} else {
								unmapped();
							}
						}
					break;

					case 0xa7da: 	if(is_mo6) access<0xa7da, read_write>(video_, value); else unmapped();	break;
					case 0xa7db:	if(is_mo6) access<0xa7db, read_write>(video_, value); else unmapped();	break;
					case 0xa7dc:	if(is_mo6) access<0xa7dc, read_write>(video_, value); else unmapped();	break;
					case 0xa7dd:
						if(is_mo6) {
							access<0xa7dd, read_write>(memory_, value);
							access<0xa7dd, read_write>(video_, value);
						} else {
							unmapped();
						}
					break;


					default:
						unmapped();
					break;
				}
			} else {
				if constexpr (CPU::M6809::is_read(read_write)) {
					value = memory_.read(address);

					if constexpr (lic == CPU::M6809::LIC::InstructionFetch) {
						// Catch RDBITS.
						if(allow_fast_tape_hack_ && address == 0xf168) {
							// Inputs:
							//
							//	M0044 = current tape polarity (complement if applicable).
							//	M0045 = byte in progress; ROL new bit into here.
							//
							// Additional output:
							//
							//	A = 00 or FF as per bit detected.
							//
							[&] {
								auto *const serialiser = tape_player_.serialiser();
								if(!serialiser) return;

								Storage::Tape::Thomson::MO::Parser parser;
								const auto dp = m6809_.registers().template reg<CPU::M6809::R8::DP>();
								auto &polarity = memory_[size_t((dp << 8) | 0x44)];
								auto &data = memory_[size_t((dp << 8) | 0x45)];

								parser.seed_level(
									polarity & 0x80 ? Storage::Tape::Pulse::Low : Storage::Tape::Pulse::High
								);

								const auto offset = serialiser->offset();
								const auto bit = parser.bit(*serialiser);
								if(!bit.has_value()) {
									serialiser->set_offset(offset);
									return;
								}

								data = uint8_t((data << 1) | uint8_t(*bit));
								if(!*bit) {
									polarity ^= 0xff;
								}
								m6809_.registers().template reg<CPU::M6809::R8::A>() = *bit ? 0xff : 0x00;

								// The parser reads up to the end of the bit. The ROM routine ends about two-thirds
								// of the way through the bit. So 'rewind' the tape a little.
								tape_player_.add_delay(Cycles(200));

								value = 0x39;	// RTS
							} ();
						}
					}
				} else {
					if(address < 40*200) {
						video_.flush();
					}

					memory_.write(address, value);
				}
			}
		}
		return Cycles(0);
	}

private:
	struct M6809Traits {
		static constexpr bool uses_mrdy = false;
		static constexpr auto pause_precision = CPU::M6809::PausePrecision::BetweenInstructions;
		using BusHandlerT = ConcreteMachine;
	};
	CPU::M6809::Processor<M6809Traits> m6809_;
	MemoryMap<is_mo6> memory_;

	friend struct SystemPIAPortHandler;
	struct SystemPIAPortHandler {
		SystemPIAPortHandler(ConcreteMachine &machine) : machine_(machine) {}

		//
		// Control lines:
		//
		//	CA1: lightpen input
		//	CA2: drive motor control output
		//	CB1: 50Hz interrupt input
		//	CB2: "Video incrustation"?
		//

		//
		// Port A:
		//
		//	b7 [input]: tape data (high = tape attached but not running)
		//	b6 [output]: tape data
		//	b1–b5: ... machine specific ...
		//	b0 [output]: lower 8kb paging
		//
		// MO5:
		//
		//	b5 [input]: light pen button
		//	b1–b3 [output]: border colour
		//
		// MO6:
		//
		//	b5 [output]: ROM page selection
		//	b4 [output]: shift lock LED
		//	b3 [output]: additional bit for keyboard line
		//	b2 [output]: "mute souris" (mute mouse? Probably related to the mouse and 6-bit DAC sharing lines.)
		//	b1 [input]: light pen button (maybe?)
		//

		//
		// Port B:
		//
		//	b7 [input]: state of selected key
		//	b4–6 [output]: keyboard column
		//	b1–3 [output]: keyboard line
		//	b0 [output]: 1-bit sound
		//

		template <Motorola::MC6821::Port port>
		uint8_t input() {
			if constexpr (port == Motorola::MC6821::Port::A) {
				return
					(machine_.tape_player_.input() ? 0x00 : 0x80);
			}

			if constexpr (port == Motorola::MC6821::Port::B) {
				return key_states_[key_] ? 0x00 : 0x80;
			}

			__builtin_unreachable();
		}

		template <Motorola::MC6821::Port port>
		void output(const uint8_t value) {
			if constexpr (port == Motorola::MC6821::Port::A) {
				machine_.memory_.page_video(value & 0x01);
				machine_.memory_.page_monitor(value & 0x20);

				if constexpr (!is_mo6) {
					machine_.video_->set_border_colour((value >> 1) & 0xf);
				} else {
					// TODO: incorporate extra bit in key addressing.
					// At present this doesn't seem to match expectations, not least because the monitor seems to be
					// toggling it in and out of being an output as far as the 6821 can see. Definitely research required.
					key_ = (key_ & 0b0'111'111) | ((value << 3) & 0b1'000'000);
//					printf("Extra bit [%d]; key: %02x\n", value & 0x8, key_);

					if(machine_.activity_observer_) {
						machine_.activity_observer_->set_led_status(machine_.ShiftLED, value & 0x10);
					}
				}
			}

			if constexpr (port == Motorola::MC6821::Port::B) {
				key_ = (key_ & 0b1'000'000) | ((value >> 1) & 0b0'111'111);
//				printf("Key: %02x\n", key_);
				machine_.set_audio(value & 1, std::nullopt);
			}
		}

		template <Motorola::MC6821::IRQ irq>
		void set(const bool active) {
			if constexpr (irq == Motorola::MC6821::IRQ::A) {
				machine_.m6809_.template set<CPU::M6809::Line::FIRQ>(active);
			}

			if constexpr (irq == Motorola::MC6821::IRQ::B) {
				machine_.m6809_.template set<CPU::M6809::Line::IRQ>(active);
			}
		}

		template <Motorola::MC6821::Control control>
		void observe(const bool value) {
			if constexpr (control == Motorola::MC6821::Control::CA2) {
				machine_.tape_player_.set_motor_control(!value);
			}
			if constexpr (control == Motorola::MC6821::Control::CB2) {
				Log::info().append("Video encrustation (?) is %d", value);
			}
		}

		void clear_all_keys() {
			std::fill(std::begin(key_states_), std::end(key_states_), false);
		}

		void set_key_state(const uint16_t key, const bool is_pressed) {
			key_states_[key] = is_pressed;
		}

	private:
		ConcreteMachine &machine_;
		uint8_t key_ = 0;
		bool key_states_[0x80]{};	// There are 64 key addresses on an MO5; 128 on an MO6.
	};
	SystemPIAPortHandler system_pia_port_handler_;
	Motorola::MC6821::MC6821<SystemPIAPortHandler, 2, 1> system_pia_;

	friend struct SoundAndGamePIAPortHandler;
	struct SoundAndGamePIAPortHandler {
		SoundAndGamePIAPortHandler(ConcreteMachine &machine) : machine_(machine) {}

		template <Motorola::MC6821::Control control>
		void observe(const bool) {}

		template <Motorola::MC6821::Port port>
		void output(const uint8_t value) {
			// Port B:
			//
			//	b0–b5: DAC output
			if constexpr (port == Motorola::MC6821::Port::B) {
				machine_.set_audio(std::nullopt, value & MusicExpansionMask);
			}
		}

		template <Motorola::MC6821::IRQ irq>
		void set(const bool) {}

		template <Motorola::MC6821::Port port>
		uint8_t input() {
			// Port A:
			//
			//	b0: joystick 0 up / left mouse click
			//	b1: joystick 0 down / right mouse click
			//	b2: joystick 0 left / XB
			//	b3: joystick 0 right / YB
			//	b4: joystick 1 up
			//	b5: joystick 1 down
			//	b6: joystick 1 left
			//	b7: joystick 1 right
			if constexpr (port == Motorola::MC6821::Port::A) {
				return port_a_;
			}

			// Port B:
			//
			//	b0: common 0
			//	b1: common 1
			//	b2: joystick 0 button 2 / XA
			//	b3: joystick 1 button 2
			//	b4:
			//	b5:
			//	b6: joystick 0 button 1 / YA
			//	b7: joystick 1 button 1
			if constexpr (port == Motorola::MC6821::Port::B) {
				return port_b_;
			}

			return 0xff;
		}

		// Control ports: (CE/CF?)
		//
		//	CA1: joystick 0 button 2
		//	CA2: joystick 0 button 1
		//	CB1: joystick 1 button 2
		//	CB2: joystick 1 button 1

		void set_joystick_input(
			const int index,
			const Inputs::Joystick::Input &input,
			bool is_active
		) {
			const auto apply = [&](uint8_t &port, const uint8_t mask) {
				if(is_active) {
					port &= ~mask;
				} else {
					port |= mask;
				}
			};

			switch(input.type) {
				using enum Inputs::Joystick::Input::Type;

				case Up:	apply(port_a_, index ? 0x10 : 0x01);	break;
				case Down:	apply(port_a_, index ? 0x20 : 0x02);	break;
				case Left:	apply(port_a_, index ? 0x40 : 0x04);	break;
				case Right:	apply(port_a_, index ? 0x80 : 0x08);	break;

				case Fire:
					if(input.info.control.index) {
						apply(port_b_, index ? 0x08 : 0x04);

						if(index) {
							machine_.sound_and_game_pia_.template set<Motorola::MC6821::Control::CB1>(is_active);
						} else {
							machine_.sound_and_game_pia_.template set<Motorola::MC6821::Control::CA1>(is_active);
						}
					} else {
						apply(port_b_, index ? 0x80 : 0x40);

						if(index) {
							machine_.sound_and_game_pia_.template set<Motorola::MC6821::Control::CB2>(is_active);
						} else {
							machine_.sound_and_game_pia_.template set<Motorola::MC6821::Control::CA2>(is_active);
						}
					}
				break;

				default: break;
			}
		}

	private:
		ConcreteMachine &machine_;
		uint8_t port_a_ = 0xff;
		uint8_t port_b_ = 0xff;
	};
	struct Joystick: public Inputs::ConcreteJoystick {
		Joystick(SoundAndGamePIAPortHandler &pia_port_handler, const int index) :
			ConcreteJoystick({
				Input(Input::Up),
				Input(Input::Down),
				Input(Input::Left),
				Input(Input::Right),
				Input(Input::Fire, 0),
				Input(Input::Fire, 1),
			}),
			pia_port_handler_(pia_port_handler),
			index_(index) {}

		void did_set_input(const Input &digital_input, bool is_active) final {
			pia_port_handler_.set_joystick_input(index_, digital_input, is_active);
		}

	private:
		SoundAndGamePIAPortHandler &pia_port_handler_;
		int index_;
	};

	SoundAndGamePIAPortHandler sound_and_game_pia_port_handler_;
	Motorola::MC6821::MC6821<SoundAndGamePIAPortHandler, 2, 1> sound_and_game_pia_;

	JustInTimeActor<Thomson::Video, Cycles> video_;

	// MARK: - Tape and disk.

	Storage::Tape::BinaryTapePlayer tape_player_;
	bool allow_fast_tape_hack_ = true;

	Thomson::CD90_640 fdc_;

	// MARK: - AudioProducer.

	Concurrency::AsyncTaskQueue<false> audio_queue_;
	Audio::DAC audio_;
	Outputs::Speaker::PullLowpass<Audio::DAC> speaker_;

	Cycles time_since_audio_update_;
	void update_audio() {
		const auto cycles = time_since_audio_update_.flush();
		speaker_.run_for(audio_queue_, cycles);
	}

	Outputs::Speaker::Speaker *get_speaker() override {
		return &speaker_;
	}

	bool audio_enabled_ = false;
	uint8_t audio_level_ = MusicExpansionMask;
	void set_audio(const std::optional<bool> enabled, const std::optional<uint8_t> level) {
		const auto new_audio_enabled = enabled.value_or(audio_enabled_);
		const auto new_audio_level = level.value_or(audio_level_);
		if(new_audio_level == audio_level_ && new_audio_enabled == audio_enabled_) {
			return;
		}

		audio_level_ = new_audio_level;
		audio_enabled_ = new_audio_enabled;
		update_audio();
		audio_.set_output(audio_enabled_ ? audio_level_ : 0);
	}

	// MARK: - ScanProducer.

	void set_scan_target(Outputs::Display::ScanTarget *const target) final {
		video_->set_scan_target(target);
	}

	Outputs::Display::ScanStatus get_scaled_scan_status() const final {
		return video_.last_valid()->get_scaled_scan_status();
	}

	// MARK: - TimedMachine.

	void run_for(const Cycles cycles) final {
		m6809_.run_for(cycles);
	}
	void flush_output(int outputs) final {
		if(outputs & Output::Video) {
			video_.flush();
		}
		if(outputs & Output::Audio) {
			update_audio();
			audio_queue_.perform();
		}
	}

	// MARK: - MappedKeyboardMachine.

	Thomson::MO::Keyboard::KeyboardMapper keyboard_mapper_;
	KeyboardMapper *keyboard_mapper() final {
		return &keyboard_mapper_;
	}

	void set_key_state(const uint16_t key, const bool is_pressed) final {
		system_pia_port_handler_.set_key_state(key, is_pressed);
	}

	void clear_all_keys() final {
		system_pia_port_handler_.clear_all_keys();
	}

	void type_string(const std::wstring &string) final {
		Utility::TypeRecipient<Thomson::MO::Keyboard::CharacterMapper>::add_typer(string);
	}

	bool can_type(const wchar_t c) const final {
		return Utility::TypeRecipient<Thomson::MO::Keyboard::CharacterMapper>::can_type(c);
	}

	HalfCycles typer_delay(const std::wstring &) const final {
		if(m6809_.template get<CPU::M6809::Line::PowerOnReset>()) {
			return Cycles(1'000'000);
		} else {
			return Cycles(0);
		}
	}

	HalfCycles typer_frequency() const final {
		return Cycles(20'000);
	}

	// MARK: - MediaTarget and MediaChangeObserver.

	bool insert_media(const Analyser::Static::Media &media) override {
		if(!media.tapes.empty()) {
			tape_player_.set_tape(media.tapes.front(), TargetPlatform::ThomsonMO);
		}

		if(!media.cartridges.empty()) {
			auto rom = media.cartridges.front()->segments().front().data;
			if(rom.size() < 16384) {
				rom.resize(16384);
			}
			memory_.set_cartridge(rom);
		}

		if(has_floppy) {
			size_t index = 0;
			for(auto &disk: media.disks) {
				fdc_.set_disk(disk, index);
				++ index;
			}
		}

		return !media.tapes.empty() || (!media.disks.empty() && has_floppy) || !media.cartridges.empty();
	}

	ChangeEffect effect_for_file_did_change(const std::string &) const override {
		return ChangeEffect::RestartMachine;
	}

	// MARK: - Activity Source.

	void set_activity_observer(Activity::Observer *const observer) override {
		if(is_mo6) {
			activity_observer_ = observer;
			activity_observer_->register_led(ShiftLED, Activity::Observer::LEDPresentation::Persistent);
		}
		tape_player_.set_activity_observer(observer);
		if(has_floppy) {
			fdc_.set_activity_observer(observer);
		}
	}
	static constexpr std::string ShiftLED = "Shift Lock";
	Activity::Observer *activity_observer_ = nullptr;

	// MARK: - Configuration options.

	std::unique_ptr<Reflection::Struct> get_options() const final {
		auto options = std::make_unique<Options>(Configurable::OptionsType::UserFriendly);
		options->quick_load = allow_fast_tape_hack_;
		return options;
	}

	void set_options(const std::unique_ptr<Reflection::Struct> &str) final {
		const auto options = dynamic_cast<Options *>(str.get());
		allow_fast_tape_hack_ = options->quick_load;
	}

	// MARK: - Joystick Machine.

	const std::vector<std::unique_ptr<Inputs::Joystick>> &get_joysticks() override {
		return joysticks_;
	}

	std::vector<std::unique_ptr<Inputs::Joystick>> joysticks_;
	void construct_joysticks() {
		joysticks_.push_back(std::make_unique<Joystick>(sound_and_game_pia_port_handler_, 0));
		joysticks_.push_back(std::make_unique<Joystick>(sound_and_game_pia_port_handler_, 1));
	}
};

}

namespace {
template <bool is_mo6>
std::unique_ptr<Machine> machine(const Target &target, const ROMMachine::ROMFetcher &rom_fetcher) {
	switch(target.floppy) {
		using enum Target::Floppy;
		case None:		return std::make_unique<ConcreteMachine<false, is_mo6>>(target, rom_fetcher);
		case CD90_640:	return std::make_unique<ConcreteMachine<true, is_mo6>>(target, rom_fetcher);
	}
}
}

std::unique_ptr<Machine> Machine::ThomsonMO(
	const Analyser::Static::Target *target,
	const ROMMachine::ROMFetcher &rom_fetcher
) {
	const Target *const thomson_target = dynamic_cast<const Target *>(target);
	if(Analyser::Static::Thomson::is_mo6(thomson_target->model)) {
		return machine<true>(*thomson_target, rom_fetcher);
	} else {
		return machine<false>(*thomson_target, rom_fetcher);
	}
}
