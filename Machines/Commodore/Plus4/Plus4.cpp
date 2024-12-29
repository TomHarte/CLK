//
//  Plus4.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/12/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#include "Plus4.hpp"

#include "Interrupts.hpp"
#include "Keyboard.hpp"
#include "Pager.hpp"
#include "Video.hpp"

#include "../../MachineTypes.hpp"
#include "../../Utility/MemoryFuzzer.hpp"
#include "../../../Processors/6502/6502.hpp"
#include "../../../Analyser/Static/Commodore/Target.hpp"

#include "../../../Storage/Tape/Tape.hpp"

using namespace Commodore::Plus4;

namespace {

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

		constexpr auto timer = offset >> 1;
		paused_[timer] = !(offset & 1);
		if constexpr (offset & 1) {
			load_high(timers_[timer]);
			if(!timer) {
				load_high(timer0_reload_);
			}
		} else {
			load_low(timers_[timer]);
			if(!timer) {
				load_low(timer0_reload_);
			}
		}
	}

	template <int offset>
	uint8_t read() {
		constexpr auto timer = offset >> 1;
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

class ConcreteMachine:
	public BusController,
	public CPU::MOS6502::BusHandler,
	public MachineTypes::MappedKeyboardMachine,
	public MachineTypes::TimedMachine,
	public MachineTypes::ScanProducer,
	public MachineTypes::MediaTarget,
	public Machine {
public:
	ConcreteMachine(const Analyser::Static::Commodore::Target &target, const ROMMachine::ROMFetcher &rom_fetcher) :
		m6502_(*this),
		interrupts_(*this),
		timers_(interrupts_),
		video_(video_map_, interrupts_)
	{
		const auto clock = clock_rate(false);
		media_divider_ = Cycles(clock);
		set_clock_rate(clock);
		tape_player_ = std::make_unique<Storage::Tape::BinaryTapePlayer>(clock);

		const auto kernel = ROM::Name::Plus4KernelPALv5;
		const auto basic = ROM::Name::Plus4BASIC;

		const ROM::Request request = ROM::Request(basic) && ROM::Request(kernel);
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

		insert_media(target.media);
		printf("Loading command is: %s\n", target.loading_command.c_str());
	}

	Cycles perform_bus_operation(
		const CPU::MOS6502::BusOperation operation,
		const uint16_t address,
		uint8_t *const value
	) {
		// Determine from the TED video subsystem the length of this clock cycle as perceived by the 6502,
		// relative to the master clock.
		const auto length = video_.cycle_length(operation == CPU::MOS6502::BusOperation::Ready);

		// Update other subsystems.
		timers_subcycles_ += length;
		const auto timers_cycles = timers_subcycles_.divide(video_.timer_cycle_length());
		timers_.tick(timers_cycles.as<int>());

		video_.run_for(length);
		tape_player_->run_for(length);

		if(operation == CPU::MOS6502::BusOperation::Ready) {
			return length;
		}

		// Perform actual access.
		if(address < 0x0002) {
			// TODO: 0x0000: data directions for parallel IO; 1  = output.
			// TODO: 0x0001:
			//	b7 = serial data in;
			//	b6 = serial clock in and cassette write;
			//	b5 = [unconnected];
			//	b4 = cassette read;
			//	b3 = cassette motor, 1 = off;
			//	b2 = serial ATN out;
			//	b1 = serial clock out and cassette write;
			//	b0 = serial data out.

			if(isReadOperation(operation)) {
				if(!address) {
					*value = io_direction_;
//					printf("Read data direction: %02x\n", *value);
				} else {
					const uint8_t all_inputs = (tape_player_->input() ? 0x00 : 0x10) | 0xe0;
					*value =
						(io_direction_ & io_output_) |
						((~io_direction_) & all_inputs);
					printf("[%04x] In: %02x\n", m6502_.value_of(CPU::MOS6502::Register::ProgramCounter), *value);
				}
			} else {
				if(!address) {
					io_direction_ = *value;
//					printf("Set data direction: %02x\n", *value);
				} else {
					io_output_ = *value;
					printf("[%04x] Out: %02x\n", m6502_.value_of(CPU::MOS6502::Register::ProgramCounter), *value);
//					tape_player_->set_motor_control(*value & 0x08);
					tape_player_->set_motor_control(!(*value & 0x08));
				}
			}

//			printf("%04x: %02x %c\n", address, *value, isReadOperation(operation) ? 'r' : 'w');
		} else if(address < 0xfd00 || address >= 0xff40) {
			if(isReadOperation(operation)) {
				*value = map_.read(address);
			} else {
				map_.write(address) = *value;
			}
		} else if(address < 0xff00) {
			// Miscellaneous hardware. All TODO.
			if(isReadOperation(operation)) {
//				printf("TODO: read @ %04x\n", address);
				if((address & 0xfff0) == 0xfd10) {
					*value = 0xff ^ 0x4;	// Seems to detect the play button.
				} else {
					*value = 0xff;
				}
			} else {
//				printf("TODO: write of %02x @ %04x\n", *value, address);
			}
		} else {
			if(isReadOperation(operation)) {
				switch(address) {
					case 0xff00:	*value = timers_.read<0>();		break;
					case 0xff01:	*value = timers_.read<1>();		break;
					case 0xff02:	*value = timers_.read<2>();		break;
					case 0xff03:	*value = timers_.read<3>();		break;
					case 0xff04:	*value = timers_.read<4>();		break;
					case 0xff05:	*value = timers_.read<5>();		break;

					case 0xff08:	*value = keyboard_latch_;		break;

					case 0xff09:	*value = interrupts_.status();	break;
					case 0xff0a:	*value = interrupts_.mask();	break;

					case 0xff06:	*value = video_.read<0xff06>();	break;
					case 0xff07:	*value = video_.read<0xff07>();	break;
					case 0xff0b:	*value = video_.read<0xff0b>();	break;
					case 0xff1c:	*value = video_.read<0xff1c>();	break;
					case 0xff1d:	*value = video_.read<0xff1d>();	break;

					case 0xff12:	*value = ff12_;	break;
					case 0xff13:	*value = ff13_ | (rom_is_paged_ ? 1 : 0);	break;

					case 0xff14:	*value = video_.read<0xff14>();	break;
					case 0xff15:	*value = video_.read<0xff15>();	break;
					case 0xff16:	*value = video_.read<0xff16>();	break;
					case 0xff17:	*value = video_.read<0xff17>();	break;
					case 0xff18:	*value = video_.read<0xff18>();	break;
					case 0xff19:	*value = video_.read<0xff19>();	break;

					default:
						printf("TODO: TED read at %04x\n", address);
				}
			} else {
				switch(address) {
					case 0xff00:	timers_.write<0>(*value);	break;
					case 0xff01:	timers_.write<1>(*value);	break;
					case 0xff02:	timers_.write<2>(*value);	break;
					case 0xff03:	timers_.write<3>(*value);	break;
					case 0xff04:	timers_.write<4>(*value);	break;
					case 0xff05:	timers_.write<5>(*value);	break;

					case 0xff08:
						keyboard_latch_ = ~(
							((*value & 0x01) ? 0x00 : key_states_[0]) |
							((*value & 0x02) ? 0x00 : key_states_[1]) |
							((*value & 0x04) ? 0x00 : key_states_[2]) |
							((*value & 0x08) ? 0x00 : key_states_[3]) |
							((*value & 0x10) ? 0x00 : key_states_[4]) |
							((*value & 0x20) ? 0x00 : key_states_[5]) |
							((*value & 0x40) ? 0x00 : key_states_[6]) |
							((*value & 0x80) ? 0x00 : key_states_[7])
						);
					break;

					case 0xff09:
						interrupts_.set_status(*value);
					break;
					case 0xff0a:
						interrupts_.set_mask(*value);
						video_.write<0xff0a>(*value);
					break;
					case 0xff0b:	video_.write<0xff0b>(*value);	break;

					case 0xff06:	video_.write<0xff06>(*value);	break;
					case 0xff07:	video_.write<0xff07>(*value);	break;
					case 0xff0c:	video_.write<0xff0c>(*value);	break;
					case 0xff0d:	video_.write<0xff0d>(*value);	break;
					case 0xff12:
						ff12_ = *value & 0x3f;
						video_.write<0xff12>(*value);

						if((*value & 4)) {
							page_video_rom();
						} else {
							page_video_ram();
						}
					break;
					case 0xff13:
						ff13_ = *value & 0xfe;
						video_.write<0xff13>(*value);
					break;
					case 0xff14:	video_.write<0xff14>(*value);	break;
					case 0xff1a:	video_.write<0xff1a>(*value);	break;
					case 0xff1b:	video_.write<0xff1b>(*value);	break;

					case 0xff15:	video_.write<0xff15>(*value);	break;
					case 0xff16:	video_.write<0xff16>(*value);	break;
					case 0xff17:	video_.write<0xff17>(*value);	break;
					case 0xff18:	video_.write<0xff18>(*value);	break;
					case 0xff19:	video_.write<0xff19>(*value);	break;

					case 0xff3e:	page_cpu_rom();					break;
					case 0xff3f:	page_cpu_ram();					break;

					// TODO: audio is 0xff10, 0xff11, 0xff0e, 0xff0f and shares 0xff18.

					default:
						printf("TODO: TED write at %04x\n", address);
				}
			}
		}

		return length;
	}

private:
	CPU::MOS6502::Processor<CPU::MOS6502::Personality::P6502, ConcreteMachine, true> m6502_;

	void set_irq_line(bool active) override {
		m6502_.set_irq_line(active);
	}
	void set_ready_line(bool active) override {
		m6502_.set_ready_line(active);
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
	}
	void page_cpu_ram() {
		map_.page<PagerSide::Read, 0x8000, 32768>(&ram_[0x8000]);
		rom_is_paged_ = false;
	}
	bool rom_is_paged_ = false;

	void set_scan_target(Outputs::Display::ScanTarget *const target) final {
		video_.set_scan_target(target);
	}

	Outputs::Display::ScanStatus get_scaled_scan_status() const final {
		return video_.get_scaled_scan_status();
	}

	void run_for(const Cycles cycles) final {
		m6502_.run_for(cycles);
	}

	bool insert_media(const Analyser::Static::Media &media) final {
		if(!media.tapes.empty()) {
			tape_player_->set_tape(media.tapes[0]);
		}

		return true;
	}

	Commodore::Plus4::Pager map_;
	Commodore::Plus4::Pager video_map_;
	std::array<uint8_t, 65536> ram_;
	std::vector<uint8_t> kernel_;
	std::vector<uint8_t> basic_;
	uint8_t ff12_, ff13_;

	Interrupts interrupts_;
	Cycles timers_subcycles_;
	Timers timers_;
	Video video_;

	// MARK: - MappedKeyboardMachine.
	MappedKeyboardMachine::KeyboardMapper *get_keyboard_mapper() override {
		static Commodore::Plus4::KeyboardMapper keyboard_mapper_;
		return &keyboard_mapper_;
	}

	void set_key_state(uint16_t key, bool is_pressed) override {
		if(is_pressed) {
			key_states_[line(key)] |= mask(key);
		} else {
			key_states_[line(key)] &= ~mask(key);
		}
	}

	std::array<uint8_t, 8> key_states_{};
	uint8_t keyboard_latch_ = 0xff;

	Cycles media_divider_;
	std::unique_ptr<Storage::Tape::BinaryTapePlayer> tape_player_;
	uint8_t io_direction_ = 0x00, io_output_ = 0x00;
};

}
std::unique_ptr<Machine> Machine::Plus4(
	const Analyser::Static::Target *target,
	const ROMMachine::ROMFetcher &rom_fetcher
) {
	using Target = Analyser::Static::Commodore::Target;
	const Target *const commodore_target = dynamic_cast<const Target *>(target);
	return std::make_unique<ConcreteMachine>(*commodore_target, rom_fetcher);
}
