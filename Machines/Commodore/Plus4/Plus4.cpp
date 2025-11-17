//
//  Plus4.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/12/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#include "Plus4.hpp"

#include "Audio.hpp"
#include "Interrupts.hpp"
#include "Keyboard.hpp"
#include "Pager.hpp"
#include "Video.hpp"

#include "Machines/MachineTypes.hpp"
#include "Machines/Utility/MemoryFuzzer.hpp"
#include "Processors/6502Mk2/6502Mk2.hpp"
#include "Analyser/Static/Commodore/Target.hpp"
#include "Outputs/Log.hpp"
#include "Outputs/Speaker/Implementation/LowpassSpeaker.hpp"
#include "Configurable/StandardOptions.hpp"

#include "Analyser/Dynamic/ConfidenceCounter.hpp"
#include "Analyser/Static/Commodore/Target.hpp"

#include "Storage/Tape/Parsers/Commodore.hpp"
#include "Storage/Tape/Tape.hpp"
#include "Machines/Commodore/SerialBus.hpp"
#include "Machines/Commodore/1540/C1540.hpp"

#include "Processors/6502Esque/Implementation/LazyFlags.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

using namespace Commodore;
using namespace Commodore::Plus4;

namespace {
using Logger = Log::Logger<Log::Source::Plus4>;

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
		const auto apply = [&](uint8_t mask) {
			if(is_active) mask_ &= ~mask; else mask_ |= mask;
		};

		switch(digital_input.type) {
			default: return;
			case Input::Right:	apply(0x08);	break;
			case Input::Left:	apply(0x04);	break;
			case Input::Down:	apply(0x02);	break;
			case Input::Up:		apply(0x01);	break;
			case Input::Fire:	apply(0xc0);	break;
		}
	}

	uint8_t mask() const {
		return mask_;
	}

private:
	uint8_t mask_ = 0xff;
};

class Timers {
public:
	Timers(Interrupts &interrupts) : interrupts_(interrupts) {}

	template <int offset>
	void write(const uint8_t value) {
		const auto load_low = [&](uint16_t &target) {
			target = uint16_t((target & 0xff00) | (value << 0));
		};
		const auto load_high = [&](uint16_t &target) {
			target = uint16_t((target & 0x00ff) | (value << 8));
		};

		static constexpr auto timer = offset >> 1;
		paused_[timer] = ~offset & 1;
		if constexpr (offset & 1) {
			load_high(timers_[timer]);
			if constexpr (!timer) {
				load_high(timer0_reload_);
			}
		} else {
			load_low(timers_[timer]);
			if constexpr (!timer) {
				load_low(timer0_reload_);
			}
		}
	}

	template <int offset>
	uint8_t read() {
		static constexpr auto timer = offset >> 1;
		if constexpr (offset & 1) {
			return uint8_t(timers_[timer] >> 8);
		} else {
			return uint8_t(timers_[timer] >> 0);
		}
	}

	void tick(int count) {
		// Quick hack here; do better than stepping through one at a time.
		while(count--) {
			decrement<0>();
			decrement<1>();
			decrement<2>();
		}
	}

private:
	template <int timer>
	void decrement() {
		if(paused_[timer]) return;

		// Check for reload.
		if(!timer && !timers_[timer]) {
			timers_[timer] = timer0_reload_;
		}

		-- timers_[timer];

		// Check for interrupt.
		if(!timers_[timer]) {
			switch(timer) {
				case 0:	interrupts_.apply(Interrupts::Flag::Timer1);	break;
				case 1:	interrupts_.apply(Interrupts::Flag::Timer2);	break;
				case 2:	interrupts_.apply(Interrupts::Flag::Timer3);	break;
			}
		}
	}

	uint16_t timers_[3]{};
	uint16_t timer0_reload_ = 0xffff;
	bool paused_[3]{};

	Interrupts &interrupts_;
};

class SerialPort: public Serial::Port {
public:
	void set_input(const Serial::Line line, const Serial::LineLevel value) override {
		levels_[size_t(line)] = value;
	}

	Serial::LineLevel level(const Serial::Line line) const {
		return levels_[size_t(line)];
	}

private:
	Serial::LineLevel levels_[5];
};

class ConcreteMachine:
	public Activity::Source,
	public BusController,
	public ClockingHint::Observer,
	public Configurable::Device,
	public CPU::MOS6502::BusHandler,
	public MachineTypes::AudioProducer,
	public MachineTypes::JoystickMachine,
	public MachineTypes::MappedKeyboardMachine,
	public MachineTypes::TimedMachine,
	public MachineTypes::ScanProducer,
	public MachineTypes::MediaTarget,
	public Machine,
	public Utility::TypeRecipient<CharacterMapper> {
public:
	ConcreteMachine(const Analyser::Static::Commodore::Plus4Target &target, const ROMMachine::ROMFetcher &rom_fetcher) :
		m6502_(*this),
		interrupts_(*this),
		timers_(interrupts_),
		video_(video_map_, interrupts_),
		audio_(audio_queue_),
		speaker_(audio_)
	{
		const auto clock = clock_rate(false);
		media_divider_ = Cycles(clock);
		set_clock_rate(clock);
		speaker_.set_input_rate(float(clock));

		const auto kernel = ROM::Name::Plus4KernelPALv5;
		const auto basic = ROM::Name::Plus4BASIC;
		ROM::Request request = ROM::Request(basic) && ROM::Request(kernel);
		if(target.has_c1541) {
			request = request && C1540::Machine::rom_request(C1540::Personality::C1541);
		}

		auto roms = rom_fetcher(request);
		if(!request.validate(roms)) {
			throw ROMMachine::Error::MissingROMs;
		}

		kernel_ = roms.find(kernel)->second;
		basic_ = roms.find(basic)->second;

		Memory::Fuzz(ram_);
		map_.page<PagerSide::ReadWrite, 0, 65536>(ram_.data());
		page_cpu_rom();

		video_map_.page<PagerSide::ReadWrite, 0, 65536>(ram_.data());

		if(target.has_c1541) {
			c1541_ = std::make_unique<C1540::Machine>(C1540::Personality::C1541, roms);
			c1541_->set_serial_bus(serial_bus_);
			Serial::attach(serial_port_, serial_bus_);
			c1541_->run_for(Cycles(2000000));
		}

		tape_player_ = std::make_unique<Storage::Tape::BinaryTapePlayer>(clock);
		tape_player_->set_clocking_hint_observer(this);

		joysticks_.emplace_back(std::make_unique<Joystick>());
		joysticks_.emplace_back(std::make_unique<Joystick>());

		insert_media(target.media);
		if(!target.loading_command.empty()) {
			// Prefix a space as a delaying technique.
			type_string(std::string(" ") + target.loading_command);
		}
	}

	~ConcreteMachine() {
		audio_queue_.lock_flush();
	}

	// HACK. NOCOMMIT.
//	int pulse_num_ = 0;

	template <CPU::MOS6502Mk2::BusOperation operation, typename AddressT>
	Cycles perform(const AddressT address, CPU::MOS6502Mk2::data_t<operation> value) {
		// Determine from the TED video subsystem the length of this clock cycle as perceived by the 6502,
		// relative to the master clock.
		const auto length = video_.cycle_length(operation == CPU::MOS6502Mk2::BusOperation::Ready);

		// Update other subsystems.
		advance_timers_and_tape(length);
		if(!superspeed_) {
			video_.run_for(length);

			if(c1541_) {
				c1541_cycles_ += length * Cycles(1'000'000);
				c1541_->run_for(c1541_cycles_.divide(media_divider_));
			}


			time_since_audio_update_ += length;
		}

		if(operation == CPU::MOS6502Mk2::BusOperation::Ready) {
			return length;
		}

		// Perform actual access.
		if(address < 0x0002) {
			// 0x0000: data directions for parallel IO; 1  = output.
			// 0x0001:
			//	b7 = serial data in;
			//	b6 = serial clock in and cassette write;
			//	b5 = [unconnected];
			//	b4 = cassette read;
			//	b3 = cassette motor, 1 = off;
			//	b2 = serial ATN out;
			//	b1 = serial clock out and cassette write;
			//	b0 = serial data out.

			if constexpr (is_read(operation)) {
				if(!address) {
					value = io_direction_;
				} else {
					value = io_input();
				}
			} else {
				if(!address) {
					io_direction_ = value;
				} else {
					io_output_ = value;
				}

				const auto output = io_output_ | ~io_direction_;
				update_tape_motor();
				serial_port_.set_output(Serial::Line::Data, Serial::LineLevel(~output & 0x01));
				serial_port_.set_output(Serial::Line::Clock, Serial::LineLevel(~output & 0x02));
				serial_port_.set_output(Serial::Line::Attention, Serial::LineLevel(~output & 0x04));
			}
		} else if(address < 0xfd00 || address >= 0xff40) {
//			if(
//				use_fast_tape_hack_ &&
//				operation == CPU::MOS6502Esque::BusOperation::ReadOpcode
//			) {
//				superspeed_ |= address == 0xe5fd;
//				superspeed_ &= (address != 0xe68b) && (address != 0xe68d);
//			}

			static constexpr bool use_hle = true;

//			if(
//				use_fast_tape_hack_ &&
//				operation == CPU::MOS6502Esque::BusOperation::ReadOpcode &&
//				address == 0xe5fd
//			) {
//				printf("Pulse %d from %lld ",
//					pulse_num_,
//					tape_player_->serialiser()->offset()
//				);
//			}

			if constexpr (is_read(operation)) {
				if(
					use_fast_tape_hack_ &&
					operation == CPU::MOS6502Mk2::BusOperation::ReadOpcode &&
					(
						(use_hle && address == 0xe5fd) ||
						address == 0xe68b ||
						address == 0xe68d
					)
				) {
	//				++pulse_num_;
					if(use_hle) {
						read_dipole();
					}

	//				using Flag = CPU::MOS6502::Flag;
	//				using Register = CPU::MOS6502::Register;
	//				const auto flags = m6502_.value_of(Register::Flags);
	//				printf("to %lld: %c%c%c\n",
	//					tape_player_->serialiser()->offset(),
	//					flags & Flag::Sign ? 'n' : '-',
	//					flags & Flag::Overflow ? 'v' : '-',
	//					flags & Flag::Carry ? 'c' : '-'
	//				);
					value = 0x60;
				} else {
					value = map_.read(address);
				}
			} else {
				map_.write(address) = value;
			}


			// TODO: rdbyte and ldsync is probably sufficient?

//			if(use_fast_tape_hack_ && operation == CPU::MOS6502Esque::BusOperation::ReadOpcode) {
//				static constexpr uint16_t ldsync = 0;
//				switch(address) {
//					default: break;
//
//					case ldsync:
//					break;
//				}
//
//				if(address == 0xe9cc) {
//					// Skip the `jsr rdblok` that opens `fah` (i.e. find any header), performing
//					// its function as a high-level emulation.
//					Storage::Tape::Commodore::Parser parser(TargetPlatform::Plus4);
//					auto header = parser.get_next_header(*tape_player_->serialiser());
//
//					const auto tape_position = tape_player_->serialiser()->offset();
//					if(header) {
//						// Copy to in-memory buffer and set type.
//						std::memcpy(&ram_[0x0333], header->data.data(), 191);
//						map_.write(0xb6) = 0x33;
//						map_.write(0xb7) = 0x03;
//						map_.write(0xf8) = header->type_descriptor();
////						hold_tape_ = true;
//						Logger::info().append("Found header");
//					} else {
//						// no header found, so pretend this hack never interceded
//						tape_player_->serialiser()->set_offset(tape_position);
////						hold_tape_ = false;
//						Logger::info().append("Didn't find header");
//					}
//
//					// Clear status and the verify flags.
//					ram_[0x90] = 0;
//					ram_[0x93] = 0;
//
//					value = 0x0c;	// NOP abs.
//				}
//			}
		} else if(address < 0xff00) {
			// Miscellaneous hardware. All TODO.
			if constexpr (is_read(operation)) {
				switch(address & 0xfff0) {
					case 0xfd10:
						// 6529 parallel port, about which I know only what I've found in kernel ROM disassemblies.

						// If play button is not currently pressed and this read is immediately followed by
						// an AND 4, press it. The kernel will deal with motor control subsequently.
						if(!play_button_) {
							const uint16_t pc = m6502_.registers().pc.full;
							const uint8_t next[] = {
								map_.read(pc+0),
								map_.read(pc+1),
								map_.read(pc+2),
								map_.read(pc+3),
							};

							// TODO: boil this down to a PC check. It's currently in this form as I'm unclear what
							// diversity of kernels exist.
							if(next[0] == 0x29 && next[1] == 0x04 && next[2] == 0xd0 && next[3] == 0xf4) {
								play_button_ = true;
								update_tape_motor();
							}
						}

						value = 0xff ^ (play_button_ ? 0x4 :0x0);
					break;

					case 0xfdd0:
					case 0xfdf0:
						value = uint8_t(address >> 8);
					break;

					default:
						value = 0xff;
						Logger::info().append("TODO: read @ %04x", address);
					break;
				}
			} else {
				switch(address & 0xfff0) {
					case 0xfd30:
						keyboard_mask_ = value;
					break;

					case 0xfdd0: {
//						const auto low = address & 3;
//						const auto high = (address >> 2) & 3;
						// TODO: set up ROMs.
					} break;

					default:
						Logger::info().append("TODO: write of %02x @ %04x", value, address);
					break;
				}
			}
		} else {
			const auto pc = m6502_.registers().pc.full;
			const bool is_from_rom =
				(rom_is_paged_ && pc >= 0x8000) ||
				(pc >= 0x400 && pc < 0x500) ||
				(pc >= 0x700 && pc < 0x800);
			bool is_hit = true;

			if constexpr (is_read(operation)) {
				switch(address) {
					case 0xff00:	value = timers_.read<0>();		break;
					case 0xff01:	value = timers_.read<1>();		break;
					case 0xff02:	value = timers_.read<2>();		break;
					case 0xff03:	value = timers_.read<3>();		break;
					case 0xff04:	value = timers_.read<4>();		break;
					case 0xff05:	value = timers_.read<5>();		break;
					case 0xff06:	value = video_.read<0xff06>();	break;
					case 0xff07:	value = video_.read<0xff07>();	break;
					case 0xff08: {
						const uint8_t keyboard_input =
							~(
								((keyboard_mask_ & 0x01) ? 0x00 : key_states_[0]) |
								((keyboard_mask_ & 0x02) ? 0x00 : key_states_[1]) |
								((keyboard_mask_ & 0x04) ? 0x00 : key_states_[2]) |
								((keyboard_mask_ & 0x08) ? 0x00 : key_states_[3]) |
								((keyboard_mask_ & 0x10) ? 0x00 : key_states_[4]) |
								((keyboard_mask_ & 0x20) ? 0x00 : key_states_[5]) |
								((keyboard_mask_ & 0x40) ? 0x00 : key_states_[6]) |
								((keyboard_mask_ & 0x80) ? 0x00 : key_states_[7])
							);

						const uint8_t joystick_mask =
							0xff &
							((joystick_mask_ & 0x02) ? 0xff : (joystick(0).mask() | 0x40)) &
							((joystick_mask_ & 0x04) ? 0xff : (joystick(1).mask() | 0x80));

						value = keyboard_input & joystick_mask;
					} break;
					case 0xff09:	value = interrupts_.status();	break;
					case 0xff0a:
						value = interrupts_.mask() | video_.read<0xff0a>() | 0xa0;
					break;
					case 0xff0b:	value = video_.read<0xff0b>();	break;
					case 0xff0c:	value = video_.read<0xff0c>();	break;
					case 0xff0d:	value = video_.read<0xff0d>();	break;
					case 0xff0e:	value = ff0e_;					break;
					case 0xff0f:	value = ff0f_;					break;
					case 0xff10:	value = ff10_ | 0xfc;			break;
					case 0xff11:	value = ff11_;					break;
					case 0xff12:	value = ff12_ | 0xc0;			break;
					case 0xff13:	value = ff13_ | (rom_is_paged_ ? 1 : 0);	break;
					case 0xff14:	value = video_.read<0xff14>();	break;
					case 0xff15:	value = video_.read<0xff15>();	break;
					case 0xff16:	value = video_.read<0xff16>();	break;
					case 0xff17:	value = video_.read<0xff17>();	break;
					case 0xff18:	value = video_.read<0xff18>();	break;
					case 0xff19:	value = video_.read<0xff19>();	break;
					case 0xff1a:	value = video_.read<0xff1a>();	break;
					case 0xff1b:	value = video_.read<0xff1b>();	break;
					case 0xff1c:	value = video_.read<0xff1c>();	break;
					case 0xff1d:	value = video_.read<0xff1d>();	break;
					case 0xff1e:	value = video_.read<0xff1e>();	break;
					case 0xff1f:	value = video_.read<0xff1f>();	break;

					case 0xff3e:	value = 0;						break;
					case 0xff3f:	value = 0;						break;

					default:
						Logger::info().append("TODO: TED read at %04x", address);
						value = 0xff;
						is_hit = false;
				}
			} else {
				switch(address) {
					case 0xff00:	timers_.write<0>(value);		break;
					case 0xff01:	timers_.write<1>(value);		break;
					case 0xff02:	timers_.write<2>(value);		break;
					case 0xff03:	timers_.write<3>(value);		break;
					case 0xff04:	timers_.write<4>(value);		break;
					case 0xff05:	timers_.write<5>(value);		break;
					case 0xff06:	video_.write<0xff06>(value);	break;
					case 0xff07:
						video_.write<0xff07>(value);
						update_audio();
						audio_.set_divider(value);
					break;
					case 0xff08:
						// Observation here: the kernel posts a 0 to this
						// address upon completing each keyboard scan cycle,
						// once per frame.
						if(typer_ && !value) {
							if(!typer_->type_next_character()) {
								clear_all_keys();
								typer_.reset();
							}
						}

						joystick_mask_ = value;
					break;
					case 0xff09:
						interrupts_.set_status(value);
					break;
					case 0xff0a:
						interrupts_.set_mask(value);
						video_.write<0xff0a>(value);
					break;
					case 0xff0b:	video_.write<0xff0b>(value);	break;
					case 0xff0c:	video_.write<0xff0c>(value);	break;
					case 0xff0d:	video_.write<0xff0d>(value);	break;
					case 0xff0e:
						ff0e_ = value;
						update_audio();
						audio_.set_frequency_low<0>(value);
					break;
					case 0xff0f:
						ff0f_ = value;
						update_audio();
						audio_.set_frequency_low<1>(value);
					break;
					case 0xff10:
						ff10_ = value;
						update_audio();
						audio_.set_frequency_high<1>(value);
					break;
					case 0xff11:
						ff11_ = value;
						update_audio();
						audio_.set_control(value);
					break;
					case 0xff12:
						ff12_ = value & 0x3f;
						video_.write<0xff12>(value);

						if((value & 4)) {
							page_video_rom();
						} else {
							page_video_ram();
						}

						update_audio();
						audio_.set_frequency_high<0>(value);
					break;
					case 0xff13:
						ff13_ = value & 0xfe;
						video_.write<0xff13>(value);
					break;
					case 0xff14:	video_.write<0xff14>(value);	break;
					case 0xff15:	video_.write<0xff15>(value);	break;
					case 0xff16:	video_.write<0xff16>(value);	break;
					case 0xff17:	video_.write<0xff17>(value);	break;
					case 0xff18:	video_.write<0xff18>(value);	break;
					case 0xff19:	video_.write<0xff19>(value);	break;
					case 0xff1a:	video_.write<0xff1a>(value);	break;
					case 0xff1b:	video_.write<0xff1b>(value);	break;
					case 0xff1c:	video_.write<0xff1c>(value);	break;
					case 0xff1d:	video_.write<0xff1d>(value);	break;
					case 0xff1e:	video_.write<0xff1e>(value);	break;
					case 0xff1f:	video_.write<0xff1f>(value);	break;

					case 0xff3e:	page_cpu_rom();					break;
					case 0xff3f:	page_cpu_ram();					break;

					default:
						Logger::info().append("TODO: TED write at %04x", address);
						is_hit = false;
				}
			}
			if(!is_from_rom) {
				if(is_hit) confidence_.add_hit(); else confidence_.add_miss();
			}
		}

		return superspeed_ ? Cycles(0) : length;
	}

private:
	struct M6502Traits {
		static constexpr auto uses_ready_line = true;
		static constexpr auto pause_precision = CPU::MOS6502Mk2::PausePrecision::BetweenInstructions;
		using BusHandlerT = ConcreteMachine;
	};
	CPU::MOS6502Mk2::Processor<CPU::MOS6502Mk2::Model::M6502, M6502Traits> m6502_;

	Outputs::Speaker::Speaker *get_speaker() override {
		return &speaker_;
	}

	void set_activity_observer(Activity::Observer *const observer) final {
		if(c1541_) c1541_->set_activity_observer(observer);
	}

	void set_irq_line(const bool active) override {
		m6502_.template set<CPU::MOS6502Mk2::Line::IRQ>(active);
	}
	void set_ready_line(const bool active) override {
		m6502_.template set<CPU::MOS6502Mk2::Line::Ready>(active);
	}

	void page_video_rom() {
		video_map_.page<PagerSide::Read, 0x8000, 16384>(basic_.data());
		video_map_.page<PagerSide::Read, 0xc000, 16384>(kernel_.data());
	}
	void page_video_ram() {
		video_map_.page<PagerSide::Read, 0x8000, 32768>(&ram_[0x8000]);
	}

	void page_cpu_rom() {
		// TODO: allow other ROM selection. And no ROM?
		map_.page<PagerSide::Read, 0x8000, 16384>(basic_.data());
		map_.page<PagerSide::Read, 0xc000, 16384>(kernel_.data());
		rom_is_paged_ = true;
		set_use_fast_tape();
	}
	void page_cpu_ram() {
		map_.page<PagerSide::Read, 0x8000, 32768>(&ram_[0x8000]);
		rom_is_paged_ = false;
		set_use_fast_tape();
	}
	bool rom_is_paged_ = false;

	void set_scan_target(Outputs::Display::ScanTarget *const target) final {
		video_.set_scan_target(target);
	}

	Outputs::Display::ScanStatus get_scaled_scan_status() const final {
		return video_.get_scaled_scan_status();
	}

	void set_display_type(const Outputs::Display::DisplayType display_type) final {
		video_.set_display_type(display_type);
	}

	Outputs::Display::DisplayType get_display_type() const final {
		return video_.get_display_type();
	}

	void run_for(const Cycles cycles) final {
		m6502_.run_for(cycles);

		// I don't know why.
		update_audio();
		audio_queue_.perform();
	}

	void flush_output(int outputs) override {
		if(outputs & Output::Audio) {
			update_audio();
			audio_queue_.perform();
		}
	}

	bool insert_media(const Analyser::Static::Media &media) final {
		if(!media.tapes.empty()) {
			tape_player_->set_tape(media.tapes[0], TargetPlatform::Plus4);
		}

		if(!media.disks.empty() && c1541_) {
			c1541_->set_disk(media.disks[0]);
		}

		return true;
	}

	Plus4::Pager map_;
	Plus4::Pager video_map_;
	std::array<uint8_t, 65536> ram_;
	std::vector<uint8_t> kernel_;
	std::vector<uint8_t> basic_;
	uint8_t ff0e_, ff0f_, ff10_, ff11_, ff12_, ff13_;

	Interrupts interrupts_;
	Cycles timers_subcycles_;
	Timers timers_;
	Video video_;

	Concurrency::AsyncTaskQueue<false> audio_queue_;
	Audio audio_;
	Cycles time_since_audio_update_;
	Outputs::Speaker::PullLowpass<Audio> speaker_;
	void update_audio() {
		speaker_.run_for(audio_queue_, time_since_audio_update_.flush<Cycles>());
	}

	// MARK: - MappedKeyboardMachine.
	MappedKeyboardMachine::KeyboardMapper *get_keyboard_mapper() override {
		static Plus4::KeyboardMapper keyboard_mapper_;
		return &keyboard_mapper_;
	}

	void type_string(const std::string &string) final {
		Utility::TypeRecipient<CharacterMapper>::add_typer(string);
	}

	bool can_type(const char c) const final {
		return Utility::TypeRecipient<CharacterMapper>::can_type(c);
	}

	void set_key_state(uint16_t key, bool is_pressed) override {
		if(is_pressed) {
			key_states_[line(key)] |= mask(key);
		} else {
			key_states_[line(key)] &= ~mask(key);
		}
	}

	void clear_all_keys() final {
		std::fill(key_states_.begin(), key_states_.end(), 0);
	}

	std::array<uint8_t, 8> key_states_{};
	uint8_t keyboard_mask_ = 0xff;
	uint8_t joystick_mask_ = 0xff;

	Cycles media_divider_, c1541_cycles_;
	std::unique_ptr<C1540::Machine> c1541_;
	Serial::Bus serial_bus_;
	SerialPort serial_port_;

	std::unique_ptr<Storage::Tape::BinaryTapePlayer> tape_player_;
	bool play_button_ = false;
	bool allow_fast_tape_hack_ = false;	// TODO: implement fast-tape hack.
	bool use_fast_tape_hack_ = false;
	bool superspeed_ = false;
	void set_use_fast_tape() {
		use_fast_tape_hack_ =
			allow_fast_tape_hack_ && tape_player_->motor_control() && rom_is_paged_ && !tape_player_->is_at_end();
	}
	void update_tape_motor() {
		const auto output = io_output_ | ~io_direction_;
		tape_player_->set_motor_control(play_button_ && (~output & 0x08));
	}
	void advance_timers_and_tape(const Cycles length) {
		timers_subcycles_ += length;
		const auto timers_cycles = timers_subcycles_.divide(video_.timer_cycle_length());
		timers_.tick(timers_cycles.as<int>());

		tape_player_->run_for(length);
	}

	// TODO: substantially simplify the below; at the minute it's a
	// literal transcription of the original as a simple first step.
	void read_dipole() {
		using Flag = CPU::MOS6502Mk2::Flag;

		//
		// Get registers now and ensure they'll be written back at function exit.
		//
		auto registers = m6502_.registers();
		struct ScopeGuard {
			ScopeGuard(std::function<void(void)> at_exit) : at_exit_(at_exit) {}
			~ScopeGuard() {	at_exit_();	}
		private:
			std::function<void(void)> at_exit_;
		} store_registers([&] {
			m6502_.set_registers(registers);
		});

		//
		// Time advancement.
		//
		const auto advance_cycles = [&](int cycles) -> bool {
			advance_timers_and_tape(video_.cycle_length(false) * cycles);
			return !use_fast_tape_hack_;
		};

		//
		// 6502 pseudo-ops.
		//
		const auto ldabs = [&] (uint8_t &target, const uint16_t address) {
			registers.flags.set_per<Flag::NegativeZero>(target = map_.read(address));
		};
		const auto ldimm = [&] (uint8_t &target, const uint8_t value) {
			registers.flags.set_per<Flag::NegativeZero>(target = value);
		};
		const auto pha = [&] () {
			map_.write(0x100 + registers.s) = registers.a;
			--registers.s;
		};
		const auto pla = [&] () {
			++registers.s;
			registers.a = map_.read(0x100 + registers.s);
		};
		const auto bit = [&] (const uint8_t value) {
			registers.flags.set_per<Flag::Zero>(registers.a & value);
			registers.flags.set_per<Flag::Negative>(value);
			registers.flags.set_per<Flag::Overflow>(value);
		};
		const auto cmp = [&] (const uint8_t value) {
			const uint16_t temp16 = registers.a - value;
			registers.flags.set_per<Flag::NegativeZero>(uint8_t(temp16));
			registers.flags.set_per<Flag::Carry>(((~temp16) >> 8)&1);
		};
		const auto andimm = [&] (const uint8_t value) {
			registers.a &= value;
			registers.flags.set_per<Flag::NegativeZero>(registers.a);
		};
		const auto ne = [&]() -> bool {
			return !registers.flags.get<Flag::Zero>();
		};
		const auto eq = [&]() -> bool {
			return registers.flags.get<Flag::Zero>();
		};

		//
		// Common branch points.
		//
		const auto dipok = [&] {
			//       clc             	; everything's fine
			//       rts
			registers.flags.set_per<Flag::Carry>(0);
		};
		const auto rshort = [&] {
			//       bit  tshrtd     	; got a short
			//       bvs  dipok      	; !bra
			bit(0x40);
			dipok();
		};
		const auto rlong = [&] {
			//       bit  tlongd     	; got a long
			bit(0x00);
			dipok();
		};
		const auto rderr1 = [&] {
			//       sec             	; i'm confused
			//       rts
			registers.flags.set_per<Flag::Carry>(Flag::Carry);
		};

		//
		// Labels.
		//
		static constexpr uint16_t dsamp1 = 0x7b8;
		static constexpr uint16_t dsamp2 = 0x7ba;
		static constexpr uint16_t zcell = 0x07bc;

		//rddipl
		//       ldx  dsamp1     	; setup x,y with 1st sample point
		//       ldy  dsamp1+1
		ldabs(registers.x, dsamp1);
		ldabs(registers.y, dsamp1 + 1);
		advance_cycles(8);

		//badeg1
		do {
			//       lda  dsamp2+1   	; put 2nd samp value on stack in reverse order
			//       pha
			//       lda  dsamp2
			//       pha
			ldabs(registers.a, dsamp2 + 1);
			pha();
			ldabs(registers.a, dsamp2);
			pha();
			advance_cycles(14);

			//       lda  #$10
			//rwtl   			; wait till rd line is high
			//       bit  port	[= $0001]
			//       beq  rwtl       	; !ls!
			ldimm(registers.a, 0x10);
			advance_cycles(2);
			do {
				bit(io_input());
				if(advance_cycles(6)) {
					return;
				}
			} while(eq());

			//rwth   			;it's high...now wait till it's low
			//       bit  port
			//       bne  rwth	; caught the edge
			do {
				bit(io_input());
				if(advance_cycles(6)) {
					return;
				}
			} while(ne());


			//       stx  timr2l
			//       sty  timr2h
			timers_.write<2>(registers.x);
			timers_.write<3>(registers.y);
			advance_cycles(8);


			//; go! ...ta
			//
			//       pla		;go! ...ta
			//       sta  timr3l
			//       pla
			//       sta  timr3h     	;go! ...tb
			pla();
			timers_.write<4>(registers.a);
			pla();
			timers_.write<5>(registers.a);
			advance_cycles(14);


			//; clear timer flags
			//
			//       lda  #$50       	; clr ta,tb
			//       sta  tedirq
			ldimm(registers.a, 0x50);
			interrupts_.set_status(registers.a);
			advance_cycles(6);


			//; um...check that edge again
			//
			//casdb1
			//       lda  port
			//       cmp  port
			//       bne  casdb1     	; something is going on here...
			//       and  #$10       	; a look at that edge again
			//       bne  badeg1     	; woa! got a bad edge trigger  !ls!
			do {
				ldimm(registers.a, io_input());
				cmp(io_input());
				if(advance_cycles(9)) {
					return;
				}
			} while(ne());
			andimm(0x10);
			advance_cycles(5);
		} while(ne());


		//
		//; must have been a valid edge
		//;
		//; do stop key check here
		//
		//       jsr  balout

		/* balout not checked */


		//       lda  #$10
		//wata   			; wait for ta to timeout
		ldimm(registers.a, 0x10);
		advance_cycles(3);
		do {
			//       bit  port       	; kuldge, kludge, kludge !!! <<><>>
			//       bne  rshort     	; kuldge, kludge, kludge !!! <<><>>
			bit(io_input());
			if(ne()) {
				rshort();
				return;
			}

			//       bit  tedirq
			//       beq  wata
			bit(interrupts_.status());

			if(advance_cycles(12)) {
				return;
			}
		} while(eq());


		//
		//; now do the dipole sample #1
		//
		//casdb2
		do {
			//       lda  port
			//       cmp  port
			ldimm(registers.a, io_input());
			cmp(io_input());

			if(advance_cycles(9)) {
				return;
			}
			//       bne  casdb2
		} while(ne());

		//       and  #$10
		//       bne  rshort     	; shorts anyone?
		andimm(0x10);
		advance_cycles(3);
		if(ne()) {
			rshort();
			return;
		}

		//
		//; perhaps a long or a word?
		//
		//       lda  #$40
		//watb
		//       bit  tedirq
		//       beq  watb
		//
		//; wait for tb to timeout
		//; now do the dipole sample #2
		ldimm(registers.a, 0x40);
		advance_cycles(3);
		do {
			bit(interrupts_.status());
			if(advance_cycles(6)) {
				return;
			}
		} while(eq());


		//casdb3
		//       lda  port
		//       cmp  port
		//       bne  casdb3
		do {
			ldimm(registers.a, io_input());
			cmp(io_input());
			if(advance_cycles(9)) {
				return;
			}
		} while(ne());

		//       and  #$10
		//       bne  rlong      	; looks like a long from here !ls!
		andimm(0x10);
		advance_cycles(2);
		if(ne()) {
			rlong();
			return;
		}

		//			; or could it be a word?
		//       lda  zcell
		//       sta  timr2l
		//       lda  zcell+1
		//       sta  timr2h
		ldabs(registers.a, zcell);
		timers_.write<2>(registers.a);
		ldabs(registers.a, zcell + 1);
		timers_.write<3>(registers.y);
		advance_cycles(16);


		//			; go! z-cell check
		//			; clear ta flag
		//       lda  #$10
		//       sta  tedirq	; verify +180 half of word dipole
		//       lda  #$10
		ldimm(registers.a, 0x10);
		interrupts_.set_status(registers.a);
		ldimm(registers.a, 0x10);
		advance_cycles(8);

		//wata2
		//       bit  tedirq
		//       beq  wata2	; check z-cell is low
		do {
			bit(interrupts_.status());
			if(advance_cycles(7)) {
				return;
			}
		} while(eq());

		//casdb4
		//       lda  port
		//       cmp  port
		//       bne  casdb4
		do {
			ldimm(registers.a, io_input());
			cmp(io_input());
			if(advance_cycles(9)) {
				return;
			}
		} while(ne());

		//       and  #$10
		//       beq  rderr1     	; !ls!
		//       bit  twordd     	; got a word dipole
		//       bmi  dipok      	; !bra
		andimm(0x10);
		advance_cycles(2);
		if(eq()) {
			rderr1();
			return;
		}
		bit(0x80);
		advance_cycles(2);
		dipok();
	}

	uint8_t io_direction_ = 0x00, io_output_ = 0x00;
	uint8_t io_input() const {
		const uint8_t all_inputs =
			(tape_player_->input() ? 0x00 : 0x10) |
			(serial_port_.level(Serial::Line::Data) ? 0x80 : 0x00) |
			(serial_port_.level(Serial::Line::Clock) ? 0x40 : 0x00);
		return
			(io_direction_ & io_output_) |
			(~io_direction_ & all_inputs);
	}

	std::vector<std::unique_ptr<Inputs::Joystick>> &get_joysticks() override {
		return joysticks_;
	}
	Joystick &joystick(size_t index) const {
		return *static_cast<Joystick *>(joysticks_[index].get());
	}
	std::vector<std::unique_ptr<Inputs::Joystick>> joysticks_;

	// MARK: - ClockingHint::Observer.
	void set_component_prefers_clocking(ClockingHint::Source *, ClockingHint::Preference) override {
		set_use_fast_tape();
	}

	// MARK: - Confidence.
	Analyser::Dynamic::ConfidenceCounter confidence_;
	float get_confidence() final { return confidence_.confidence(); }
	std::string debug_type() final {
		return "Plus4";
	}

	// MARK: - Configuration options.
	std::unique_ptr<Reflection::Struct> get_options() const final {
		auto options = std::make_unique<Options>(Configurable::OptionsType::UserFriendly);
		options->output = get_video_signal_configurable();
		options->quick_load = allow_fast_tape_hack_;
		return options;
	}

	void set_options(const std::unique_ptr<Reflection::Struct> &str) final {
		const auto options = dynamic_cast<Options *>(str.get());

		set_video_signal_configurable(options->output);
		allow_fast_tape_hack_ = options->quick_load;
		set_use_fast_tape();
	}
};
}

std::unique_ptr<Machine> Machine::Plus4(
	const Analyser::Static::Target *target,
	const ROMMachine::ROMFetcher &rom_fetcher
) {
	using Target = Analyser::Static::Commodore::Plus4Target;
	const Target *const commodore_target = dynamic_cast<const Target *>(target);
	return std::make_unique<ConcreteMachine>(*commodore_target, rom_fetcher);
}
