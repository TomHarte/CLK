//
//  BBCMicro.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/09/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#include "BBCMicro.hpp"

#include "Activity/Source.hpp"

#include "Machines/MachineTypes.hpp"
#include "Machines/Utility/MemoryFuzzer.hpp"

#include "Processors/6502/6502.hpp"

#include "Components/6522/6522.hpp"
#include "Components/6845/CRTC6845.hpp"
#include "Components/SN76489/SN76489.hpp"
#include "Components/6850/6850.hpp"
#include "Components/uPD7002/uPD7002.hpp"

// TODO: factor this more appropriately.
#include "Machines/Acorn/Electron/Plus3.hpp"

#include "Analyser/Static/Acorn/Target.hpp"
#include "Outputs/Log.hpp"

#include "Outputs/CRT/CRT.hpp"
#include "Outputs/Speaker/Implementation/LowpassSpeaker.hpp"
#include "Concurrency/AsyncTaskQueue.hpp"

#include "Keyboard.hpp"

#include <algorithm>
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
		sn76489_(TI::SN76489::Personality::SN76489, audio_queue_, 2),
		speaker_(sn76489_)
	{
		// Combined with the additional divider specified above, implies this chip is clocked at 4Mhz.
		speaker_.set_input_rate(2'000'000.0f);
	}

	~Audio() {
		audio_queue_.flush();
	}

	TI::SN76489 *operator ->() {
		speaker_.run_for(audio_queue_, time_since_update_.flush<Cycles>());
		return &sn76489_;
	}

	void operator +=(const Cycles duration) {
		time_since_update_ += duration;
	}

	void flush() {
		speaker_.run_for(audio_queue_, time_since_update_.flush<Cycles>());
		audio_queue_.perform();
	}

	Outputs::Speaker::Speaker *speaker() {
		return &speaker_;
	}

private:
	Concurrency::AsyncTaskQueue<false> audio_queue_;
	TI::SN76489 sn76489_;
	Outputs::Speaker::PullLowpass<TI::SN76489> speaker_;
	Cycles time_since_update_;
};

/*!
	Models the user-port VIA.
*/
struct UserVIAPortHandler: public MOS::MOS6522::IRQDelegatePortHandler {
};
using UserVIA = MOS::MOS6522::MOS6522<UserVIAPortHandler>;

/*!
	Target for the video base address.
*/
struct VideoBaseAddress {
	void set_video_base(const uint8_t code) {
		switch(code) {
			case 0b00:	video_base_ = 0x4000;	break;
			case 0b01:	video_base_ = 0x6000;	break;
			case 0b10:	video_base_ = 0x3000;	break;
			case 0b11:	video_base_ = 0x5800;	break;
		}
	}

protected:
	uint16_t video_base_ = 0;
};

/*!
	Models the system VIA, which connects to the SN76489 and the keyboard.
*/
struct SystemVIAPortHandler;
using SystemVIA = MOS::MOS6522::MOS6522<SystemVIAPortHandler>;

struct SystemVIAPortHandler: public MOS::MOS6522::IRQDelegatePortHandler {
	SystemVIAPortHandler(Audio &audio, VideoBaseAddress &video_base, SystemVIA &via) :
		audio_(audio), video_base_(video_base), via_(via)
	{
		// Set initial mode to mode 0.
		set_key(7, true);
		set_key(8, true);
		set_key(9, true);
	}

	// CA2: key pressed;
	// CA1: vertical sync;
	// CB2: lightpen strobe offscreen;
	// CB1: ADC conversion complete.

	template <MOS::MOS6522::Port port>
	void set_port_output(const uint8_t value, uint8_t) {
		if(port == MOS::MOS6522::Port::A) {
			port_a_output_ = value;
			update_ca2();
			return;
		}

		// The addressable latch.
		//
		// B0: enable writes to the sound generator;
		// B1, B2: read/write to the speech processor;
		// B3: keyboard scanning mode; 1 => automatic; 0 => programmatic;
		// B4/B5: hardware scrolling;
		// B6/B7: keyboard LEDs.
		const auto mask = uint8_t(1 << (value & 7));
		const auto old_latch = latch_;
		latch_ = (latch_ & ~mask) | ((value & 8) ? mask : 0);

		// Check for a strobe on the audio output.
		if((old_latch^latch_) & old_latch & LatchFlags::WriteToSN76489) {
			audio_->write(port_a_output_);
		}

		// Pass on the video wraparound/base.
		video_base_.set_video_base((latch_ >> 4) & 3);

		// If keyboard scanning mode has changed, update CA2.
		if(mask == LatchFlags::KeyboardIsScanning) {
			update_ca2();
		}

		// Update keyboard LEDs.
		if(mask >= 0x40) {
			const bool new_caps = latch_ & 0x80;
			const bool new_shift = latch_ & 0x40;

			if(new_caps != caps_led_state_) {
				caps_led_state_ = new_caps;
				activity_observer_->set_led_status(caps_led, caps_led_state_);
			}

			if(new_shift != shift_led_state_) {
				shift_led_state_ = new_shift;
				activity_observer_->set_led_status(shift_led, shift_led_state_);
			}
		}
	}

	template <MOS::MOS6522::Port port>
	uint8_t get_port_input() const {
		if(port == MOS::MOS6522::Port::B) {
			// TODO:
			//
			//	b4/5: joystick fire buttons;
			//	b6/7: speech interrupt/ready inputs.
			return 0x3f;	// b6 = b7 = 0 => no speech hardware.
		}

		if(latch_ & LatchFlags::KeyboardIsScanning) {
			return 0xff;
		}

		// Read keyboard. Low six bits of output are key to check, state should be returned in high bit.
		const uint8_t key_state = key_column(port_a_output_)[key_row(port_a_output_)] ? 0x80 : 0x00;
		return key_state;
	}

	void set_key(const uint8_t key, const bool pressed) {
		key_column(key)[key_row(key)] = pressed;
		update_ca2();
	}

	void advance_keyboard_scan(const HalfCycles count) {
		if(!(latch_ & LatchFlags::KeyboardIsScanning)) {
			return;
		}

		const int ending_column = keyboard_scan_column_ + count.as<int>();
		int steps = (ending_column >> 1) - (keyboard_scan_column_ >> 1);
		while(steps--) {
			keyboard_scan_column_ += 2;
			update_ca2();
		}
		keyboard_scan_column_ = ending_column;
	}

	void set_activity_observer(Activity::Observer *const observer) {
		activity_observer_ = observer;

		if(activity_observer_) {
			activity_observer_->register_led(caps_led, Activity::Observer::LEDPresentation::Persistent);
			activity_observer_->register_led(shift_led, Activity::Observer::LEDPresentation::Persistent);

			activity_observer_->set_led_status(caps_led, caps_led_state_);
			activity_observer_->set_led_status(shift_led, shift_led_state_);
		}
	}

private:
	uint8_t latch_ = 0;
	enum LatchFlags: uint8_t {
		WriteToSN76489 = 1 << 0,
		KeyboardIsScanning = 1 << 3,
	};

	uint8_t port_a_output_ = 0;

	Audio &audio_;
	VideoBaseAddress &video_base_;

	SystemVIA &via_;

	// MARK: - Keyboard state and helpers.

	using KeyRow = std::bitset<8>;
	std::array<KeyRow, 16> key_states_{};
	int keyboard_scan_column_ = 0;

	KeyRow &key_column(const uint8_t key) {
		return key_states_[key & 0xf];
	}
	const KeyRow &key_column(const uint8_t key) const {
		return key_states_[key & 0xf];
	}
	static constexpr size_t key_row(const uint8_t key) {
		return (key >> 4) & 7;
	}

	void update_ca2() {
		const bool state = key_column(
			[&]() {
				if(latch_ & LatchFlags::KeyboardIsScanning) {
					return uint8_t(keyboard_scan_column_ >> 1);
				} else {
					return uint8_t(port_a_output_ & 0xf);
				}
		} ()).to_ulong() & 0xfe;	// Discard the first row.

		via_.set_control_line_input<MOS::MOS6522::Port::A, MOS::MOS6522::Line::Two>(state);
	}

	static inline const std::string caps_led = "CAPS";
	static inline const std::string shift_led = "SHIFT";
	bool caps_led_state_ = false;
	bool shift_led_state_ = false;
	Activity::Observer *activity_observer_ = nullptr;
};

/*!
	Handles CRTC bus activity.
*/
class CRTCBusHandler: public VideoBaseAddress {
public:
	CRTCBusHandler(const uint8_t *const ram, SystemVIA &system_via) :
		crt_(1024, 1, Outputs::Display::Type::PAL50, Outputs::Display::InputDataType::Red1Green1Blue1),
		ram_(ram),
		system_via_(system_via) {}

	void set_palette(const uint8_t value) {
		const auto index = value >> 4;
		palette_[index] = uint8_t(
			7 ^ (
				((value & 0b100) >> 2) |
				((value & 0b001) << 2) |
				(value & 0b010)
			)
		);
		flash_flags_[size_t(index)] = value & 0b1000;
	}

	void set_control(const uint8_t value) {
		crtc_clock_multiplier_ = (value & 0x10) ? 1 : 2;

		active_collation_.pixels_per_clock = 1 << ((value >> 2) & 0x03);
		active_collation_.is_teletext = value & 0x02;
		if(active_collation_.is_teletext) {
			Logger::error().append("TODO: video control => teletext %d", bool(value & 0x02));
		}

		flash_mask_ = value & 0x01 ? 7 : 0;
		cursor_mask_ = value & 0b1110'0000;
	}

	/*!
		The CRTC entry function for the main part of each clock cycle; takes the current
		bus state and determines what output to produce based on the current palette and mode.
	*/
	void perform_bus_cycle(const Motorola::CRTC::BusState &state) {
		system_via_.set_control_line_input<MOS::MOS6522::Port::A, MOS::MOS6522::Line::One>(state.vsync);

		// Count cycles since horizontal sync to insert a colour burst.
		if(state.hsync) {
			++cycles_into_hsync_;
		} else {
			cycles_into_hsync_ = 0;
		}
		const bool is_colour_burst = cycles_into_hsync_ >= 5 && cycles_into_hsync_ < 9;

		// Sync is taken to override pixels, and is combined as a simple OR.
		const bool is_sync = state.hsync || state.vsync;

		// Check for a cursor leading edge.
		cursor_shifter_ >>= 4;
		if(state.cursor != previous_cursor_enabled_) {
			if(state.cursor && state.display_enable) {	// TODO: should I have to test display enable here? Or should
														// the CRTC already have factored that in?
				cursor_shifter_ =
					((cursor_mask_ & 0x80) ? 0x0007 : 0) |
					((cursor_mask_ & 0x40) ? 0x0070 : 0) |
					((cursor_mask_ & 0x20) ? 0x7700 : 0);
			}
			previous_cursor_enabled_ = state.cursor;
		}

		OutputMode output_mode;
		const bool should_fetch = state.display_enable && !(state.row_address & 8);
		if(is_sync) {
			output_mode = OutputMode::Sync;
		} else if(is_colour_burst) {
			output_mode = OutputMode::ColourBurst;
		} else if(should_fetch || cursor_shifter_) {
			output_mode = OutputMode::Pixels;
		} else {
			output_mode = OutputMode::Blank;
		}

		// If a transition between sync/border/pixels just occurred, flush whatever was
		// in progress to the CRT and reset counting.
		if(output_mode != previous_output_mode_) {
			if(cycles_) {
				switch(previous_output_mode_) {
					default:
					case OutputMode::Blank:			crt_.output_blank(cycles_);					break;
					case OutputMode::Sync:			crt_.output_sync(cycles_);					break;
					case OutputMode::ColourBurst:	crt_.output_default_colour_burst(cycles_);	break;
					case OutputMode::Pixels:		flush_pixels();								break;
				}
			}

			cycles_ = 0;
			previous_output_mode_ = output_mode;
		}

		// Increment cycles since state changed.
		cycles_ += crtc_clock_multiplier_ << 3;

		// Collect some more pixels if output is ongoing.
		if(previous_output_mode_ == OutputMode::Pixels) {
			// Flush the current buffer pixel if full; the CRTC allows many different display
			// widths so it's not necessarily possible to predict the correct number in advance
			// and using the upper bound could lead to inefficient behaviour.
			if(pixel_data_ && (pixels_collected() == 320 || active_collation_ != previous_collation_)) {
				flush_pixels();
				cycles_ = 0;
			}
			previous_collation_ = active_collation_;

			if(!pixel_data_) {
				pixel_pointer_ = pixel_data_ = crt_.begin_data(320, 8);
			}
			if(pixel_pointer_) {
				uint16_t address;

				if(state.refresh_address & (1 << 13)) {
					// Teletext address generation mode.
					address = uint16_t(
						0x3c00 |
						((state.refresh_address & 0x800) << 3) |
						(state.refresh_address & 0x3ff)
					);
					// TODO: wraparound? Does that happen on Mode 7?
				} else {
					address = uint16_t((state.refresh_address << 3) | (state.row_address & 7));
					if(address & 0x8000) {
						address = (address + video_base_) & 0x7fff;
					}
				}

				// Hard coded: pixel mode!
				pixel_shifter_ = should_fetch ? ram_[address] : 0;
				switch(crtc_clock_multiplier_ * active_collation_.pixels_per_clock) {
					case 1: shift_pixels<1>(cursor_shifter_ & 7);	break;
					case 2: shift_pixels<2>(cursor_shifter_ & 7);	break;
					case 4: shift_pixels<4>(cursor_shifter_ & 7);	break;
					case 8: shift_pixels<8>(cursor_shifter_ & 7);	break;
					case 16: shift_pixels<16>(cursor_shifter_ & 7);	break;
					default: break;
				}
			}
		}
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
		Pixels
	};
	struct PixelCollation {
		int pixels_per_clock;
		bool is_teletext;

		bool operator !=(const PixelCollation &rhs) {
			if(is_teletext && rhs.is_teletext) return false;
			return pixels_per_clock != rhs.pixels_per_clock;
		}
	};

	OutputMode previous_output_mode_ = OutputMode::Sync;
	int cycles_ = 0;
	int cycles_into_hsync_ = 0;

	Outputs::CRT::CRT crt_;

	uint8_t *pixel_data_ = nullptr, *pixel_pointer_ = nullptr;
	size_t pixels_collected() const {
		return size_t(pixel_pointer_ - pixel_data_);
	}
	void flush_pixels() {
		crt_.output_data(cycles_, pixels_collected());
		pixel_pointer_ = pixel_data_ = nullptr;
	}
	PixelCollation previous_collation_;
	uint8_t palette_[16];
	std::bitset<16> flash_flags_;
	uint8_t flash_mask_ = 0;

	int crtc_clock_multiplier_ = 1;
	PixelCollation active_collation_;
	uint8_t pixel_shifter_ = 0;

	uint8_t cursor_mask_ = 0;
	uint32_t cursor_shifter_ = 0;
	bool previous_cursor_enabled_ = false;

	template <int count> void shift_pixels(const uint8_t cursor_mask) {
		for(int c = 0; c < count; c++) {
			const uint8_t colour =
				((pixel_shifter_ & 0x80) >> 4) |
				((pixel_shifter_ & 0x20) >> 3) |
				((pixel_shifter_ & 0x08) >> 2) |
				((pixel_shifter_ & 0x02) >> 1);
			pixel_shifter_ <<= 1;
			*pixel_pointer_++ = palette_[colour] ^ (flash_flags_[colour] ? flash_mask_ : 0x00) ^ cursor_mask;
		}
	}

	const uint8_t *const ram_ = nullptr;
	SystemVIA &system_via_;
};
using CRTC = Motorola::CRTC::CRTC6845<
	CRTCBusHandler,
	Motorola::CRTC::Personality::HD6845S,
	Motorola::CRTC::CursorType::MDA>;
}

template <bool has_1770>
class ConcreteMachine:
	public Activity::Source,
	public Machine,
	public MachineTypes::AudioProducer,
	public MachineTypes::MappedKeyboardMachine,
	public MachineTypes::MediaTarget,
	public MachineTypes::ScanProducer,
	public MachineTypes::TimedMachine,
	public MOS::MOS6522::IRQDelegatePortHandler::Delegate,
	public NEC::uPD7002::Delegate,
	public WD::WD1770::Delegate
{
public:
	ConcreteMachine(
		const Analyser::Static::Acorn::BBCMicroTarget &target,
		const ROMMachine::ROMFetcher &rom_fetcher
	) :
		m6502_(*this),
		system_via_port_handler_(audio_, crtc_bus_handler_, system_via_),
		user_via_(user_via_port_handler_),
		system_via_(system_via_port_handler_),
		crtc_bus_handler_(ram_.data(), system_via_),
		crtc_(crtc_bus_handler_),
		acia_(HalfCycles(2'000'000)), // TODO: look up real ACIA clock rate.
		adc_(HalfCycles(2'000'000))
	{
		set_clock_rate(2'000'000);

		system_via_port_handler_.set_interrupt_delegate(this);
		user_via_port_handler_.set_interrupt_delegate(this);

		// Grab ROMs.
		using Request = ::ROM::Request;
		using Name = ::ROM::Name;

		auto request = Request(Name::AcornBASICII) && Request(Name::BBCMicroMOS12);
		if(target.has_1770dfs) {
			request = request && Request(Name::AcornDFS226);
		}

		auto roms = rom_fetcher(request);
		if(!request.validate(roms)) {
			throw ROMMachine::Error::MissingROMs;
		}

		const auto os_data = roms.find(Name::BBCMicroMOS12)->second;
		std::copy(os_data.begin(), os_data.end(), os_.begin());

		install_sideways(15, roms.find(Name::AcornBASICII)->second, false);
		if(target.has_1770dfs) {
			install_sideways(14, roms.find(Name::AcornDFS226)->second, false);
		}

		// Setup fixed parts of memory map.
		page(0, &ram_[0], true);
		page(1, &ram_[16384], true);
		page_sideways(15);
		page(3, os_.data(), false);
		Memory::Fuzz(ram_);

		if constexpr (has_1770) {
			wd1770_.set_delegate(this);
		}

		insert_media(target.media);
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
		const auto duration = Cycles(is_1mhz(address) ? 2 + (phase_&1) : 1);
		phase_ += duration.as<int>();


		//
		// 1Mhz devices.
		//
		const auto half_cycles = HalfCycles(duration.as_integral());
		system_via_.run_for(half_cycles);
		system_via_port_handler_.advance_keyboard_scan(half_cycles);
		user_via_.run_for(half_cycles);


		//
		// 2Mhz devices.
		//
		audio_ += duration;
		if(crtc_2mhz_) {
			crtc_.run_for(duration);
		} else {
			// TODO: possibly skip one cycle if clock speed just changed partway through a 1Mhz window?
			const auto cycles = (phase_ >> 1) - ((phase_ - duration.as<int>()) >> 1);
			crtc_.run_for(Cycles(cycles));
		}
		adc_.run_for(duration);


		if constexpr (has_1770) {
			// The WD1770 is nominally clocked at 8Mhz.
			wd1770_.run_for(duration * 4);
		}

		//
		// Questionably-clocked devices.
		//
		acia_.run_for(half_cycles);


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
//					Logger::info().append("ACIA read");
					*value = acia_.read(address);
				} else {
//					Logger::info().append("ACIA write: %02x", *value);
					acia_.write(address, *value);
				}
			} else if(address >= 0xfec0 && address < 0xfee0) {
				if(is_read(operation)) {
					*value = adc_.read(address);
				} else {
					adc_.write(address, *value);
				}
			} else if(has_1770 && address >= 0xfe80 && address < 0xfe88) {
				switch(address) {
					case 0xfe80:
						if(!is_read(operation)) {
							wd1770_.set_control_register(*value);
						}
					break;
					default:
						if(is_read(operation)) {
							*value = wd1770_.read(address);
						} else {
							wd1770_.write(address, *value);
						}
					break;
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
			}
		}

		return duration;
	}

private:
	// MARK: - Activity::Source.
	void set_activity_observer(Activity::Observer *const observer) override {
		if(has_1770) {
			wd1770_.set_activity_observer(observer);
		}
		system_via_port_handler_.set_activity_observer(observer);
	}

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

	// MARK: - KeyboardMachine.
	BBCMicro::KeyboardMapper mapper_;
	KeyboardMapper *get_keyboard_mapper() override {
		return &mapper_;
	}

	void set_key_state(const uint16_t key, const bool is_pressed) override {
		if(key == BBCMicro::KeyboardMapper::KeyBreak) {
			m6502_.set_reset_line(is_pressed);
		} else {
			system_via_port_handler_.set_key(uint8_t(key), is_pressed);
		}
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

	// MARK: - uPD7002::Delegate.
	void did_change_interrupt_status(NEC::uPD7002 &) override {
		update_irq_line();
	}

	// MARK: - MediaTarget.
	bool insert_media(const Analyser::Static::Media &media) override {
		if(!media.disks.empty() && has_1770) {
			wd1770_.set_disk(media.disks.front(), 0);
		}

		return !media.disks.empty();
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
			system_via_.get_interrupt_line() /*||
			adc_.interrupt()*/
		);
	}

	Audio audio_;

	CRTCBusHandler crtc_bus_handler_;
	CRTC crtc_;
	bool crtc_2mhz_ = true;

	Motorola::ACIA::ACIA acia_;

	NEC::uPD7002 adc_;

	// MARK: - WD1770.
	Electron::Plus3 wd1770_;
	void wd1770_did_change_output(WD::WD1770 &) override {
		m6502_.set_nmi_line(wd1770_.get_interrupt_request_line() || wd1770_.get_data_request_line());
	}
};

}

using namespace BBCMicro;

std::unique_ptr<Machine> Machine::BBCMicro(
	const Analyser::Static::Target *target,
	const ROMMachine::ROMFetcher &rom_fetcher
) {
	using Target = Analyser::Static::Acorn::BBCMicroTarget;
	const Target *const acorn_target = dynamic_cast<const Target *>(target);
	if(acorn_target->has_1770dfs) {
		return std::make_unique<BBCMicro::ConcreteMachine<true>>(*acorn_target, rom_fetcher);
	} else {
		return std::make_unique<BBCMicro::ConcreteMachine<false>>(*acorn_target, rom_fetcher);
	}
}
