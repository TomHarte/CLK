//
//  BBCMicro.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/09/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#include "BBCMicro.hpp"

#include "Machines/MachineTypes.hpp"

#include "Components/6522/6522.hpp"
#include "Components/SN76489/SN76489.hpp"
#include "Processors/6502/6502.hpp"

#include "Analyser/Static/Acorn/Target.hpp"
#include "Outputs/Log.hpp"

#include "Outputs/Speaker/Implementation/LowpassSpeaker.hpp"
#include "Concurrency/AsyncTaskQueue.hpp"

#include <array>
#include <bitset>
#include <cassert>
#include <cstdint>

namespace BBCMicro {

namespace {
using Logger = Log::Logger<Log::Source::BBCMicro>;

struct Audio {
	Audio() :
		sn76489_(TI::SN76489::Personality::SN76489, audio_queue_),
		speaker_(sn76489_)
	{
		// I'm *VERY* unsure about this.
		speaker_.set_input_rate(2'000'000.0f);
	}

	~Audio() {
		audio_queue_.flush();
	}

	TI::SN76489 *operator ->() {
		flush();
		return &sn76489_;
	}

	void operator +=(const HalfCycles duration) {
		speaker_.run_for(audio_queue_, time_since_update_.flush<Cycles>());
		time_since_update_ += duration;
	}

	void flush() {
		audio_queue_.perform();
	}

	Outputs::Speaker::Speaker *speaker() {
		return &speaker_;
	}

private:
	Concurrency::AsyncTaskQueue<false> audio_queue_;
	TI::SN76489 sn76489_;
	Outputs::Speaker::PullLowpass<TI::SN76489> speaker_;
	HalfCycles time_since_update_;
};

struct UserVIAPortHandler: public MOS::MOS6522::IRQDelegatePortHandler {
};

struct SystemVIAPortHandler: public MOS::MOS6522::IRQDelegatePortHandler {
	SystemVIAPortHandler(Audio &audio) : audio_(audio) {}

	// CA2: key pressed;
	// CA1: vertical sync;
	// CB2: lightpen strobe offscreen;
	// CB1: ADC conversion complete.

	template <MOS::MOS6522::Port port>
	void set_port_output(const uint8_t value, uint8_t) {
		if(port == MOS::MOS6522::Port::A) {
			Logger::info().append("Port A write: %02x", value);
			return;
		}

		// The addressable latch.
		//
		// B0: enable writes to the sound generator;
		// B1, B2: read/write to the sound processor;
		// B3: enable writes to the keyboard.
		const auto mask = uint8_t(1 << (value & 7));
		const auto old_latch = latch_;
		latch_ = (latch_ & ~mask) | ((value & 8) ? mask : 0);

		// Check for a strobe on the audio output.
		if((old_latch^latch_) & old_latch & 1) {
			audio_->write(port_a_output_);
		}
		Logger::info().append("Programmable latch: %d%d%d%d", bool(latch_ & 8), bool(latch_ & 4), bool(latch_ & 2), bool(latch_ & 1));
	}

	template <MOS::MOS6522::Port port>
	uint8_t get_port_input() const {
		if(port == MOS::MOS6522::Port::B) {
			// TODO:
			//
			//	b4/5: joystick fire buttons;
			//	b6/7: speech interrupt/ready inputs.

			Logger::info().append("Port B read");
			return 0xff;
		}

		Logger::info().append("Port A read");
		return 0xff;
	}

private:
	uint8_t latch_ = 0;
	uint8_t port_a_output_ = 0;
	Audio &audio_;
};

}

class ConcreteMachine:
	public Machine,
	public MachineTypes::AudioProducer,
	public MachineTypes::ScanProducer,
	public MachineTypes::TimedMachine,
	public MOS::MOS6522::IRQDelegatePortHandler::Delegate
{
public:
	ConcreteMachine(
		const Analyser::Static::Acorn::BBCMicroTarget &target,
		const ROMMachine::ROMFetcher &rom_fetcher
	) :
		m6502_(*this),
		system_via_port_handler_(audio_),
		user_via_(user_via_port_handler_),
		system_via_(system_via_port_handler_)
	{
		set_clock_rate(2'000'000);

		system_via_port_handler_.set_interrupt_delegate(this);
		user_via_port_handler_.set_interrupt_delegate(this);

		// Grab ROMs.
		using Request = ::ROM::Request;
		using Name = ::ROM::Name;
		const auto request = Request(Name::AcornBASICII) && Request(Name::BBCMicroMOS12);
		auto roms = rom_fetcher(request);
		if(!request.validate(roms)) {
			throw ROMMachine::Error::MissingROMs;
		}

		const auto os_data = roms.find(Name::BBCMicroMOS12)->second;
		std::copy(os_data.begin(), os_data.end(), os_.begin());

		install_sideways(15, roms.find(Name::AcornBASICII)->second, false);

		// Setup fixed parts of memory map.
		page(0, &ram_[0], true);
		page(1, &ram_[1], true);
		page_sideways(15);
		page(3, os_.data(), true);

		(void)target;
	}

	// MARK: - 6502 bus.
	Cycles perform_bus_operation(
		const CPU::MOS6502::BusOperation operation,
		const uint16_t address,
		uint8_t *const value
	) {
		// Returns @c true if @c address is a device on the 1Mhz bus; @c false otherwise.
		static constexpr auto is_1mhz = [](const uint16_t address) {
			// Fast exit if outside the IO space.
			if(address < 0xfc00) return false;
			if(address >= 0xff00) return false;

			// Pages FC ('Fred'), FD ('Jim').
			if(address < 0xfe00) return true;

			// The 6845, 6850 and serial ULA.
			if(address < 0xfe18) return true;

			// The two VIAs.
			if(address >= 0xfe40 && address < 0xfe80) return true;

			// The ADC.
			if(address >= 0xfec0 && address < 0xfee0) return true;

			// Otherwise: in IO space, but not a 1Mhz device.
			return false;
		};

		// Determine whether this access hits the 1Mhz bus; if so then apply appropriate penalty, and update phase.
		const auto duration = is_1mhz(address) ? Cycles(2 + (phase_&1)) : Cycles(1);
		phase_ += duration.as<int>();


		//
		// Dependent device updates.
		//
		const auto half_cycles = HalfCycles(duration.as_integral());
		audio_ += half_cycles;
		system_via_.run_for(half_cycles);
		user_via_.run_for(half_cycles);


		//
		// Check for an IO access; if found then perform that and exit.
		//
		if(address >= 0xfc00 && address < 0xff00) {
			if(address >= 0xfe40 && address < 0xfe60) {
				if(is_read(operation)) {
					*value = system_via_.read(address);
				} else {
					system_via_.write(address, *value);
				}
			} else if(address >= 0xfe60 && address < 0xfe80) {
				if(is_read(operation)) {
					*value = user_via_.read(address);
				} else {
					user_via_.write(address, *value);
				}
			} else {
				Logger::error().append("Unhandled IO access at %04x", address);
			}
			return duration;
		}

		//
		// ROM or RAM access.
		//
		if(is_read(operation)) {
			*value = memory_[address >> 14][address];
		} else {
			if(memory_write_masks_[address >> 14]) {
				memory_[address >> 14][address] = *value;
			}
		}

		return duration;
	}

private:
	// MARK: - AudioProducer.
	Outputs::Speaker::Speaker *get_speaker() override {
		return audio_.speaker();
	}

	// MARK: - ScanProducer.
	void set_scan_target(Outputs::Display::ScanTarget *) override {}
	Outputs::Display::ScanStatus get_scan_status() const override {
		return Outputs::Display::ScanStatus{};
	}

	// MARK: - TimedMachine.
	void run_for(const Cycles cycles) override {
		m6502_.run_for(cycles);
	}

	void flush_output(const int outputs) final {
		if(outputs & Output::Audio) {
			audio_.flush();
		}
	}

	// MARK: - IRQDelegatePortHandler::Delegate.
	void mos6522_did_change_interrupt_status(void *) override {
		update_irq_line();
	}

	// MARK: - Clock phase.
	int phase_ = 0;

	// MARK: - Memory.
	std::array<uint8_t, 32 * 1024> ram_;
	using ROM = std::array<uint8_t, 16 * 1024>;
	ROM os_;
	std::array<ROM, 16> roms_;

	std::bitset<16> rom_inserted_;
	std::bitset<16> rom_write_masks_;

	uint8_t *memory_[4];
	std::bitset<4> memory_write_masks_;
	bool sideways_read_mask_ = false;
	void page(const size_t slot, uint8_t *const source, bool is_writeable) {
		memory_[slot] = source - (slot * 16384);
		memory_write_masks_[slot] = is_writeable;
	}

	void page_sideways(const size_t source) {
		sideways_read_mask_ = rom_inserted_[source];
		page(2, roms_[source].data(), rom_write_masks_[source]);
	}

	void install_sideways(const size_t slot, const std::vector<uint8_t> &source, bool is_writeable) {
		rom_write_masks_[slot] = is_writeable;

		assert(source.size() == roms_[slot].size());
		std::copy(source.begin(), source.end(), roms_[slot].begin());
	}

	// MARK: - Components.
	CPU::MOS6502::Processor<CPU::MOS6502::Personality::P6502, ConcreteMachine, false> m6502_;

	UserVIAPortHandler user_via_port_handler_;
	SystemVIAPortHandler system_via_port_handler_;
	MOS::MOS6522::MOS6522<UserVIAPortHandler> user_via_;
	MOS::MOS6522::MOS6522<SystemVIAPortHandler> system_via_;

	void update_irq_line() {
		m6502_.set_irq_line(
			user_via_.get_interrupt_line() ||
			system_via_.get_interrupt_line()
		);
	}

	Audio audio_;
};

}

using namespace BBCMicro;

std::unique_ptr<Machine> Machine::BBCMicro(
	const Analyser::Static::Target *target,
	const ROMMachine::ROMFetcher &rom_fetcher
) {
	using Target = Analyser::Static::Acorn::BBCMicroTarget;
	const Target *const acorn_target = dynamic_cast<const Target *>(target);
	return std::make_unique<BBCMicro::ConcreteMachine>(*acorn_target, rom_fetcher);
}
