//
//  ZX8081.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "ZX8081.hpp"

#include "../../MachineTypes.hpp"

#include "../../../Components/AY38910/AY38910.hpp"
#include "../../../Processors/Z80/Z80.hpp"
#include "../../../Storage/Tape/Tape.hpp"
#include "../../../Storage/Tape/Parsers/ZX8081.hpp"

#include "../../../ClockReceiver/ForceInline.hpp"

#include "../../Utility/MemoryFuzzer.hpp"
#include "../../Utility/Typer.hpp"

#include "../../../Outputs/Speaker/Implementation/LowpassSpeaker.hpp"

#include "../../../Analyser/Static/ZX8081/Target.hpp"

#include "../Keyboard/Keyboard.hpp"
#include "Video.hpp"

#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

namespace {
	// The clock rate is 3.25Mhz.
	constexpr unsigned int ZX8081ClockRate = 3250000;
}

// TODO:
//	Quiksilva sound support:
// 	7FFFh.W		PSG index
//	7FFEh.R/W	PSG data

namespace Sinclair {
namespace ZX8081 {

enum ROMType: uint8_t {
	ZX80 = 0, ZX81
};

using CharacterMapper = Sinclair::ZX::Keyboard::CharacterMapper;

template<bool is_zx81> class ConcreteMachine:
	public MachineTypes::TimedMachine,
	public MachineTypes::ScanProducer,
	public MachineTypes::AudioProducer,
	public MachineTypes::MediaTarget,
	public MachineTypes::MappedKeyboardMachine,
	public Configurable::Device,
	public Utility::TypeRecipient<CharacterMapper>,
	public CPU::Z80::BusHandler,
	public Machine {
	public:
		ConcreteMachine(const Analyser::Static::ZX8081::Target &target, const ROMMachine::ROMFetcher &rom_fetcher) :
			Utility::TypeRecipient<CharacterMapper>(keyboard_machine()),
			z80_(*this),
			keyboard_(keyboard_machine()),
			keyboard_mapper_(keyboard_machine()),
			tape_player_(ZX8081ClockRate),
			ay_(GI::AY38910::Personality::AY38910, audio_queue_),
			speaker_(ay_) {
			set_clock_rate(ZX8081ClockRate);
			speaker_.set_input_rate(float(ZX8081ClockRate) / 2.0f);

			const bool use_zx81_rom = target.is_ZX81 || target.ZX80_uses_ZX81_ROM;
			const ROM::Name rom_name = use_zx81_rom ? ROM::Name::ZX81 : ROM::Name::ZX80;
			const ROM::Request request(rom_name);
			auto roms = rom_fetcher(request);
			if(!request.validate(roms)) {
				throw ROMMachine::Error::MissingROMs;
			}
			rom_ = std::move(roms.find(rom_name)->second);

			rom_mask_ = uint16_t(rom_.size() - 1);

			switch(target.memory_model) {
				case Analyser::Static::ZX8081::Target::MemoryModel::Unexpanded:
					ram_.resize(1024);
					ram_base_ = 16384;
					ram_mask_ = 1023;
				break;
				case Analyser::Static::ZX8081::Target::MemoryModel::SixteenKB:
					ram_.resize(16384);
					ram_base_ = 16384;
					ram_mask_ = 16383;
				break;
				case Analyser::Static::ZX8081::Target::MemoryModel::SixtyFourKB:
					ram_.resize(65536);
					ram_base_ = 8192;
					ram_mask_ = 65535;
				break;
			}
			Memory::Fuzz(ram_);

			// Ensure valid initial key state.
			clear_all_keys();
			if(!target.loading_command.empty()) {
				type_string(target.loading_command);
				should_autorun_ = true;
			}

			insert_media(target.media);
		}

		~ConcreteMachine() {
			audio_queue_.flush();
		}

		forceinline HalfCycles perform_machine_cycle(const CPU::Z80::PartialMachineCycle &cycle) {
			const HalfCycles previous_counter = horizontal_counter_;
			horizontal_counter_ += cycle.length;
			time_since_ay_update_ += cycle.length;

			if(previous_counter < vsync_start_ && horizontal_counter_ >= vsync_start_) {
				video_.run_for(vsync_start_ - previous_counter);
				set_hsync(true);
				line_counter_ = (line_counter_ + 1) & 7;
				if(nmi_is_enabled_) {
					z80_.set_non_maskable_interrupt_line(true);
				}
				video_.run_for(horizontal_counter_ - vsync_start_);
			} else if(previous_counter < vsync_end_ && horizontal_counter_ >= vsync_end_) {
				video_.run_for(vsync_end_ - previous_counter);
				set_hsync(false);
				if(nmi_is_enabled_) {
					z80_.set_non_maskable_interrupt_line(false);
					z80_.set_wait_line(false);
				}
				video_.run_for(horizontal_counter_ - vsync_end_);
			} else {
				video_.run_for(cycle.length);
			}

			if constexpr (is_zx81) horizontal_counter_ %= HalfCycles(Cycles(207));
			if(!tape_advance_delay_) {
				tape_player_.run_for(cycle.length);
			} else {
				tape_advance_delay_ = std::max(tape_advance_delay_ - cycle.length, HalfCycles(0));
			}

			if(nmi_is_enabled_ && !z80_.get_halt_line() && z80_.get_non_maskable_interrupt_line()) {
				z80_.set_wait_line(true);
			}

			if(!cycle.is_terminal()) {
				return Cycles(0);
			}

			const uint16_t address = cycle.address ? *cycle.address : 0;
			bool is_opcode_read = false;
			switch(cycle.operation) {
				case CPU::Z80::PartialMachineCycle::Output:
					if(!nmi_is_enabled_) {
						line_counter_ = 0;
						set_vsync(false);
					}
					if(!(address & 2)) nmi_is_enabled_ = false;
					if(!(address & 1)) nmi_is_enabled_ = is_zx81;
					if(is_zx81 && !nmi_is_enabled_) z80_.set_wait_line(false);

					// The below emulates the ZonX AY expansion device.
					if constexpr (is_zx81) {
						if((address&0xef) == 0xcf) {
							ay_set_register(*cycle.value);
						} else if((address&0xef) == 0x0f) {
							ay_set_data(*cycle.value);
						}
					}
				break;

				case CPU::Z80::PartialMachineCycle::Input: {
					uint8_t value = 0xff;
					if(!(address&1)) {
						if(!nmi_is_enabled_) set_vsync(true);

						value &= keyboard_.read(address);
						value &= ~(tape_player_.get_input() ? 0x00 : 0x80);
					}

					// The below emulates the ZonX AY expansion device.
					if constexpr (is_zx81) {
						if((address&0xef) == 0xcf) {
							value &= ay_read_data();
						}
					}
					*cycle.value = value;
				} break;

				case CPU::Z80::PartialMachineCycle::Interrupt:
					// resetting event is M1 and IOREQ both simultaneously having leading edges;
					// that happens 2 cycles before the end of INTACK. So the timer was reset and
					// now has advanced twice.
					horizontal_counter_ = HalfCycles(2);

					*cycle.value = 0xff;
				break;

				case CPU::Z80::PartialMachineCycle::Refresh:
					// The ZX80 and 81 signal an interrupt while refresh is active and bit 6 of the refresh
					// address is low. The Z80 signals a refresh, providing the refresh address during the
					// final two cycles of an opcode fetch. Therefore communicate a transient signalling
					// of the IRQ line if necessary.
					if(!(address & 0x40)) {
						z80_.set_interrupt_line(true, Cycles(-2));
						z80_.set_interrupt_line(false);
					}
					if(has_latched_video_byte_) {
						std::size_t char_address = size_t((address & 0xfe00) | ((latched_video_byte_ & 0x3f) << 3) | line_counter_);
						const uint8_t mask = (latched_video_byte_ & 0x80) ? 0x00 : 0xff;
						if(char_address < ram_base_) {
							latched_video_byte_ = rom_[char_address & rom_mask_] ^ mask;
						} else {
							latched_video_byte_ = ram_[address & ram_mask_] ^ mask;
						}

						video_.output_byte(latched_video_byte_);
						has_latched_video_byte_ = false;
					}
				break;

				case CPU::Z80::PartialMachineCycle::ReadOpcode:
					// Check for use of the fast tape hack.
					if(use_fast_tape_hack_ && address == tape_trap_address_) {
						const uint64_t prior_offset = tape_player_.get_tape()->get_offset();
						const int next_byte = parser_.get_next_byte(tape_player_.get_tape());
						if(next_byte != -1) {
							const uint16_t hl = z80_.value_of(CPU::Z80::Register::HL);
							ram_[hl & ram_mask_] = uint8_t(next_byte);
							*cycle.value = 0x00;
							z80_.set_value_of(CPU::Z80::Register::ProgramCounter, tape_return_address_ - 1);

							// Assume that having read one byte quickly, we're probably going to be asked to read
							// another shortly. Therefore, temporarily disable the tape motor for 1000 cycles in order
							// to avoid fighting with real time. This is a stop-gap fix.
							tape_advance_delay_ = 1000;
							return 0;
						} else {
							tape_player_.get_tape()->set_offset(prior_offset);
						}
					}

					if(should_autorun_ && address == finished_load_address_) {
						type_string(is_zx81 ? "r \n" : "r\n ");	// Spaces here are not especially scientific; they merely ensure sufficient pauses for both the ZX80 and 81, empirically.
						should_autorun_ = false;
					}

					// Check for automatic tape control.
					if(use_automatic_tape_motor_control_) {
						tape_player_.set_motor_control((address >= automatic_tape_motor_start_address_) && (address < automatic_tape_motor_end_address_));
					}
					is_opcode_read = true;
					[[fallthrough]];

				case CPU::Z80::PartialMachineCycle::Read:
					if(address < ram_base_) {
						*cycle.value = rom_[address & rom_mask_];
					} else {
						const uint8_t value = ram_[address & ram_mask_];

						// If this is an M1 cycle reading from above the 32kb mark and HALT is not
						// currently active, latch for video output and return a NOP. Otherwise,
						// just return the value as read.
						if(is_opcode_read && address&0x8000 && !(value & 0x40) && !z80_.get_halt_line()) {
							latched_video_byte_ = value;
							has_latched_video_byte_ = true;
							*cycle.value = 0;
						} else *cycle.value = value;
					}
				break;

				case CPU::Z80::PartialMachineCycle::Write:
					if(address >= ram_base_) {
						ram_[address & ram_mask_] = *cycle.value;
					}
				break;

				default: break;
			}

			if(typer_) typer_->run_for(cycle.length);
			return HalfCycles(0);
		}

		void flush_output(int outputs) final {
			if(outputs & Output::Video) {
				video_.flush();
			}

			if constexpr (is_zx81) {
				if(outputs & Output::Audio) {
					update_audio();
					audio_queue_.perform();
				}
			}
		}

		void set_scan_target(Outputs::Display::ScanTarget *scan_target) final {
			video_.set_scan_target(scan_target);
		}

		Outputs::Display::ScanStatus get_scaled_scan_status() const final {
			return video_.get_scaled_scan_status();
		}

		Outputs::Speaker::Speaker *get_speaker() final {
			return is_zx81 ? &speaker_ : nullptr;
		}

		void run_for(const Cycles cycles) final {
			z80_.run_for(cycles);
		}

		bool insert_media(const Analyser::Static::Media &media) final {
			if(!media.tapes.empty()) {
				tape_player_.set_tape(media.tapes.front());
			}

			set_use_fast_tape();
			return !media.tapes.empty();
		}

		void type_string(const std::string &string) final {
			Utility::TypeRecipient<CharacterMapper>::add_typer(string);
		}

		bool can_type(char c) const final {
			return Utility::TypeRecipient<CharacterMapper>::can_type(c);
		}

		// MARK: - Keyboard
		void set_key_state(uint16_t key, bool is_pressed) final {
			keyboard_.set_key_state(key, is_pressed);
		}

		void clear_all_keys() final {
			keyboard_.clear_all_keys();
		}

		static constexpr Sinclair::ZX::Keyboard::Machine keyboard_machine() {
			return is_zx81 ? Sinclair::ZX::Keyboard::Machine::ZX81 : Sinclair::ZX::Keyboard::Machine::ZX80;
		}

		// MARK: - Tape control

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

		// MARK: - Typer timing
		HalfCycles get_typer_delay(const std::string &) const final {
			return z80_.get_is_resetting() ? Cycles(7'000'000) : Cycles(0);
		}

		HalfCycles get_typer_frequency() const final {
			return Cycles(146'250);
		}

		KeyboardMapper *get_keyboard_mapper() final {
			return &keyboard_mapper_;
		}

		// MARK: - Configuration options.

		std::unique_ptr<Reflection::Struct> get_options() final {
			auto options = std::make_unique<Options>(Configurable::OptionsType::UserFriendly);	// OptionsType is arbitrary, but not optional.
			options->automatic_tape_motor_control = use_automatic_tape_motor_control_;
			options->quickload = allow_fast_tape_hack_;
			return options;
		}

		void set_options(const std::unique_ptr<Reflection::Struct> &str) {
			const auto options = dynamic_cast<Options *>(str.get());
			set_use_automatic_tape_motor_control(options->automatic_tape_motor_control);
			allow_fast_tape_hack_ = options->quickload;
			set_use_fast_tape();
		}

	private:
		CPU::Z80::Processor<ConcreteMachine, false, is_zx81> z80_;
		Video video_;

		// If fast tape loading is enabled then the PC will be trapped at tape_trap_address_;
		// the emulator will then do a high-level reinterpretation of the standard ZX80/81 reading
		// of a single byte, and the next thing executed will be at tape_return_address_;
		static constexpr uint16_t tape_trap_address_ = is_zx81 ? 0x37c : 0x220;
		static constexpr uint16_t tape_return_address_ = is_zx81 ? 0x380 : 0x248;

		// If automatic tape motor control is enabled then the tape will be permitted to play any time
		// the program counter is >= automatic_tape_motor_start_address_ and < automatic_tape_motor_end_address_.
		static constexpr uint16_t automatic_tape_motor_start_address_ = is_zx81 ? 0x340 : 0x206;
		static constexpr uint16_t automatic_tape_motor_end_address_ = is_zx81 ? 0x3c3 : 0x24d;

		// When automatically loading, if the PC gets to the finished_load_address_ in order to print 0/0
		// (so it's anything that indicates that loading completed, but the program did not autorun) then the
		// emulator will automatically RUN whatever has been loaded.
		static constexpr uint16_t finished_load_address_ = is_zx81 ?
			0x6d1 :	// ZX81: this is the routine that prints 0/0 (i.e. success).
			0x203;	// ZX80: this is the JR that exits the ZX80's LOAD and returns to MAIN-EXEC.
		bool should_autorun_ = false;

		std::vector<uint8_t> ram_;
		uint16_t ram_mask_, ram_base_;

		std::vector<uint8_t> rom_;
		uint16_t rom_mask_;

		bool vsync_ = false, hsync_ = false;
		int line_counter_ = 0;

		Sinclair::ZX::Keyboard::Keyboard keyboard_;
		Sinclair::ZX::Keyboard::KeyboardMapper keyboard_mapper_;

		HalfClockReceiver<Storage::Tape::BinaryTapePlayer> tape_player_;
		Storage::Tape::ZX8081::Parser parser_;

		bool nmi_is_enabled_ = false;

		static constexpr auto vsync_start_ = is_zx81 ? HalfCycles(32) : HalfCycles(26);
		static constexpr auto vsync_end_ = is_zx81 ? HalfCycles(64) : HalfCycles(66);
		HalfCycles horizontal_counter_;

		uint8_t latched_video_byte_ = 0;
		bool has_latched_video_byte_ = false;

		bool use_fast_tape_hack_ = false;
		bool allow_fast_tape_hack_ = false;
		void set_use_fast_tape() {
			use_fast_tape_hack_ = allow_fast_tape_hack_ && tape_player_.has_tape();
		}
		bool use_automatic_tape_motor_control_ = true;
		HalfCycles tape_advance_delay_ = 0;

		// MARK: - Video
		inline void set_vsync(bool sync) {
			vsync_ = sync;
			update_sync();
		}

		inline void set_hsync(bool sync) {
			hsync_ = sync;
			update_sync();
		}

		inline void update_sync() {
			video_.set_sync(vsync_ || hsync_);
		}

		// MARK: - Audio
		Concurrency::AsyncTaskQueue<false> audio_queue_;
		using AY = GI::AY38910::AY38910<false>;
		AY ay_;
		Outputs::Speaker::PullLowpass<AY> speaker_;
		HalfCycles time_since_ay_update_;
		inline void ay_set_register(uint8_t value) {
			update_audio();
			GI::AY38910::Utility::select_register(ay_, value);
		}
		inline void ay_set_data(uint8_t value) {
			update_audio();
			GI::AY38910::Utility::write_data(ay_, value);
		}
		inline uint8_t ay_read_data() {
			update_audio();
			return GI::AY38910::Utility::read(ay_);
		}
		inline void update_audio() {
			speaker_.run_for(audio_queue_, time_since_ay_update_.divide_cycles(Cycles(2)));
		}
};

}
}

using namespace Sinclair::ZX8081;

// See header; constructs and returns an instance of the ZX80 or 81.
std::unique_ptr<Machine> Machine::ZX8081(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	const auto zx_target = dynamic_cast<const Analyser::Static::ZX8081::Target *>(target);

	// Instantiate the correct type of machine.
	if(zx_target->is_ZX81)	return std::make_unique<ConcreteMachine<true>>(*zx_target, rom_fetcher);
	else					return std::make_unique<ConcreteMachine<false>>(*zx_target, rom_fetcher);
}

Machine::~Machine() {}
