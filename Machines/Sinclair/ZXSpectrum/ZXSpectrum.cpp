//
//  ZXSpectrum.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/03/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "ZXSpectrum.hpp"

#include "Video.hpp"

#define LOG_PREFIX "[Spectrum] "

#include "../../../Activity/Source.hpp"
#include "../../MachineTypes.hpp"

#include "../../../Processors/Z80/Z80.hpp"

#include "../../../Components/AudioToggle/AudioToggle.hpp"
#include "../../../Components/AY38910/AY38910.hpp"

// TODO: possibly there's a better factoring than this, but for now
// just grab the CPC's version of an FDC.
#include "../../AmstradCPC/FDC.hpp"

#include "../../../Outputs/Log.hpp"
#include "../../../Outputs/Speaker/Implementation/CompoundSource.hpp"
#include "../../../Outputs/Speaker/Implementation/LowpassSpeaker.hpp"
#include "../../../Outputs/Speaker/Implementation/SampleSource.hpp"

#include "../../../Storage/Tape/Tape.hpp"
#include "../../../Storage/Tape/Parsers/Spectrum.hpp"

#include "../../../Analyser/Static/ZXSpectrum/Target.hpp"

#include "../../Utility/MemoryFuzzer.hpp"
#include "../../Utility/Typer.hpp"

#include "../../../ClockReceiver/JustInTime.hpp"

#include "../../../Processors/Z80/State/State.hpp"

#include "../Keyboard/Keyboard.hpp"

#include <array>

namespace Sinclair {
namespace ZXSpectrum {

using Model = Analyser::Static::ZXSpectrum::Target::Model;
using CharacterMapper = Sinclair::ZX::Keyboard::CharacterMapper;

template<Model model> class ConcreteMachine:
	public Activity::Source,
 	public ClockingHint::Observer,
	public Configurable::Device,
	public CPU::Z80::BusHandler,
	public Machine,
	public MachineTypes::AudioProducer,
	public MachineTypes::MappedKeyboardMachine,
	public MachineTypes::MediaTarget,
	public MachineTypes::ScanProducer,
	public MachineTypes::TimedMachine,
	public Utility::TypeRecipient<CharacterMapper> {
	public:
		ConcreteMachine(const Analyser::Static::ZXSpectrum::Target &target, const ROMMachine::ROMFetcher &rom_fetcher) :
			Utility::TypeRecipient<CharacterMapper>(Sinclair::ZX::Keyboard::Machine::ZXSpectrum),
			z80_(*this),
			ay_(GI::AY38910::Personality::AY38910, audio_queue_),
			audio_toggle_(audio_queue_),
			mixer_(ay_, audio_toggle_),
			speaker_(mixer_),
			keyboard_(Sinclair::ZX::Keyboard::Machine::ZXSpectrum),
			keyboard_mapper_(Sinclair::ZX::Keyboard::Machine::ZXSpectrum),
			tape_player_(clock_rate() * 2),
			fdc_(clock_rate() * 2)
		{
			set_clock_rate(clock_rate());
			speaker_.set_input_rate(float(clock_rate()) / 2.0f);

			// With only the +2a and +3 currently supported, the +3 ROM is always
			// the one required.
			std::vector<ROMMachine::ROM> rom_names;
			const std::string machine = "ZXSpectrum";
			switch(model) {
				case Model::SixteenK:
				case Model::FortyEightK:
					rom_names.emplace_back(machine, "the 48kb ROM", "48.rom", 16 * 1024, 0xddee531f);
				break;

				case Model::OneTwoEightK:
					rom_names.emplace_back(machine, "the 128kb ROM", "128.rom", 32 * 1024, 0x2cbe8995);
				break;

				case Model::Plus2:
					rom_names.emplace_back(machine, "the +2 ROM", "plus2.rom", 32 * 1024, 0xe7a517dc);
				break;

				case Model::Plus2a:
				case Model::Plus3: {
					const std::initializer_list<uint32_t> crc32s = { 0x96e3c17a, 0xbe0d9ec4 };
					rom_names.emplace_back(machine, "the +2a/+3 ROM", "plus3.rom", 64 * 1024, crc32s);
				} break;
			}
			const auto roms = rom_fetcher(rom_names);
			if(!roms[0]) throw ROMMachine::Error::MissingROMs;
			memcpy(rom_.data(), roms[0]->data(), std::min(rom_.size(), roms[0]->size()));

			// Register for sleeping notifications.
			tape_player_.set_clocking_hint_observer(this);

			// Set up initial memory map.
			update_memory_map();
			set_video_address();
			Memory::Fuzz(ram_);

			// Insert media.
			insert_media(target.media);

			// Possibly depress the enter key.
			if(target.should_hold_enter) {
				// Hold it for five seconds, more or less.
				duration_to_press_enter_ = Cycles(5 * clock_rate());
				keyboard_.set_key_state(ZX::Keyboard::KeyEnter, true);
			}
		}

		~ConcreteMachine() {
			audio_queue_.flush();
		}

		static constexpr unsigned int clock_rate() {
			constexpr unsigned int OriginalClockRate = 3'500'000;
			constexpr unsigned int Plus3ClockRate = 3'546'875;	// See notes below; this is a guess.

			// Notes on timing for the +2a and +3:
			//
			// Standard PAL produces 283.7516 colour cycles per line, each line being 64µs.
			// The oft-quoted 3.5469 Mhz would seem to imply 227.0016 clock cycles per line.
			// Since those Spectrums actually produce 228 cycles per line, but software like
			// Chromatrons seems to assume a fixed phase relationship, I guess that the real
			// clock speed is whatever gives:
			//
			// 228 / [cycles per line] * 283.7516 = [an integer].
			//
			// i.e. 228 * 283.7516 = [an integer] * [cycles per line], such that cycles per line ~= 227
			// ... which would imply that 'an integer' is probably 285, i.e.
			//
			// 228 / [cycles per line] * 283.7516 = 285
			// => 227.00128 = [cycles per line]
			// => clock rate = 3.546895 Mhz?
			//
			// That is... unless I'm mistaken about the PAL colour subcarrier and it's actually 283.75,
			// which would give exactly 227 cycles/line and therefore 3.546875 Mhz.
			//
			// A real TV would be likely to accept either, I guess. But it does seem like
			// the Spectrum is a PAL machine with a fixed colour phase relationship. For
			// this emulator's world, that's a first!

			return model < Model::OneTwoEightK ? OriginalClockRate : Plus3ClockRate;
		}

		// MARK: - TimedMachine.

		void run_for(const Cycles cycles) override {
			z80_.run_for(cycles);

			// Use this very broad timing base for the automatic enter depression.
			// It's not worth polluting the main loop.
			if(duration_to_press_enter_ > Cycles(0)) {
				if(duration_to_press_enter_ < cycles) {
					duration_to_press_enter_ = Cycles(0);
					keyboard_.set_key_state(ZX::Keyboard::KeyEnter, false);
				} else {
					duration_to_press_enter_ -= cycles;
				}
			}
		}

		void flush() {
			video_.flush();
			update_audio();
			audio_queue_.perform();

			if constexpr (model == Model::Plus3) {
				fdc_.flush();
			}
		}

		// MARK: - ScanProducer.

		void set_scan_target(Outputs::Display::ScanTarget *scan_target) override {
			video_->set_scan_target(scan_target);
		}

		Outputs::Display::ScanStatus get_scaled_scan_status() const override {
			return video_->get_scaled_scan_status();
		}

		void set_display_type(Outputs::Display::DisplayType display_type) override {
			video_->set_display_type(display_type);
		}

		// MARK: - BusHandler.

		forceinline HalfCycles perform_machine_cycle(const CPU::Z80::PartialMachineCycle &cycle) {
			using PartialMachineCycle = CPU::Z80::PartialMachineCycle;

			const uint16_t address = cycle.address ? *cycle.address : 0x0000;

			// Apply contention if necessary.
			if constexpr (model >= Model::Plus2a) {
				// Model applied: the trigger for the ULA inserting a delay is the falling edge
				// of MREQ, which is always half a cycle into a read or write.
				if(
					is_contended_[address >> 14] &&
					cycle.operation >= PartialMachineCycle::ReadOpcodeStart &&
					cycle.operation <= PartialMachineCycle::WriteStart) {

					const HalfCycles delay = video_.last_valid()->access_delay(video_.time_since_flush() + HalfCycles(1));
					advance(cycle.length + delay);
					return delay;
				}
			} else {
				switch(cycle.operation) {
					default:
						advance(cycle.length);
					return HalfCycles(0);

					case CPU::Z80::PartialMachineCycle::InputStart:
					case CPU::Z80::PartialMachineCycle::OutputStart: {
						// The port address is loaded prior to IOREQ being visible; a contention
						// always occurs if it is in the $4000–$8000 range regardless of current
						// memory mapping.
						HalfCycles delay;
						HalfCycles time = video_.time_since_flush() + HalfCycles(1);

						if((address & 0xc000) == 0x4000) {
							for(int c = 0; c < ((address & 1) ? 4 : 2); c++) {
								const auto next_delay = video_.last_valid()->access_delay(time);
								delay += next_delay;
								time += next_delay + 2;
							}
						} else {
							if(!(address & 1)) {
								delay = video_.last_valid()->access_delay(time + HalfCycles(2));
							}
						}

						advance(cycle.length + delay);
						return delay;
					}

					case PartialMachineCycle::ReadOpcodeStart:
					case PartialMachineCycle::ReadStart:
					case PartialMachineCycle::WriteStart: {
						// These all start by loading the address bus, then set MREQ
						// half a cycle later.
						if(is_contended_[address >> 14]) {
							const HalfCycles delay = video_.last_valid()->access_delay(video_.time_since_flush() + HalfCycles(1));

							advance(cycle.length + delay);
							return delay;
						}
					}

					case PartialMachineCycle::Internal: {
						// Whatever's on the address bus will remain there, without IOREQ or
						// MREQ interceding, for this entire bus cycle. So apply contentions
						// all the way along.
						if(is_contended_[address >> 14]) {
							const auto half_cycles = cycle.length.as<int>();
							assert(!(half_cycles & 1));

							HalfCycles time = video_.time_since_flush() + HalfCycles(1);
							HalfCycles delay;
							for(int c = 0; c < half_cycles; c += 2) {
								const auto next_delay = video_.last_valid()->access_delay(time);
								delay += next_delay;
								time += next_delay + 2;
							}

							advance(cycle.length + delay);
							return delay;
						}
					}

					case CPU::Z80::PartialMachineCycle::Input:
					case CPU::Z80::PartialMachineCycle::Output:
					case CPU::Z80::PartialMachineCycle::Read:
					case CPU::Z80::PartialMachineCycle::Write:
					case CPU::Z80::PartialMachineCycle::ReadOpcode:
						// For these, carry on into the actual handler, below.
					break;
				}
			}

			// For all other machine cycles, model the action as happening at the end of the machine cycle;
			// that means advancing time now.
			advance(cycle.length);

			switch(cycle.operation) {
				default: break;

				case PartialMachineCycle::ReadOpcode:
					// Fast loading: ROM version.
					//
					// The below patches over part of the 'LD-BYTES' routine from the 48kb ROM.
					if(use_fast_tape_hack_ && address == 0x056b && read_pointers_[0] == &rom_[classic_rom_offset()]) {
						// Stop pressing enter, if neccessry.
						if(duration_to_press_enter_ > Cycles(0)) {
							duration_to_press_enter_ = Cycles(0);
							keyboard_.set_key_state(ZX::Keyboard::KeyEnter, false);
						}

						if(perform_rom_ld_bytes_56b()) {
							*cycle.value = 0xc9; // i.e. RET.
							break;
						}
					}

				case PartialMachineCycle::Read:
					if constexpr (model == Model::SixteenK) {
						// Assumption: with nothing mapped above 0x8000 on the 16kb Spectrum,
						// read the floating bus.
						if(address >= 0x8000) {
							*cycle.value = video_->get_floating_value();
							break;
						}
					}

					*cycle.value = read_pointers_[address >> 14][address];

					if constexpr (model >= Model::Plus2a) {
						if(is_contended_[address >> 14]) {
							video_->set_last_contended_area_access(*cycle.value);
						}
					}
				break;

				case PartialMachineCycle::Write:
					// Flush video if this access modifies screen contents.
					if(is_video_[address >> 14] && (address & 0x3fff) < 6912) {
						video_.flush();
					}

					write_pointers_[address >> 14][address] = *cycle.value;

					if constexpr (model >= Model::Plus2a) {
						// Fill the floating bus buffer if this write is within the contended area.
						if(is_contended_[address >> 14]) {
							video_->set_last_contended_area_access(*cycle.value);
						}
					}
				break;

				case PartialMachineCycle::Output:
					// Test for port FE.
					if(!(address&1)) {
						update_audio();
						audio_toggle_.set_output(*cycle.value & 0x10);

						video_->set_border_colour(*cycle.value & 7);

						// b0–b2: border colour
						// b3: enable tape input (?)
						// b4: tape and speaker output
					}

					// Test for classic 128kb paging register (i.e. port 7ffd).
					if constexpr (model >= Model::OneTwoEightK) {
						if((address & 0xc002) == 0x4000) {
							port7ffd_ = *cycle.value;
							update_memory_map();

							// Set the proper video base pointer.
							set_video_address();

							// Potentially lock paging, _after_ the current
							// port values have taken effect.
							disable_paging_ |= *cycle.value & 0x20;
						}
					}

					// Test for +2a/+3 paging (i.e. port 1ffd).
					if constexpr (model >= Model::Plus2a) {
						if((address & 0xf002) == 0x1000) {
							port1ffd_ = *cycle.value;
							update_memory_map();
							update_video_base();

							if constexpr (model == Model::Plus3) {
								fdc_->set_motor_on(*cycle.value & 0x08);
							}
						}
					}

					// Route to the AY if one is fitted.
					if constexpr (model >= Model::OneTwoEightK) {
						if((address & 0xc002) == 0xc000) {
							// Select AY register.
							update_audio();
							GI::AY38910::Utility::select_register(ay_, *cycle.value);
						}

						if((address & 0xc002) == 0x8000) {
							// Write to AY register.
							update_audio();
							GI::AY38910::Utility::write_data(ay_, *cycle.value);
						}
					}

					// Check for FDC accesses.
					if constexpr (model == Model::Plus3) {
						switch(address) {
							default: break;
							case 0x3ffd: case 0x2ffd:
								fdc_->write((address >> 12) & 1, *cycle.value);
							break;
						}
					}
				break;

				case PartialMachineCycle::Input: {
					bool did_match = false;
					*cycle.value = 0xff;

					if(!(address&1)) {
						did_match = true;

						// Port FE:
						//
						// address b8+: mask of keyboard lines to select
						// result: b0–b4: mask of keys pressed
						// b6: tape input

						*cycle.value &= keyboard_.read(address);
						*cycle.value &= tape_player_.get_input() ? 0xbf : 0xff;

						// If this read is within 200 cycles of the previous,
						// count it as an adjacent hit; if 20 of those have
						// occurred then start the tape motor.
						if(use_automatic_tape_motor_control_) {
							if(cycles_since_tape_input_read_ < HalfCycles(400)) {
								++recent_tape_hits_;

								if(recent_tape_hits_ == 20) {
									tape_player_.set_motor_control(true);
								}
							} else {
								recent_tape_hits_ = 0;
							}

							cycles_since_tape_input_read_ = HalfCycles(0);
						}
					}

					if constexpr (model >= Model::OneTwoEightK) {
						if((address & 0xc002) == 0xc000) {
							did_match = true;

							// Read from AY register.
							update_audio();
							*cycle.value &= GI::AY38910::Utility::read(ay_);
						}
					}

					if constexpr (model >= Model::Plus2a) {
						// Check for a +2a/+3 floating bus read; these are particularly arcane.
						// See footnote to https://spectrumforeveryone.com/technical/memory-contention-floating-bus/
						// and, much more rigorously, http://sky.relative-path.com/zx/floating_bus.html
						if(!disable_paging_ && (address & 0xf003) == 0x0001) {
							*cycle.value &= video_->get_floating_value();
						}
					}

					if constexpr (model == Model::Plus3) {
						switch(address) {
							default: break;
							case 0x3ffd: case 0x2ffd:
								*cycle.value &= fdc_->read((address >> 12) & 1);
							break;
						}
					}

					if constexpr (model < Model::Plus2) {
						if(!did_match) {
							*cycle.value = video_->get_floating_value();
						}
					}
				} break;
			}

			return HalfCycles(0);
		}

	private:
		void advance(HalfCycles duration) {
			time_since_audio_update_ += duration;

			video_ += duration;
			if(video_.did_flush()) {
				z80_.set_interrupt_line(video_.last_valid()->get_interrupt_line(), video_.last_sequence_point_overrun());
			}

			if(!tape_player_is_sleeping_) tape_player_.run_for(duration.as_integral());

			// Update automatic tape motor control, if enabled; if it's been
			// 3 seconds since software last possibly polled the tape, stop it.
			if(use_automatic_tape_motor_control_ && cycles_since_tape_input_read_ < HalfCycles(clock_rate() * 6)) {
				cycles_since_tape_input_read_ += duration;

				if(cycles_since_tape_input_read_ >= HalfCycles(clock_rate() * 6)) {
					tape_player_.set_motor_control(false);
					recent_tape_hits_ = 0;
				}
			}

			if constexpr (model == Model::Plus3) {
				fdc_ += Cycles(duration.as_integral());
			}

			if(typer_) typer_->run_for(duration);
		}

		void type_string(const std::string &string) override {
			Utility::TypeRecipient<CharacterMapper>::add_typer(string);
		}

		bool can_type(char c) const override {
			return Utility::TypeRecipient<CharacterMapper>::can_type(c);
		}

	public:

		// MARK: - Typer.
		HalfCycles get_typer_delay(const std::string &) const override {
			return z80_.get_is_resetting() ? Cycles(7'000'000) : Cycles(0);
		}

		HalfCycles get_typer_frequency() const override{
			return Cycles(70'908);
		}

		KeyboardMapper *get_keyboard_mapper() override {
			return &keyboard_mapper_;
		}

		// MARK: - Keyboard.
		void set_key_state(uint16_t key, bool is_pressed) override {
			keyboard_.set_key_state(key, is_pressed);
		}

		void clear_all_keys() override {
			keyboard_.clear_all_keys();

			// Caveat: if holding enter synthetically, continue to do so.
			if(duration_to_press_enter_ > Cycles(0)) {
				keyboard_.set_key_state(ZX::Keyboard::KeyEnter, true);
			}
		}

		// MARK: - MediaTarget.
		bool insert_media(const Analyser::Static::Media &media) override {
			// If there are any tapes supplied, use the first of them.
			if(!media.tapes.empty()) {
				tape_player_.set_tape(media.tapes.front());
				set_use_fast_tape();
			}

			// Insert up to four disks.
			int c = 0;
			for(auto &disk : media.disks) {
				fdc_->set_disk(disk, c);
				c++;
				if(c == 4) break;
			}

			return !media.tapes.empty()  || (!media.disks.empty() && model == Model::Plus3);
		}

		// MARK: - ClockingHint::Observer.

		void set_component_prefers_clocking(ClockingHint::Source *, ClockingHint::Preference) override {
			tape_player_is_sleeping_ = tape_player_.preferred_clocking() == ClockingHint::Preference::None;
		}

		// MARK: - Tape control.

		void set_use_automatic_tape_motor_control(bool enabled) {
			use_automatic_tape_motor_control_ = enabled;
			if(!enabled) {
				tape_player_.set_motor_control(false);
			}
		}

		void set_tape_is_playing(bool is_playing) final {
			tape_player_.set_motor_control(is_playing);
		}

		bool get_tape_is_playing() final {
			return tape_player_.get_motor_control();
		}

		// MARK: - Configuration options.

		std::unique_ptr<Reflection::Struct> get_options() override {
			auto options = std::make_unique<Options>(Configurable::OptionsType::UserFriendly);	// OptionsType is arbitrary, but not optional.
			options->automatic_tape_motor_control = use_automatic_tape_motor_control_;
			options->quickload = allow_fast_tape_hack_;
			return options;
		}

		void set_options(const std::unique_ptr<Reflection::Struct> &str) override {
			const auto options = dynamic_cast<Options *>(str.get());
			set_video_signal_configurable(options->output);
			set_use_automatic_tape_motor_control(options->automatic_tape_motor_control);
			allow_fast_tape_hack_ = options->quickload;
			set_use_fast_tape();
		}

		// MARK: - AudioProducer.

		Outputs::Speaker::Speaker *get_speaker() override {
			return &speaker_;
		}

		// MARK: - Activity Source.
		void set_activity_observer(Activity::Observer *observer) override {
			if constexpr (model == Model::Plus3) fdc_->set_activity_observer(observer);
			tape_player_.set_activity_observer(observer);
		}

	private:
		CPU::Z80::Processor<ConcreteMachine, false, false> z80_;

		// MARK: - Memory.
		std::array<uint8_t, 64*1024> rom_;
		std::array<uint8_t, 128*1024> ram_;

		std::array<uint8_t, 16*1024> scratch_;
		const uint8_t *read_pointers_[4];
		uint8_t *write_pointers_[4];
		uint8_t pages_[4];
		bool is_contended_[4];
		bool is_video_[4];

		uint8_t port1ffd_ = 0;
		uint8_t port7ffd_ = 0;
		bool disable_paging_ = false;

		void update_memory_map() {
			// If paging is permanently disabled, don't react.
			if(disable_paging_) {
				return;
			}

			if(port1ffd_ & 0x01) {
				// "Special paging mode", i.e. one of four fixed
				// RAM configurations, port 7ffd doesn't matter.

				switch(port1ffd_ & 0x06) {
					default:
					case 0x00:
						set_memory(0, 0);
						set_memory(1, 1);
						set_memory(2, 2);
						set_memory(3, 3);
					break;

					case 0x02:
						set_memory(0, 4);
						set_memory(1, 5);
						set_memory(2, 6);
						set_memory(3, 7);
					break;

					case 0x04:
						set_memory(0, 4);
						set_memory(1, 5);
						set_memory(2, 6);
						set_memory(3, 3);
					break;

					case 0x06:
						set_memory(0, 4);
						set_memory(1, 7);
						set_memory(2, 6);
						set_memory(3, 3);
					break;
				}
			} else {
				// Apply standard 128kb-esque mapping (albeit with extra ROM to pick from).
				set_memory(0, 0x80 | ((port1ffd_ >> 1) & 2) | ((port7ffd_ >> 4) & 1));
				set_memory(1, 5);
				set_memory(2, 2);
				set_memory(3, port7ffd_ & 7);
			}
		}

		void set_memory(int bank, uint8_t source) {
			if constexpr (model >= Model::Plus2a) {
				is_contended_[bank] = (source >= 4 && source < 8);
			} else {
				is_contended_[bank] = source & 1;
			}
			pages_[bank] = source;

			uint8_t *const read = (source < 0x80) ? &ram_[source * 16384] : &rom_[(source & 0x7f) * 16384];
			const auto offset = bank*16384;

			read_pointers_[bank] = read - offset;
			write_pointers_[bank] = ((source < 0x80) ? read : scratch_.data()) - offset;
		}

		void set_video_address() {
			video_->set_video_source(&ram_[((port7ffd_ & 0x08) ? 7 : 5) * 16384]);
			update_video_base();
		}

		void update_video_base() {
			const uint8_t video_page = (port7ffd_ & 0x08) ? 7 : 5;
			is_video_[0] = pages_[0] == video_page;
			is_video_[1] = pages_[1] == video_page;
			is_video_[2] = pages_[2] == video_page;
			is_video_[3] = pages_[3] == video_page;
		}

		// MARK: - Audio.
		Concurrency::DeferringAsyncTaskQueue audio_queue_;
		GI::AY38910::AY38910<false> ay_;
		Audio::Toggle audio_toggle_;
		Outputs::Speaker::CompoundSource<GI::AY38910::AY38910<false>, Audio::Toggle> mixer_;
		Outputs::Speaker::LowpassSpeaker<Outputs::Speaker::CompoundSource<GI::AY38910::AY38910<false>, Audio::Toggle>> speaker_;

		HalfCycles time_since_audio_update_;
		void update_audio() {
			speaker_.run_for(audio_queue_, time_since_audio_update_.divide_cycles(Cycles(2)));
		}

		// MARK: - Video.
		using VideoType =
			std::conditional_t<
				model <= Model::FortyEightK, Video<VideoTiming::FortyEightK>,
				std::conditional_t<
					model <= Model::Plus2, Video<VideoTiming::OneTwoEightK>,
					Video<VideoTiming::Plus3>
				>
			>;
		JustInTimeActor<VideoType> video_;

		// MARK: - Keyboard.
		Sinclair::ZX::Keyboard::Keyboard keyboard_;
		Sinclair::ZX::Keyboard::KeyboardMapper keyboard_mapper_;

		// MARK: - Tape.
		Storage::Tape::BinaryTapePlayer tape_player_;
		bool tape_player_is_sleeping_ = false;

		bool use_automatic_tape_motor_control_ = true;
		HalfCycles cycles_since_tape_input_read_;
		int recent_tape_hits_ = 0;

		bool allow_fast_tape_hack_ = false;
		bool use_fast_tape_hack_ = false;
		void set_use_fast_tape() {
			use_fast_tape_hack_ = allow_fast_tape_hack_ && tape_player_.has_tape();
		}

		// Reimplements the 'LD-BYTES' routine, as documented at
		// https://skoolkid.github.io/rom/asm/0556.html but picking
		// up from address 56b i.e.
		//
		// In:
		//	A': 0x00 or 0xff for block type;
		//	F': carry set if loading, clear if verifying;
		//	DE: block length;
		//	IX: start address.
		//
		// Out:
		//	F: carry set for success, clear for error.
		//
		// And, empirically:
		//	IX: one beyond final address written;
		//	DE: 0;
		//	L: parity byte;
		//	H: 0 for no error, 0xff for error;
		//	A: same as H.
		//	BC: ???
		bool perform_rom_ld_bytes_56b() {
			using Parser = Storage::Tape::ZXSpectrum::Parser;
			Parser parser(Parser::MachineType::ZXSpectrum);

			using Register = CPU::Z80::Register;
			uint8_t flags = uint8_t(z80_.get_value_of_register(Register::FlagsDash));
			if(!(flags & 1)) return false;

			const uint8_t block_type = uint8_t(z80_.get_value_of_register(Register::ADash));
			const auto block = parser.find_block(tape_player_.get_tape());
			if(!block || block_type != (*block).type) return false;

			uint16_t length = z80_.get_value_of_register(Register::DE);
			uint16_t target = z80_.get_value_of_register(Register::IX);

			flags = 0x93;
			uint8_t parity = 0x00;
			while(length--) {
				auto next = parser.get_byte(tape_player_.get_tape());
				if(!next) {
					flags &= ~1;
					break;
				}

				write_pointers_[target >> 14][target] = *next;
				parity ^= *next;
				++target;
			}

			auto stored_parity = parser.get_byte(tape_player_.get_tape());
			if(!stored_parity) {
				flags &= ~1;
			} else {
				z80_.set_value_of_register(Register::L, *stored_parity);
			}

			z80_.set_value_of_register(Register::Flags, flags);
			z80_.set_value_of_register(Register::DE, length);
			z80_.set_value_of_register(Register::IX, target);

			const uint8_t h = (flags & 1) ? 0x00 : 0xff;
			z80_.set_value_of_register(Register::H, h);
			z80_.set_value_of_register(Register::A, h);

			return true;
		}

		static constexpr int classic_rom_offset() {
			switch(model) {
				case Model::SixteenK:
				case Model::FortyEightK:
				return 0x0000;

				case Model::OneTwoEightK:
				case Model::Plus2:
				return 0x4000;

				case Model::Plus2a:
				case Model::Plus3:
				return 0xc000;
			}
		}

		// MARK: - Disc.
		JustInTimeActor<Amstrad::FDC, Cycles> fdc_;

		// MARK: - Automatic startup.
		Cycles duration_to_press_enter_;
};


}
}

using namespace Sinclair::ZXSpectrum;

Machine *Machine::ZXSpectrum(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	const auto zx_target = dynamic_cast<const Analyser::Static::ZXSpectrum::Target *>(target);

	switch(zx_target->model) {
		case Model::SixteenK:		return new ConcreteMachine<Model::SixteenK>(*zx_target, rom_fetcher);
		case Model::FortyEightK:	return new ConcreteMachine<Model::FortyEightK>(*zx_target, rom_fetcher);
		case Model::OneTwoEightK:	return new ConcreteMachine<Model::OneTwoEightK>(*zx_target, rom_fetcher);
		case Model::Plus2:			return new ConcreteMachine<Model::Plus2>(*zx_target, rom_fetcher);
		case Model::Plus2a:			return new ConcreteMachine<Model::Plus2a>(*zx_target, rom_fetcher);
		case Model::Plus3:			return new ConcreteMachine<Model::Plus3>(*zx_target, rom_fetcher);
	}

	return nullptr;
}

Machine::~Machine() {}
