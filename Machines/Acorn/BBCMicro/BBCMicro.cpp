//
//  BBCMicro.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/09/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#include "BBCMicro.hpp"

#include "Machines/MachineTypes.hpp"
#include "Machines/Utility/MemoryFuzzer.hpp"

#include "Processors/6502/6502.hpp"

#include "Components/6522/6522.hpp"
#include "Components/6845/CRTC6845.hpp"
#include "Components/SN76489/SN76489.hpp"
#include "Components/6850/6850.hpp"

#include "Analyser/Static/Acorn/Target.hpp"
#include "Outputs/Log.hpp"

#include "Outputs/CRT/CRT.hpp"
#include "Outputs/Speaker/Implementation/LowpassSpeaker.hpp"
#include "Concurrency/AsyncTaskQueue.hpp"

#include <array>
#include <bitset>
#include <cassert>
#include <cstdint>

namespace BBCMicro {

namespace {
using Logger = Log::Logger<Log::Source::BBCMicro>;

/*!
	Combines an SN76489 with an appropriate asynchronous queue and filtering speaker.
*/
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

/*!
	Models the user-port VIA.
*/
struct UserVIAPortHandler: public MOS::MOS6522::IRQDelegatePortHandler {
};
using UserVIA = MOS::MOS6522::MOS6522<UserVIAPortHandler>;

/*!
	Models the system VIA, which connects to the SN76489 and the keyboard.
*/
struct SystemVIAPortHandler: public MOS::MOS6522::IRQDelegatePortHandler {
	SystemVIAPortHandler(Audio &audio) : audio_(audio) {}

	// CA2: key pressed;
	// CA1: vertical sync;
	// CB2: lightpen strobe offscreen;
	// CB1: ADC conversion complete.

	template <MOS::MOS6522::Port port>
	void set_port_output(const uint8_t value, uint8_t) {
		if(port == MOS::MOS6522::Port::A) {
//			Logger::info().append("Port A write: %02x", value);
			port_a_output_ = value;
			return;
		}

		// The addressable latch.
		//
		// B0: enable writes to the sound generator;
		// B1, B2: read/write to the sound processor;
		// B3: enable writes to the keyboard.
		// B4/B5: "hardware scrolling" (new base address > 32768?)
		// B6/B7: keyboard LEDs.
		const auto mask = uint8_t(1 << (value & 7));
		const auto old_latch = latch_;
		latch_ = (latch_ & ~mask) | ((value & 8) ? mask : 0);

		// Check for a strobe on the audio output.
		if((old_latch^latch_) & old_latch & 1) {
			audio_->write(port_a_output_);
		}

		// Update keyboard LEDs.
		if(mask >= 0x40) {
			Logger::info().append("CAPS: %d SHIFT: %d", bool(latch_ & 0x40), bool(latch_ & 0x40));
		}
	}

	template <MOS::MOS6522::Port port>
	uint8_t get_port_input() const {
		if(port == MOS::MOS6522::Port::B) {
			// TODO:
			//
			//	b4/5: joystick fire buttons;
			//	b6/7: speech interrupt/ready inputs.

			Logger::info().append("Port B read");
			return 0x3f;	// b6 = b7 = 0 => no speech hardware?
		}

		if(latch_ & 0b1000) {
			return 0xff;
		}

		// Read keyboard. Low six bits of output are key to check, state should be returned in high bit.
		Logger::info().append("Keyboard read from key %d", port_a_output_);
		switch(port_a_output_ & 0x7f) {
			default:	return 0x00;	// Default: key not pressed.

			case 9:		return 0x00;	//
			case 8:		return 0x00;	// Startup mode.	(= mode 7?)
			case 7:		return 0x00;	//
		}
	}

private:
	uint8_t latch_ = 0;
	uint8_t port_a_output_ = 0;
	Audio &audio_;
};
using SystemVIA = MOS::MOS6522::MOS6522<SystemVIAPortHandler>;

/*!
	Handles CRTC bus activity.
*/
class CRTCBusHandler {
public:
	CRTCBusHandler(const uint8_t *const ram, SystemVIA &system_via) :
		crt_(1024, 1, Outputs::Display::Type::PAL50, Outputs::Display::InputDataType::Red1Green1Blue1),
		ram_(ram),
		system_via_(system_via) {}

	void set_palette(const uint8_t value) {
		const auto index = value >> 4;
		Logger::info().append("Palette entry %d set to %x", index, value & 0xf);
	}

	void set_control(const uint8_t value) {
		Logger::info().append("Video control set to %x", value);
		cycle_length_ = (value & 0x10) ? 8 : 16;
		Logger::info().append("TODO: video control => flash %d", bool(value & 0x01));
		Logger::info().append("TODO: video control => teletext %d", bool(value & 0x02));
		Logger::info().append("TODO: video control => columns %d", (value >> 2) & 0x03);
		Logger::info().append("TODO: video control => cursor segment %d%d%d", bool(value & 0x80), bool(value & 0x40), bool(value & 0x20));
	}

	/*!
		The CRTC entry function for the main part of each clock cycle; takes the current
		bus state and determines what output to produce based on the current palette and mode.
	*/
	void perform_bus_cycle(const Motorola::CRTC::BusState &state) {
		system_via_.set_control_line_input<MOS::MOS6522::Port::A, MOS::MOS6522::Line::One>(state.vsync);

//		bool print = false;
//		uint16_t start_address = 0x7c00;
//		int rows = 24;
//		if(print) {
//			for(int y = 0; y < rows; y++) {
//				for(int x = 0; x < 40; x++) {
//					printf("%c", ram_[start_address + y*40 + x]);
//				}
//				printf("\n");
//			}
//		}

		// Count cycles since horizontal sync to insert a colour burst.
		if(state.hsync) {
			++cycles_into_hsync_;
		} else {
			cycles_into_hsync_ = 0;
		}
		const bool is_colour_burst = (cycles_into_hsync_ >= 5 && cycles_into_hsync_ < 9);

		// Sync is taken to override pixels, and is combined as a simple OR.
		const bool is_sync = state.hsync || state.vsync;
		const bool is_blank = !is_sync && state.hsync;

		OutputMode output_mode;
		if(is_sync) {
			output_mode = OutputMode::Sync;
		} else if(is_colour_burst) {
			output_mode = OutputMode::ColourBurst;
		} else if(is_blank) {
			output_mode = OutputMode::Blank;
		} else if(state.display_enable) {
			output_mode = OutputMode::Pixels;
		} else {
			output_mode = OutputMode::Border;
		}

		// If a transition between sync/border/pixels just occurred, flush whatever was
		// in progress to the CRT and reset counting.
		if(output_mode != previous_output_mode_) {
			if(cycles_) {
				switch(previous_output_mode_) {
					default:
					case OutputMode::Blank:			crt_.output_blank(cycles_);					break;
					case OutputMode::Sync:			crt_.output_sync(cycles_);					break;
					case OutputMode::Border:
//						crt_.output_level(cycles_, 0xff);
						crt_.output_blank(cycles_);
					break;
					case OutputMode::ColourBurst:	crt_.output_default_colour_burst(cycles_);	break;
					case OutputMode::Pixels:
						crt_.output_level(cycles_, 0xff);
//						pixel_pointer_ = pixel_data_ = nullptr;
					break;
				}
			}

			cycles_ = 0;
			previous_output_mode_ = output_mode;
		}

		// Increment cycles since state changed.
		cycles_ += cycle_length_;

//		// Collect some more pixels if output is ongoing.
//		if(previous_output_mode_ == OutputMode::Pixels) {
//			if(!pixel_data_) {
//				pixel_pointer_ = pixel_data_ = crt_.begin_data(320, 8);
//			}
//			if(pixel_pointer_) {
//				// the CPC shuffles output lines as:
//				//	MA13 MA12	RA2 RA1 RA0		MA9 MA8 MA7 MA6 MA5 MA4 MA3 MA2 MA1 MA0		CCLK
//				// ... so form the real access address.
//				const uint16_t address =
//					uint16_t(
//						((state.refresh_address & 0x3ff) << 1) |
//						((state.row_address & 0x7) << 11) |
//						((state.refresh_address & 0x3000) << 2)
//					);
//
//				// Fetch two bytes and translate into pixels. Guaranteed: the mode can change only at
//				// hsync, so there's no risk of pixel_pointer_ overrunning 320 output pixels without
//				// exactly reaching 320 output pixels.
//				switch(mode_) {
//					case 0:
//						reinterpret_cast<uint16_t *>(pixel_pointer_)[0] = mode0_output_[ram_[address]];
//						reinterpret_cast<uint16_t *>(pixel_pointer_)[1] = mode0_output_[ram_[address+1]];
//						pixel_pointer_ += 2 * sizeof(uint16_t);
//					break;
//
//					case 1:
//						reinterpret_cast<uint32_t *>(pixel_pointer_)[0] = mode1_output_[ram_[address]];
//						reinterpret_cast<uint32_t *>(pixel_pointer_)[1] = mode1_output_[ram_[address+1]];
//						pixel_pointer_ += 2 * sizeof(uint32_t);
//					break;
//
//					case 2:
//						reinterpret_cast<uint64_t *>(pixel_pointer_)[0] = mode2_output_[ram_[address]];
//						reinterpret_cast<uint64_t *>(pixel_pointer_)[1] = mode2_output_[ram_[address+1]];
//						pixel_pointer_ += 2 * sizeof(uint64_t);
//					break;
//
//					case 3:
//						reinterpret_cast<uint16_t *>(pixel_pointer_)[0] = mode3_output_[ram_[address]];
//						reinterpret_cast<uint16_t *>(pixel_pointer_)[1] = mode3_output_[ram_[address+1]];
//						pixel_pointer_ += 2 * sizeof(uint16_t);
//					break;
//
//				}
//
//				// Flush the current buffer pixel if full; the CRTC allows many different display
//				// widths so it's not necessarily possible to predict the correct number in advance
//				// and using the upper bound could lead to inefficient behaviour.
//				if(pixel_pointer_ == pixel_data_ + 320) {
//					crt_.output_data(cycles_ * 16, size_t(cycles_ * 16 / pixel_divider_));
//					pixel_pointer_ = pixel_data_ = nullptr;
//					cycles_ = 0;
//				}
//			}
//		}
	}

	/// Sets the destination for output.
	void set_scan_target(Outputs::Display::ScanTarget *const scan_target) {
		crt_.set_scan_target(scan_target);
	}

	/// @returns The current scan status.
	Outputs::Display::ScanStatus get_scaled_scan_status() const {
		return crt_.get_scaled_scan_status();
	}

	/// Sets the type of display.
	void set_display_type(const Outputs::Display::DisplayType display_type) {
		crt_.set_display_type(display_type);
	}

	/// Gets the type of display.
	Outputs::Display::DisplayType get_display_type() const {
		return crt_.get_display_type();
	}


private:
	enum class OutputMode {
		Sync,
		Blank,
		ColourBurst,
		Border,
		Pixels
	} previous_output_mode_ = OutputMode::Sync;
	int cycles_ = 0;
	int cycles_into_hsync_ = 0;
	int cycle_length_ = 8;

	Outputs::CRT::CRT crt_;
	uint8_t *pixel_data_ = nullptr, *pixel_pointer_ = nullptr;

	const uint8_t *const ram_ = nullptr;
	SystemVIA &system_via_;
};
using CRTC = Motorola::CRTC::CRTC6845<
	CRTCBusHandler,
	Motorola::CRTC::Personality::HD6845S,
	Motorola::CRTC::CursorType::None>;
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
		system_via_(system_via_port_handler_),
		crtc_bus_handler_(ram_.data(), system_via_),
		crtc_(crtc_bus_handler_),
		acia_(HalfCycles(2'000'000)) // TODO: look up real ACIA clock rate.
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
		page(1, &ram_[16384], true);
		page_sideways(15);
		page(3, os_.data(), false);
		Memory::Fuzz(ram_);

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
		// 1Mhz devices.
		//
		const auto half_cycles = HalfCycles(duration.as_integral());
		audio_ += half_cycles;
		system_via_.run_for(half_cycles);
		user_via_.run_for(half_cycles);


		//
		// 2Mhz devices.
		//
		// TODO: if CRTC clock is 1Mhz, adapt.
		if(crtc_2mhz_) {
			crtc_.run_for(duration);
		} else {
			// TODO: possibly skip one cycle if clock speed just changed partway through a 1Mhz window?
			const auto cycles = (phase_ >> 1) - ((phase_ - duration.as<int>()) >> 1);
			crtc_.run_for(Cycles(cycles));
		}


		//
		// Questionably-clocked devices.
		//
		acia_.run_for(half_cycles);


		//
		// Check for an IO access; if found then perform that and exit.
		//
//		static bool log = false;
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
			} else if(address == 0xfe30) {
				if(is_read(operation)) {
					*value = 0xfe;
				} else {
					page_sideways(*value & 0xf);
				}
			} else if(address >= 0xfe00 && address < 0xfe08) {
				if(is_read(operation)) {
					if(address & 1) {
						*value = crtc_.get_register();
					} else {
						*value = crtc_.get_status();
					}
				} else {
					if(address & 1) {
						crtc_.set_register(*value);
					} else {
						crtc_.select_register(*value);
					}
				}
			} else if(address >= 0xfe20 && address < 0xfe30) {
				if(is_read(operation)) {
					*value = 0xfe;
				} else {
					switch(address) {
						case 0xfe20:
							crtc_bus_handler_.set_control(*value);
							crtc_2mhz_ = *value & 0x10;
						break;
						case 0xfe21:
							crtc_bus_handler_.set_palette(*value);
						break;
					}
				}
			} else if(address == 0xfee0) {
				if(is_read(operation)) {
					Logger::info().append("Read tube status: 0");
					*value = 0;
				} else {
					Logger::info().append("Wrote tube: %02x", *value);
				}
			} else if(address >= 0xfe08 && address < 0xfe10) {
				if(is_read(operation)) {
					Logger::info().append("ACIA read");
					*value = acia_.read(address);
				} else {
					Logger::info().append("ACIA write: %02x", *value);
					acia_.write(address, *value);
				}
			}
			else {
				Logger::error()
					.append("Unhandled IO %s at %04x", is_read(operation) ? "read" : "write", address)
					.append_if(!is_read(operation), ": %02x", *value);
			}
			return duration;
		}

		//
		// ROM or RAM access.
		//
//		if(operation == CPU::MOS6502Esque::BusOperation::ReadOpcode) {
//			log |= address == 0xc4c0;
//
//			if(log) {
//				printf("%04x\n", address);
//			}
//		}

		if(is_read(operation)) {
			// TODO: probably don't do this with this condition? See how it compiles. If it's a CMOV somehow, no problem.
			if((address >> 14) == 2 && !sideways_read_mask_) {
				*value = 0xff;
			} else {
				*value = memory_[address >> 14][address];
			}
		} else {
			if(memory_write_masks_[address >> 14]) {
				memory_[address >> 14][address] = *value;

				if(address >= 0x7c00 && *value) {
					Logger::info().append("Output character: %c", *value);
				}
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
	void set_scan_target(Outputs::Display::ScanTarget *const target) override {
		crtc_bus_handler_.set_scan_target(target);
	}

	Outputs::Display::ScanStatus get_scaled_scan_status() const override {
		return crtc_bus_handler_.get_scaled_scan_status();
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
		rom_inserted_[slot] = true;

		assert(source.size() == roms_[slot].size());
		std::copy(source.begin(), source.end(), roms_[slot].begin());
	}

	// MARK: - Components.
	CPU::MOS6502::Processor<CPU::MOS6502::Personality::P6502, ConcreteMachine, false> m6502_;

	UserVIAPortHandler user_via_port_handler_;
	SystemVIAPortHandler system_via_port_handler_;
	UserVIA user_via_;
	SystemVIA system_via_;

	void update_irq_line() {
		m6502_.set_irq_line(
			user_via_.get_interrupt_line() ||
			system_via_.get_interrupt_line()
		);
	}

	Audio audio_;

	CRTCBusHandler crtc_bus_handler_;
	CRTC crtc_;
	bool crtc_2mhz_ = true;

	Motorola::ACIA::ACIA acia_;
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
