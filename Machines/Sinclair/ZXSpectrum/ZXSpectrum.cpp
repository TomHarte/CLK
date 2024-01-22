//
//  ZXSpectrum.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/03/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "ZXSpectrum.hpp"

#include "State.hpp"
#include "Video.hpp"
#include "../Keyboard/Keyboard.hpp"

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

#include <array>

namespace {

/*!
	Provides a simultaneous Kempston and Interface 2-style joystick.
*/
class Joystick: public Inputs::ConcreteJoystick {
	public:
		Joystick() :
			ConcreteJoystick({
				Input(Input::Up),
				Input(Input::Down),
				Input(Input::Left),
				Input(Input::Right),
				Input(Input::Fire)
			}) {}

		void did_set_input(const Input &digital_input, bool is_active) final {
			const auto apply_kempston = [&](uint8_t mask) {
				if(is_active) kempston_ |= mask; else kempston_ &= ~mask;
			};
			const auto apply_sinclair = [&](uint16_t mask) {
				if(is_active) sinclair_ &= ~mask; else sinclair_ |= mask;
			};

			switch(digital_input.type) {
				default: return;

				case Input::Right:
					apply_kempston(0x01);
					apply_sinclair(0x0208);
				break;
				case Input::Left:
					apply_kempston(0x02);
					apply_sinclair(0x0110);
				break;
				case Input::Down:
					apply_kempston(0x04);
					apply_sinclair(0x0404);
				break;
				case Input::Up:
					apply_kempston(0x08);
					apply_sinclair(0x0802);
				break;
				case Input::Fire:
					apply_kempston(0x10);
					apply_sinclair(0x1001);
				break;
			}
		}

		/// @returns The value that a Kempston joystick interface would report if this joystick
		/// were plugged into it.
		uint8_t get_kempston() {
			return kempston_;
		}

		/// @returns The value that a Sinclair interface would report if this joystick
		/// were plugged into it via @c port (which should be either 0 or 1, for ports 1 or 2).
		uint8_t get_sinclair(int port) {
			return uint8_t(sinclair_ >> (port * 8));
		}

	private:
		uint8_t kempston_ = 0x00;
		uint16_t sinclair_ = 0xffff;
};

}

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
	public MachineTypes::JoystickMachine,
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

			ROM::Name rom_name;
			switch(model) {
				case Model::SixteenK:
				case Model::FortyEightK:	rom_name = ROM::Name::Spectrum48k;		break;
				case Model::OneTwoEightK:	rom_name = ROM::Name::Spectrum128k;		break;
				case Model::Plus2:			rom_name = ROM::Name::SpecrumPlus2;		break;
				case Model::Plus2a:
				case Model::Plus3:			rom_name = ROM::Name::SpectrumPlus3;	break;
				// TODO: possibly accept the +3 ROM in multiple parts?
			}
			const auto request = ROM::Request(rom_name);
			auto roms = rom_fetcher(request);
			if(!request.validate(roms)) {
				throw ROMMachine::Error::MissingROMs;
			}

			const auto &rom = roms.find(rom_name)->second;
			memcpy(rom_.data(), rom.data(), std::min(rom_.size(), rom.size()));

			// Register for sleeping notifications.
			tape_player_.set_clocking_hint_observer(this);

			// Attach a couple of joysticks.
			joysticks_.emplace_back(new Joystick);
			joysticks_.emplace_back(new Joystick);

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

			// Install state if supplied.
			if(target.state) {
				const auto state = static_cast<State *>(target.state.get());
				state->z80.apply(z80_);
				state->video.apply(*video_.last_valid());
				state->ay.apply(ay_);

				// If this is a 48k or 16k machine, remap source data from its original
				// linear form to whatever the banks end up being; otherwise copy as is.
				if(model <= Model::FortyEightK) {
					const size_t num_banks = std::min(size_t(48*1024), state->ram.size()) >> 14;
					for(size_t c = 0; c < num_banks; c++) {
						memcpy(&banks_[c + 1].write[(c+1) * 0x4000], &state->ram[c * 0x4000], 0x4000);
					}
				} else {
					memcpy(ram_.data(), state->ram.data(), std::min(ram_.size(), state->ram.size()));

					port1ffd_ = state->last_1ffd;
					port7ffd_ = state->last_7ffd;
					update_memory_map();
				}
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

		void flush_output(int outputs) override {
			if(outputs & Output::Video) {
				video_.flush();
			}

			if(outputs & Output::Audio) {
				update_audio();
				audio_queue_.perform();
			}

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

		Outputs::Display::DisplayType get_display_type() const override {
			return video_->get_display_type();
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
					banks_[address >> 14].is_contended &&
					cycle.operation >= PartialMachineCycle::ReadOpcodeStart &&
					cycle.operation <= PartialMachineCycle::WriteStart) {

					const auto delay = video_.last_valid()->access_delay(video_.time_since_flush());
					advance(cycle.length + delay);
					return delay;
				}
			} else {
				switch(cycle.operation) {
					case CPU::Z80::PartialMachineCycle::Input:
					case CPU::Z80::PartialMachineCycle::Output:
					case CPU::Z80::PartialMachineCycle::Read:
					case CPU::Z80::PartialMachineCycle::Write:
					case CPU::Z80::PartialMachineCycle::ReadOpcode:
					case CPU::Z80::PartialMachineCycle::Interrupt:
						// For these, carry on into the actual handler, below.
					break;

					// For anything else that isn't listed below, just advance
					// time and conclude here.
					default:
						advance(cycle.length);
					return HalfCycles(0);

					case CPU::Z80::PartialMachineCycle::InputStart:
					case CPU::Z80::PartialMachineCycle::OutputStart: {
						// The port address is loaded prior to IOREQ being visible; a contention
						// always occurs if it is in the $4000–$8000 range regardless of current
						// memory mapping.
						HalfCycles delay;
						HalfCycles time = video_.time_since_flush();

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
					} break;

					case PartialMachineCycle::ReadOpcodeStart:
					case PartialMachineCycle::ReadStart:
					case PartialMachineCycle::WriteStart: {
						// These all start by loading the address bus, then set MREQ
						// half a cycle later.
						if(banks_[address >> 14].is_contended) {
							const auto delay = video_.last_valid()->access_delay(video_.time_since_flush());

							advance(cycle.length + delay);
							return delay;
						}
					} break;

					case PartialMachineCycle::Internal: {
						// Whatever's on the address bus will remain there, without IOREQ or
						// MREQ interceding, for this entire bus cycle. So apply contentions
						// all the way along.
						if(banks_[address >> 14].is_contended) {
							const auto half_cycles = cycle.length.as<int>();
							assert(!(half_cycles & 1));

							HalfCycles time = video_.time_since_flush();
							HalfCycles delay;
							for(int c = 0; c < half_cycles; c += 2) {
								const auto next_delay = video_.last_valid()->access_delay(time);
								delay += next_delay;
								time += next_delay + 2;
							}

							advance(cycle.length + delay);
							return delay;
						}
					} break;
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
					if(use_fast_tape_hack_ && address == 0x056b && banks_[0].read == &rom_[classic_rom_offset()]) {
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
					[[fallthrough]];

				case PartialMachineCycle::Read:
					if constexpr (model == Model::SixteenK) {
						// Assumption: with nothing mapped above 0x8000 on the 16kb Spectrum,
						// read the floating bus.
						if(address >= 0x8000) {
							*cycle.value = video_->get_floating_value();
							break;
						}
					}

					*cycle.value = banks_[address >> 14].read[address];

					if constexpr (model >= Model::Plus2a) {
						if(banks_[address >> 14].is_contended) {
							video_->set_last_contended_area_access(*cycle.value);
						}
					}
				break;

				case PartialMachineCycle::Write:
					// Flush video if this access modifies screen contents.
					if(banks_[address >> 14].is_video && (address & 0x3fff) < 6912) {
						video_.flush();
					}

					banks_[address >> 14].write[address] = *cycle.value;

					if constexpr (model >= Model::Plus2a) {
						// Fill the floating bus buffer if this write is within the contended area.
						if(banks_[address >> 14].is_contended) {
							video_->set_last_contended_area_access(*cycle.value);
						}
					}
				break;

				// Partial port decodings here and in ::Input are as documented
				// at https://worldofspectrum.org/faq/reference/ports.htm

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
					if (
						(model >= Model::OneTwoEightK && model <= Model::Plus2 && (address & 0x8002) == 0x0000) ||
						(model >= Model::Plus2a && (address & 0xc002) == 0x4000)
					) {
						port7ffd_ = *cycle.value;
						update_memory_map();

						// Set the proper video base pointer.
						set_video_address();
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
						switch(address & 0xc002) {
							case 0xc000:
								// Select AY register.
								update_audio();
								GI::AY38910::Utility::select_register(ay_, *cycle.value);
							break;

							case 0x8000:
								// Write to AY register.
								update_audio();
								GI::AY38910::Utility::write_data(ay_, *cycle.value);
							break;
						}
					}

					// Check for FDC accesses.
					if constexpr (model == Model::Plus3) {
						switch(address & 0xf002) {
							default: break;
							case 0x3000: case 0x2000:
								fdc_->write((address >> 12) & 1, *cycle.value);
							break;
						}
					}
				break;

				case PartialMachineCycle::Input: {
					[[maybe_unused]] bool did_match = false;
					*cycle.value = 0xff;

					if(!(address&32)) {
						did_match = true;
						*cycle.value &= static_cast<Joystick *>(joysticks_[0].get())->get_kempston();
					}

					if(!(address&1)) {
						did_match = true;

						// Port FE:
						//
						// address b8+: mask of keyboard lines to select
						// result: b0–b4: mask of keys pressed
						// b6: tape input

						*cycle.value &= keyboard_.read(address);
						*cycle.value &= tape_player_.get_input() ? 0xbf : 0xff;

						// Add Joystick input on top.
						if(!(address&0x1000)) *cycle.value &= static_cast<Joystick *>(joysticks_[0].get())->get_sinclair(0);
						if(!(address&0x0800)) *cycle.value &= static_cast<Joystick *>(joysticks_[1].get())->get_sinclair(1);

						// If this read is between 50 and 200 cycles since the
						// previous, count it as an adjacent hit; if 20 of those
						// have occurred then start the tape motor.
						if(use_automatic_tape_motor_control_) {
							if(cycles_since_tape_input_read_ >= HalfCycles(100) && cycles_since_tape_input_read_ < HalfCycles(200)) {
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
						switch(address & 0xf002) {
							default: break;
							case 0x3000: case 0x2000:
								*cycle.value &= fdc_->read((address >> 12) & 1);
							break;
						}
					}

					if constexpr (model <= Model::Plus2) {
						if(!did_match) {
							*cycle.value = video_->get_floating_value();
						}
					}
				} break;

				case PartialMachineCycle::Interrupt:
					// At least one piece of Spectrum software, Escape from M.O.N.J.A.S. explicitly
					// assumes that a 0xff value will be on the bus during an interrupt acknowledgment.
					// I wasn't otherwise aware that this value is reliable.
					*cycle.value = 0xff;
				break;
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
			// 0.5 seconds since software last possibly polled the tape, stop it.
			if(use_automatic_tape_motor_control_ && cycles_since_tape_input_read_ < HalfCycles(clock_rate())) {
				cycles_since_tape_input_read_ += duration;

				if(cycles_since_tape_input_read_ >= HalfCycles(clock_rate())) {
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

			return !media.tapes.empty() || (!media.disks.empty() && model == Model::Plus3);
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
			options->output = get_video_signal_configurable();
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
		struct Bank {
			const uint8_t * read;
			uint8_t *write;
			uint8_t page;
			bool is_contended;
			bool is_video;
		};
		std::array<Bank, 4> banks_;

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

			// Potentially lock paging, _after_ the current
			// port values have taken effect.
			disable_paging_ = port7ffd_ & 0x20;
		}

		void set_memory(std::size_t bank, uint8_t source) {
			if constexpr (model >= Model::Plus2a) {
				banks_[bank].is_contended = source >= 4 && source < 8;
			} else {
				banks_[bank].is_contended = source < 0x80 && source & 1;
			}
			banks_[bank].page = source;

			uint8_t *const read = (source < 0x80) ? &ram_[source * 16384] : &rom_[(source & 0x7f) * 16384];
			const auto offset = bank*16384;

			banks_[bank].read = read - offset;
			banks_[bank].write = ((source < 0x80) ? read : scratch_.data()) - offset;
		}

		void set_video_address() {
			video_->set_video_source(&ram_[((port7ffd_ & 0x08) ? 7 : 5) * 16384]);
			update_video_base();
		}

		void update_video_base() {
			const uint8_t video_page = (port7ffd_ & 0x08) ? 7 : 5;
			banks_[0].is_video = banks_[0].page == video_page;
			banks_[1].is_video = banks_[1].page == video_page;
			banks_[2].is_video = banks_[2].page == video_page;
			banks_[3].is_video = banks_[3].page == video_page;
		}

		// MARK: - Audio.
		Concurrency::AsyncTaskQueue<false> audio_queue_;
		GI::AY38910::AY38910<false> ay_;
		Audio::Toggle audio_toggle_;
		Outputs::Speaker::CompoundSource<GI::AY38910::AY38910<false>, Audio::Toggle> mixer_;
		Outputs::Speaker::PullLowpass<Outputs::Speaker::CompoundSource<GI::AY38910::AY38910<false>, Audio::Toggle>> speaker_;

		HalfCycles time_since_audio_update_;
		void update_audio() {
			speaker_.run_for(audio_queue_, time_since_audio_update_.divide_cycles(Cycles(2)));
		}

		// MARK: - Video.
		using VideoType =
			std::conditional_t<
				model <= Model::FortyEightK, Video::Video<Video::Timing::FortyEightK>,
				std::conditional_t<
					model <= Model::Plus2, Video::Video<Video::Timing::OneTwoEightK>,
					Video::Video<Video::Timing::Plus3>
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
			uint8_t flags = uint8_t(z80_.value_of(Register::FlagsDash));
			if(!(flags & 1)) return false;

			const uint8_t block_type = uint8_t(z80_.value_of(Register::ADash));
			const auto block = parser.find_block(tape_player_.get_tape());
			if(!block || block_type != (*block).type) return false;

			uint16_t length = z80_.value_of(Register::DE);
			uint16_t target = z80_.value_of(Register::IX);

			flags = 0x93;
			uint8_t parity = 0x00;
			while(length--) {
				auto next = parser.get_byte(tape_player_.get_tape());
				if(!next) {
					flags &= ~1;
					break;
				}

				banks_[target >> 14].write[target] = *next;
				parity ^= *next;
				++target;
			}

			auto stored_parity = parser.get_byte(tape_player_.get_tape());
			if(!stored_parity) {
				flags &= ~1;
			} else {
				z80_.set_value_of(Register::L, *stored_parity);
			}

			z80_.set_value_of(Register::Flags, flags);
			z80_.set_value_of(Register::DE, length);
			z80_.set_value_of(Register::IX, target);

			const uint8_t h = (flags & 1) ? 0x00 : 0xff;
			z80_.set_value_of(Register::H, h);
			z80_.set_value_of(Register::A, h);

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

		// MARK: - Joysticks
		std::vector<std::unique_ptr<Inputs::Joystick>> joysticks_;
		const std::vector<std::unique_ptr<Inputs::Joystick>> &get_joysticks() override {
			return joysticks_;
		}
};


}
}

using namespace Sinclair::ZXSpectrum;

std::unique_ptr<Machine> Machine::ZXSpectrum(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	const auto zx_target = dynamic_cast<const Analyser::Static::ZXSpectrum::Target *>(target);

	switch(zx_target->model) {
		case Model::SixteenK:		return std::make_unique<ConcreteMachine<Model::SixteenK>>(*zx_target, rom_fetcher);
		case Model::FortyEightK:	return std::make_unique<ConcreteMachine<Model::FortyEightK>>(*zx_target, rom_fetcher);
		case Model::OneTwoEightK:	return std::make_unique<ConcreteMachine<Model::OneTwoEightK>>(*zx_target, rom_fetcher);
		case Model::Plus2:			return std::make_unique<ConcreteMachine<Model::Plus2>>(*zx_target, rom_fetcher);
		case Model::Plus2a:			return std::make_unique<ConcreteMachine<Model::Plus2a>>(*zx_target, rom_fetcher);
		case Model::Plus3:			return std::make_unique<ConcreteMachine<Model::Plus3>>(*zx_target, rom_fetcher);
	}

	return nullptr;
}

Machine::~Machine() {}
