//
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
#include "Machines/Utility/Typer.hpp"

#include "Processors/6502Mk2/6502Mk2.hpp"

#include "Machines/Acorn/Tube/ULA.hpp"
#include "Machines/Acorn/Tube/Tube6502.hpp"
#include "Machines/Acorn/Tube/TubeZ80.hpp"

#include "Components/6522/6522.hpp"
#include "Components/6845/CRTC6845.hpp"
#include "Components/6850/6850.hpp"
#include "Components/SID/SID.hpp"
#include "Components/SAA5050/SAA5050.hpp"
#include "Components/SN76489/SN76489.hpp"
#include "Components/uPD7002/uPD7002.hpp"

// TODO: factor this more appropriately.
#include "Machines/Acorn/Electron/Plus3.hpp"

#include "Analyser/Static/Acorn/Target.hpp"
#include "Outputs/Log.hpp"

#include "Outputs/CRT/CRT.hpp"
#include "Outputs/Speaker/Implementation/CompoundSource.hpp"
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
using TubeProcessor = Analyser::Static::Acorn::BBCMicroTarget::TubeProcessor;

// MARK: - Joysticks.

/*!
	Provides an analogue joystick with a single fire button.
*/
class Joystick: public Inputs::ConcreteJoystick {
public:
	Joystick(NEC::uPD7002 &adc, const int first_channel) :
		ConcreteJoystick({
			Input(Input::Horizontal),
			Input(Input::Vertical),
			Input(Input::Fire)
		}),
		adc_(adc),
		first_channel_(first_channel) {}

	void did_set_input(const Input &input, const float value) final {
		switch(input.type) {
			case Input::Horizontal:
			case Input::Vertical:
				adc_.set_input(first_channel_ + (input.type == Input::Vertical), 1.0f - value);
			break;

			default: break;
		}
	}

	void did_set_input(const Input &input, const bool is_active) final {
		if(input.type == Input::Fire) {
			fire_ = is_active;
		}
	}

	bool fire() const {
		return fire_;
	}

private:
	float digital_minimum() const final {
		return 0.0f;
	}
	float digital_maximum() const final {
		return 1.0f;
	}

	NEC::uPD7002 &adc_;
	const int first_channel_;
	bool fire_ = false;
};

// MARK: - Lazy audio holder.

/*!
	Combines an SN76489 with an appropriate asynchronous queue and filtering speaker.
*/

// TODO: generalise the below and clean up across the project.
template <bool has_beebsid>
struct Audio {
private:
	using CompoundSource = Outputs::Speaker::CompoundSource<TI::SN76489, MOS::SID::SID>;
	using Source = std::conditional_t<has_beebsid, CompoundSource, TI::SN76489>;
	using Speaker = Outputs::Speaker::PullLowpass<Source>;

	Source &speaker_source() {
		if constexpr (has_beebsid) {
			return compound_;
		} else {
			return sn76489_;
		}
	}

public:
	Audio() :
		sn76489_(TI::SN76489::Personality::SN76489, audio_queue_, 4),
		sid_(audio_queue_),
		compound_(sn76489_, sid_),
		speaker_(speaker_source())
	{
		// Combined with the additional divider specified above, implies the SN76489 is clocked at 4Mhz.
		speaker_.set_input_rate(1'000'000.0f);
	}

	~Audio() {
		audio_queue_.flush();
	}

	template <typename TargetT>
	TargetT &get() {
		speaker_.run_for(audio_queue_, time_since_update_.flush<Cycles>());

		if constexpr (std::is_same_v<TargetT, TI::SN76489>) {
			return sn76489_;
		}
		if constexpr (std::is_same_v<TargetT, MOS::SID::SID>) {
			return sid_;
		}
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
	MOS::SID::SID sid_;
	CompoundSource compound_;
	Outputs::Speaker::PullLowpass<Source> speaker_;
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

// MARK: - VIAs.

/*!
	Models the system VIA, which connects to the SN76489 and the keyboard.
*/
struct VSyncReceiver {
	virtual void set_vsync(bool) = 0;
};
struct SystemVIADelegate {
	virtual void strobe_lightpen() = 0;
};

template <typename AudioT>
struct SystemVIAPortHandler: public MOS::MOS6522::IRQDelegatePortHandler, public VSyncReceiver {
	SystemVIAPortHandler(
		AudioT &audio,
		VideoBaseAddress &video_base,
		MOS::MOS6522::MOS6522<SystemVIAPortHandler<AudioT>> &via,
		SystemVIADelegate &delegate,
		const std::vector<std::unique_ptr<Inputs::Joystick>> &joysticks,
		const bool run_disk
	) :
		audio_(audio), video_base_(video_base), via_(via), joysticks_(joysticks), delegate_(delegate)
	{
		set_key_flag(uint8_t(Key::Bit3), run_disk);
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
			audio_.template get<TI::SN76489>().write(port_a_output_);
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
				if(activity_observer_) {
					activity_observer_->set_led_status(caps_led, caps_led_state_);
				}
			}

			if(new_shift != shift_led_state_) {
				shift_led_state_ = new_shift;
				if(activity_observer_) {
					activity_observer_->set_led_status(shift_led, shift_led_state_);
				}
			}
		}
	}

	template <MOS::MOS6522::Port port>
	uint8_t get_port_input() const {
		if(port == MOS::MOS6522::Port::B) {
			// TODO:
			//
			//	b4/5: joystick fire buttons (0 = pressed);
			//	b6/7: speech interrupt/ready inputs. (0 expected if no speech hardware)
			return
				0xf |
				(static_cast<Joystick *>(joysticks_[0].get())->fire() ? 0x00 : 0x10) |
				(static_cast<Joystick *>(joysticks_[1].get())->fire() ? 0x00 : 0x20);
		}

		if(latch_ & LatchFlags::KeyboardIsScanning) {
			return 0xff;
		}

		// Read keyboard. Low six bits of output are key to check, state should be returned in high bit.
		const uint8_t key_state = key_column(port_a_output_)[key_row(port_a_output_)] ? 0x80 : 0x00;
		return key_state;
	}

	template<MOS::MOS6522::Port port, MOS::MOS6522::Line line>
	void set_control_line_output(const bool value) {
		if constexpr (port == MOS::MOS6522::Port::B && line == MOS::MOS6522::Line::Two) {
			if(previous_cb2_ != value && !value) {
				delegate_.strobe_lightpen();
			}
			previous_cb2_ = value;
		}
	}

	void set_key(const uint8_t key, const bool pressed) {
		set_key_flag(key, pressed);
		update_ca2();
	}

	void clear_all_keys() {
		key_states_ = std::array<KeyRow, 16>{};
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

	bool caps_lock() const {
		return caps_led_state_;
	}

private:
	uint8_t latch_ = 0;
	enum LatchFlags: uint8_t {
		WriteToSN76489 = 1 << 0,
		KeyboardIsScanning = 1 << 3,
	};

	uint8_t port_a_output_ = 0;
	bool previous_cb2_ = false;

	AudioT &audio_;
	VideoBaseAddress &video_base_;

	MOS::MOS6522::MOS6522<SystemVIAPortHandler<AudioT>> &via_;

	// MARK: - Keyboard state and helpers.

	using KeyRow = std::bitset<8>;
	std::array<KeyRow, 16> key_states_{};
	int keyboard_scan_column_ = 0;

	void set_key_flag(const uint8_t key, const bool pressed) {
		key_column(key)[key_row(key)] = pressed;
	}
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

		via_.template set_control_line_input<MOS::MOS6522::Port::A, MOS::MOS6522::Line::Two>(state);
	}

	static inline const std::string caps_led = "CAPS";
	static inline const std::string shift_led = "SHIFT";
	bool caps_led_state_ = false;
	bool shift_led_state_ = false;
	Activity::Observer *activity_observer_ = nullptr;

	const std::vector<std::unique_ptr<Inputs::Joystick>> &joysticks_;
	SystemVIADelegate &delegate_;

	void set_vsync(const bool vsync) override {
		via_.template set_control_line_input<MOS::MOS6522::Port::A, MOS::MOS6522::Line::One>(vsync);
	}
};

// MARK: - CRTC output.

/*!
	Handles CRTC bus activity.
*/
class CRTCBusHandler: public VideoBaseAddress {
public:
	CRTCBusHandler(const uint8_t *const ram, VSyncReceiver &vsync_receiver) :
		crt_(1024, 1, Outputs::Display::Type::PAL50, Outputs::Display::InputDataType::Red1Green1Blue1),
		ram_(ram),
		vsync_receiver_(vsync_receiver)
	{}

	void set_dynamic_framing(const bool enable) {
		dynamic_framing_ = enable;
		if(enable) {
			crt_.set_dynamic_framing(
				Outputs::Display::Rect(0.13333f, 0.06507f, 0.71579f, 0.86069f),
				0.0f, 0.05f);
		} else {
			crt_.set_fixed_framing(crt_.get_rect_for_area(30, 256, 160, 800));
		}
	}

	bool dynamic_framing() const {
		return dynamic_framing_;
	}

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
		active_collation_.crtc_clock_multiplier = (value & 0x10) ? 1 : 2;
		active_collation_.pixels_per_clock = 1 << ((value >> 2) & 0x03);
		active_collation_.is_teletext = value & 0x02;
		flash_mask_ = value & 0x01 ? 7 : 0;
		cursor_mask_ = value & 0b1110'0000;
	}

	/*!
		The CRTC entry function for the main part of each clock cycle; takes the current
		bus state and determines what output to produce based on the current palette and mode.
	*/
	void perform_bus_cycle(const Motorola::CRTC::BusState &state) {
		static constexpr size_t PixelAllocationUnit = 480;	// Is assumed to be a multiple of both 12 and 16.
															// i.e. a multiple of 48.
		static_assert(!(PixelAllocationUnit % 16));
		static_assert(!(PixelAllocationUnit % 12));

		if(state.vsync != vsync_) {
			vsync_receiver_.set_vsync(state.vsync);
			vsync_ = state.vsync;
		}

		// Count cycles since horizontal sync to insert a colour burst.
		// TODO: this is copy/pasted from the CPC. How does the BBC do it?
//		if(state.hsync) {
//			++cycles_into_hsync_;
//		} else {
//			cycles_into_hsync_ = 0;
//		}
//		const bool is_colour_burst = cycles_into_hsync_ >= 5 && cycles_into_hsync_ < 9;

		// Check for a cursor leading edge.
		cursor_shifter_ >>= 4;
		if(state.cursor != previous_cursor_enabled_) {
			if(state.cursor) {
				cursor_shifter_ =
					((cursor_mask_ & 0x80) ? 0x0007 : 0) |
					((cursor_mask_ & 0x40) ? 0x0070 : 0) |
					((cursor_mask_ & 0x20) ? 0x7700 : 0);
			}
			previous_cursor_enabled_ = state.cursor;
		}

		// Consider some SAA5050 signalling.
		if(!state.vsync && previous_vsync_) {
			// Complete fiction here; the SAA5050 field flag is set by peeking inside CRTC state.
			// TODO: what really sets CRS for the SAA5050? Time since hsync maybe?
			saa5050_serialiser_.begin_frame(state.field_count.bit<0>());
		}
		previous_vsync_ = state.vsync;

		if(state.display_enable && !previous_display_enabled_ && active_collation_.is_teletext) {
			saa5050_serialiser_.begin_line();
		}
		previous_display_enabled_ = state.display_enable;

		// Grab 5050 output, if any.
		bool has_5050_output_ = saa5050_serialiser_.has_output();
		const auto saa_50505_output_ = saa5050_serialiser_.output();

		// Fetch, possibly.
		const bool should_fetch = state.display_enable && (active_collation_.is_teletext || !(state.line.get() & 8));
		if(should_fetch) {
			const uint16_t address = [&] {
				// Teletext address generation.
				if(state.refresh.get() & (1 << 13)) {
					return uint16_t(
						0x3c00 |
						((state.refresh.get() & 0x800) << 3) |
						(state.refresh.get() & 0x3ff)
					);
				}

				uint16_t address = uint16_t((state.refresh.get() << 3) | (state.line.get() & 7));
				if(address & 0x8000) {
					address = (address + video_base_) & 0x7fff;
				}
				return address;
			} ();
			const uint8_t fetched = ram_[address];
			pixel_shifter_ = fetched;
			saa5050_serialiser_.add(fetched);
		}

		// Pick new output mode.
		const OutputMode output_mode = [&] {
			if(state.hsync || state.vsync) {
				return OutputMode::Sync;
			}
//			if(is_colour_burst) {
//				return OutputMode::ColourBurst;
//			}
			if(
				(should_fetch && !active_collation_.is_teletext) ||
				(has_5050_output_ && active_collation_.is_teletext) ||
				cursor_shifter_
			) {
				return OutputMode::Pixels;
			}
			return OutputMode::Blank;
		} ();

		// If a transition between sync/border/pixels just occurred, flush whatever was
		// in progress to the CRT and reset counting. Also flush if this mode has just been effective
		// for a really long time, so as not to buffer too much.
		if(output_mode != previous_output_mode_ || cycles_ == 1024) {
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

		// Collect some more pixels if output is ongoing.
		if(output_mode == OutputMode::Pixels) {
			// Flush the current buffer pixel if full; the CRTC allows many different display
			// widths so it's not necessarily possible to predict the correct number in advance
			// and using the upper bound could lead to inefficient behaviour.
			if(pixel_data_ && (pixels_collected() == PixelAllocationUnit || active_collation_ != previous_collation_)) {
				flush_pixels();
				cycles_ = 0;
			}
			previous_collation_ = active_collation_;

			if(!pixel_data_) {
				pixel_pointer_ = pixel_data_ = crt_.begin_data(PixelAllocationUnit);
			}

			if(pixel_data_) {
				if(active_collation_.is_teletext) {
					if(has_5050_output_) {
						uint16_t pixels = saa_50505_output_.pixels();
						for(int c = 0; c < 12; c++) {
							*pixel_pointer_++ =
								((pixels & 0b1000'0000'0000) ? saa_50505_output_.alpha : saa_50505_output_.background)
									^ uint8_t(cursor_shifter_);
							pixels <<= 1;
						}
					} else {
						std::fill(pixel_pointer_, pixel_pointer_ + 12, 0);
						pixel_pointer_ += 12;
					}
				} else {
					switch(active_collation_.crtc_clock_multiplier * active_collation_.pixels_per_clock) {
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

		// Increment cycles since state changed.
		cycles_ += active_collation_.crtc_clock_multiplier << 3;
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
		int crtc_clock_multiplier = 1;
		int pixels_per_clock = 4;
		bool is_teletext = false;

		bool operator !=(const PixelCollation &rhs) {
			// If both are teletext, just inspect the clock multiplier.
			if(is_teletext && rhs.is_teletext) {
				return crtc_clock_multiplier != rhs.crtc_clock_multiplier;
			}

			// If one is teletext but the other isn't, that's a sufficient difference.
			if(is_teletext != rhs.is_teletext) return true;

			// Compare pixel clock rate.
			return pixels_per_clock != rhs.pixels_per_clock || crtc_clock_multiplier != rhs.crtc_clock_multiplier;
		}
	};

	OutputMode previous_output_mode_ = OutputMode::Sync;
	int cycles_ = 0;
//	int cycles_into_hsync_ = 0;

	Outputs::CRT::CRT crt_;
	bool dynamic_framing_ = true;

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

	PixelCollation active_collation_;
	uint8_t pixel_shifter_ = 0;

	uint8_t cursor_mask_ = 0;
	uint32_t cursor_shifter_ = 0;
	bool previous_cursor_enabled_ = false;

	bool previous_display_enabled_ = false;
	bool previous_vsync_ = false;

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
	VSyncReceiver &vsync_receiver_;
	bool vsync_ = false;

	Mullard::SAA5050Serialiser saa5050_serialiser_;
};
using CRTC = Motorola::CRTC::CRTC6845<
	CRTCBusHandler,
	Motorola::CRTC::Personality::HD6845S,
	Motorola::CRTC::CursorType::Native>;
}

// MARK: - Tube.

template <typename HostT, TubeProcessor tube_processor>
struct Tube {
	using TubeULA = Acorn::Tube::ULA<HostT>;
	TubeULA ula;
	Acorn::Tube::Processor<TubeULA, tube_processor> processor;

	Tube(HostT &owner) :
		ula(owner),
		processor(ula) {}
};

template <typename HostT>
struct Tube<HostT, TubeProcessor::None> {
	Tube(HostT &) {}
};

// MARK: - ConcreteMachine.

template <TubeProcessor tube_processor, bool has_1770, bool has_beebsid>
class ConcreteMachine:
	public Activity::Source,
	public Configurable::Device,
	public Machine,
	public MachineTypes::AudioProducer,
	public MachineTypes::JoystickMachine,
	public MachineTypes::MappedKeyboardMachine,
	public MachineTypes::MediaTarget,
	public MachineTypes::ScanProducer,
	public MachineTypes::TimedMachine,
	public MOS::MOS6522::IRQDelegatePortHandler::Delegate,
	public NEC::uPD7002::Delegate,
	public SystemVIADelegate,
	public Utility::TypeRecipient<CharacterMapper>,
	public WD::WD1770::Delegate
{
public:
	ConcreteMachine(
		const Analyser::Static::Acorn::BBCMicroTarget &target,
		const ROMMachine::ROMFetcher &rom_fetcher
	) :
		m6502_(*this),
		system_via_port_handler_(audio_, crtc_bus_handler_, system_via_, *this, joysticks_, target.should_shift_restart),
		user_via_(user_via_port_handler_),
		system_via_(system_via_port_handler_),
		crtc_bus_handler_(ram_.data(), system_via_port_handler_),
		crtc_(crtc_bus_handler_),
		acia_(HalfCycles(2'000'000)), // TODO: look up real ACIA clock rate.
		adc_(HalfCycles(2'000'000)),
		tube_(*this)
	{
		set_clock_rate(2'000'000);

		// Install two joysticks.
		joysticks_.emplace_back(new Joystick(adc_, 0));
		joysticks_.emplace_back(new Joystick(adc_, 2));

		system_via_port_handler_.set_interrupt_delegate(this);
		user_via_port_handler_.set_interrupt_delegate(this);
		adc_.set_delegate(this);

		// Grab ROMs.
		using Request = ::ROM::Request;
		using Name = ::ROM::Name;

		auto request = Request(Name::AcornBASICII) && Request(Name::BBCMicroMOS12);
		if(target.has_1770dfs || tube_processor != TubeProcessor::None) {
			request = request && Request(Name::BBCMicro1770DFS226);
		}
		if(target.has_adfs) {
			request = request && Request(Name::BBCMicroADFS130);
		}

		if constexpr (tube_processor != TubeProcessor::None) {
			request = request && Request(tube_.processor.ROM);
		}

		auto roms = rom_fetcher(request);
		if(!request.validate(roms)) {
			throw ROMMachine::Error::MissingROMs;
		}

		const auto os_data = roms.find(Name::BBCMicroMOS12)->second;
		std::copy(os_data.begin(), os_data.end(), os_.begin());

		// Put BASIC in pole position.
		install_sideways(15, roms.find(Name::AcornBASICII)->second, false);

		// Install filing systems: put the DFS before the ADFS because it's more common on the BBC if the user
		// explicitly requested DFS. Include it at the end otherwise if it's just implied by the Tube.
		size_t fs_slot = 14;
		const auto add_sideways = [&](const Name name) {
			install_sideways(fs_slot--, roms.find(name)->second, false);
		};
		if(target.has_1770dfs) {
			add_sideways(Name::BBCMicro1770DFS226);
		}
		if(target.has_adfs) {
			add_sideways(Name::BBCMicroADFS130);
		}
		if(!target.has_1770dfs && tube_processor != TubeProcessor::None) {
			add_sideways(Name::BBCMicro1770DFS226);
		}

		// Throw the tube ROM to its target.
		if constexpr (tube_processor != TubeProcessor::None) {
			tube_.processor.set_rom(roms.find(tube_.processor.ROM)->second);
		}

		// Install the ADT ROM if available, but don't error if it's missing. It's very optional.
		if(target.has_1770dfs || target.has_adfs) {
			const auto adt_rom = rom_fetcher(Request(Name::BBCMicroAdvancedDiscToolkit140));
			if(const auto rom = adt_rom.find(Name::BBCMicroAdvancedDiscToolkit140); rom != adt_rom.end()) {
				install_sideways(fs_slot--, rom->second, false);
			}
		}

		// Throw sideways RAM into all unused slots.
		if(target.has_sideways_ram) {
			for(size_t c = 0; c < 16; c++) {
				if(!rom_inserted_[c]) {
					rom_inserted_[c] = rom_write_masks_[c] = true;
				}
			}
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
		if(!target.loading_command.empty()) {
			type_string(target.loading_command);
		}
	}

	// MARK: - 6502 bus.
	template <CPU::MOS6502Mk2::BusOperation operation, typename AddressT>
	Cycles perform(const AddressT address, CPU::MOS6502Mk2::data_t<operation> value) {
		// Returns @c true if @c address is a device on the 1Mhz bus; @c false otherwise.
		const auto is_1mhz = [&] {
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
		}();

		// Determine whether this access hits the 1Mhz bus; if so then apply appropriate penalty, and update phase.
		const auto duration = Cycles(is_1mhz ? 2 + (phase_&1) : 1);
		if(typer_) typer_->run_for(duration);
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
		if constexpr (requires {tube_.processor;}) {
			tube_.processor.run_for(duration);
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
				if constexpr (is_read(operation)) {
					value = system_via_.read(address);
				} else {
					system_via_.write(address, value);
				}
			} else if(address >= 0xfe60 && address < 0xfe80) {
				if constexpr (is_read(operation)) {
					value = user_via_.read(address);
				} else {
					user_via_.write(address, value);
				}
			} else if(address == 0xfe30) {
				if constexpr (is_read(operation)) {
					value = 0xfe;
				} else {
					page_sideways(value & 0xf);
				}
			} else if(address >= 0xfe00 && address < 0xfe08) {
				if constexpr (is_read(operation)) {
					if(address & 1) {
						value = crtc_.get_register();
					} else {
						value = crtc_.get_status();
					}
				} else {
					if(address & 1) {
						crtc_.set_register(value);
					} else {
						crtc_.select_register(value);
					}
				}
			} else if(address >= 0xfe20 && address < 0xfe30) {
				if constexpr (is_read(operation)) {
					value = 0xfe;
				} else {
					switch(address) {
						case 0xfe20:
							crtc_bus_handler_.set_control(value);
							crtc_2mhz_ = value & 0x10;
						break;
						case 0xfe21:
							crtc_bus_handler_.set_palette(value);
						break;
					}
				}
			} else if(address >= 0xfee0 && address < 0xfee8) {
				if constexpr (requires {tube_.ula;}) {
					if constexpr (is_read(operation)) {
						value = tube_.ula.host_read(address);
					} else {
						tube_.ula.host_write(address, value);
					}
				} else {
					if constexpr (is_read(operation)) {
						value = address == 0xfee0 ? 0xfe : 0xff;
					}
				}
			} else if(address >= 0xfe08 && address < 0xfe10) {
				if constexpr (is_read(operation)) {
//					Logger::info().append("ACIA read");
					value = acia_.read(address);
				} else {
//					Logger::info().append("ACIA write: %02x", *value);
					acia_.write(address, value);
				}
			} else if(address >= 0xfec0 && address < 0xfee0) {
				if constexpr (is_read(operation)) {
					value = adc_.read(address);
				} else {
					adc_.write(address, value);
				}
			} else if(has_1770 && address >= 0xfe80 && address < 0xfe88) {
				switch(address) {
					case 0xfe80:
						if constexpr (!is_read(operation)) {
							wd1770_.set_control_register(value);
						} else {
							value = 0xff;
						}
					break;
					default:
						if constexpr (is_read(operation)) {
							value = wd1770_.read(address);
						} else {
							wd1770_.write(address, value);
						}
					break;
				}
			} else if(has_beebsid && address >= 0xfc20 && address < 0xfc40) {
				if constexpr (is_read(operation)) {
					value = audio_.template get<MOS::SID::SID>().read(+address);
				} else {
					audio_.template get<MOS::SID::SID>().write(+address, value);
				}
			} else {
				Logger::error()
					.append("Unhandled IO %s at %04x", is_read(operation) ? "read" : "write", address)
					.append_if(!is_read(operation), ": %02x", value);

				if constexpr (is_read(operation)) {
					value = 0xff;
				}
			}
			return duration;
		}


		//
		// ROM or RAM access.
		//
		if constexpr (is_read(operation)) {
			// TODO: probably don't do this with this condition? See how it compiles. If it's a CMOV somehow, no problem.
			if((address >> 14) == 2 && !sideways_read_mask_) {
				value = 0xff;
			} else {
				value = memory_[address >> 14][address];
			}
		} else {
			if(memory_write_masks_[address >> 14]) {
				memory_[address >> 14][address] = value;
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

	// MARK: - SystemVIADelegate.
	void strobe_lightpen() override {
		crtc_.trigger_light_pen();
	}

	// MARK: - KeyboardMachine.
	BBCMicro::KeyboardMapper mapper_;
	KeyboardMapper *get_keyboard_mapper() override {
		return &mapper_;
	}

	void set_key_state(const uint16_t key, const bool is_pressed) override {
		switch(Key(key)) {
			case Key::SwitchOffCaps:
				// Store current caps lock state for a potential restore; press caps lock
				// now if there's a need to exit caps lock mode.
				was_caps_ = system_via_port_handler_.caps_lock();
				if(was_caps_) {
					system_via_port_handler_.set_key(uint8_t(Key::CapsLock), true);
				}
			break;
			case Key::RestoreCaps:
				// Press caps lock again if the machine was originally in the caps lock state.
				// If so then SwitchOffCaps switched it off.
				if(was_caps_) {
					system_via_port_handler_.set_key(uint8_t(Key::CapsLock), true);
				}
			break;

			case Key::Break:
				set_reset(is_pressed);
			break;

			default:
				system_via_port_handler_.set_key(uint8_t(key), is_pressed);
			break;
		}
	}
	bool was_caps_ = false;

	void clear_all_keys() final {
		set_reset(false);
		system_via_port_handler_.clear_all_keys();
	}

	void set_reset(const bool reset) {
		m6502_.template set<CPU::MOS6502Mk2::Line::Reset>(reset);
		if constexpr (requires {tube_.ula;}) {
			tube_.ula.set_reset(reset);
		}
	}

	HalfCycles get_typer_delay(const std::string &text) const final {
		if(!m6502_.is_resetting()) {
			return Cycles(0);
		}

		// Add a longer delay for a command at reset that involves pressing a modifier;
		// empirically this seems to be a requirement, in order to avoid a collision with
		// the system's built-in modifier-at-startup test (e.g. to perform shift+break).
		CharacterMapper test_mapper;
		const uint16_t *const sequence = test_mapper.sequence_for_character(text[0]);
		return is_modifier(Key(sequence[0])) ? Cycles(1'000'000) : Cycles(750'000);
	}

	HalfCycles get_typer_frequency() const final {
		return Cycles(60'000);
	}

	void type_string(const std::string &string) final {
		Utility::TypeRecipient<CharacterMapper>::add_typer(string);
	}

	bool can_type(const char c) const final {
		return Utility::TypeRecipient<CharacterMapper>::can_type(c);
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
		system_via_.template set_control_line_input<MOS::MOS6522::Port::B, MOS::MOS6522::Line::One>(adc_.interrupt());
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

		assert(roms_[slot].size() % source.size() == 0);
		auto begin = roms_[slot].begin();
		while(begin != roms_[slot].end()) {
			std::copy(source.begin(), source.end(), begin);
			std::advance(begin, source.size());
		}
	}

	// MARK: - Components.

	struct M6502Traits {
		static constexpr auto uses_ready_line = false;
		static constexpr auto pause_precision = CPU::MOS6502Mk2::PausePrecision::BetweenInstructions;
		using BusHandlerT = ConcreteMachine;
	};
	CPU::MOS6502Mk2::Processor<CPU::MOS6502Mk2::Model::M6502, M6502Traits> m6502_;

	UserVIAPortHandler user_via_port_handler_;
	SystemVIAPortHandler<Audio<has_beebsid>> system_via_port_handler_;
	UserVIA user_via_;
	MOS::MOS6522::MOS6522<SystemVIAPortHandler<Audio<has_beebsid>>> system_via_;

	void update_irq_line() {
		const bool tube_irq =
			[&] {
				if constexpr (requires {tube_.ula;}) {
					return tube_.ula.has_host_irq();
				} else {
					return false;
				}
			} ();

		m6502_.template set<CPU::MOS6502Mk2::Line::IRQ>(
			user_via_.get_interrupt_line() ||
			system_via_.get_interrupt_line() ||
			tube_irq
		);
	}

	Audio<has_beebsid> audio_;

	CRTCBusHandler crtc_bus_handler_;
	CRTC crtc_;
	bool crtc_2mhz_ = true;

	Motorola::ACIA::ACIA acia_;

	NEC::uPD7002 adc_;

	// MARK: - WD1770.
	Electron::Plus3 wd1770_;
	void wd1770_did_change_output(WD::WD1770 &) override {
		m6502_.template set<CPU::MOS6502Mk2::Line::NMI>(
			wd1770_.get_interrupt_request_line() || wd1770_.get_data_request_line()
		);
	}

	// MARK: - Joysticks
	std::vector<std::unique_ptr<Inputs::Joystick>> joysticks_;
	const std::vector<std::unique_ptr<Inputs::Joystick>> &get_joysticks() override {
		return joysticks_;
	}

	// MARK: - Configuration options.
	std::unique_ptr<Reflection::Struct> get_options() const final {
		auto options = std::make_unique<Options>(Configurable::OptionsType::UserFriendly);
		options->dynamic_crop = crtc_bus_handler_.dynamic_framing();
		return options;
	}

	void set_options(const std::unique_ptr<Reflection::Struct> &str) final {
		const auto options = dynamic_cast<Options *>(str.get());
		crtc_bus_handler_.set_dynamic_framing(options->dynamic_crop);
	}

	// MARK: - Tube.

	Tube<ConcreteMachine, tube_processor> tube_;

public:
	void set_host_tube_irq(bool)					{	update_irq_line();					}
	void set_parasite_tube_irq(const bool active)	{	tube_.processor.set_irq(active);	}
	void set_parasite_tube_nmi(const bool active)	{	tube_.processor.set_nmi(active);	}
	void set_parasite_reset(const bool active)		{	tube_.processor.set_reset(active);	}
};

}

using namespace BBCMicro;

namespace {
using Target = Analyser::Static::Acorn::BBCMicroTarget;

template <Target::TubeProcessor processor, bool has_1770>
std::unique_ptr<Machine> machine(const Target &target, const ROMMachine::ROMFetcher &rom_fetcher) {
	if(target.has_beebsid) {
		return std::make_unique<BBCMicro::ConcreteMachine<processor, has_1770, true>>(target, rom_fetcher);
	} else {
		return std::make_unique<BBCMicro::ConcreteMachine<processor, has_1770, false>>(target, rom_fetcher);
	}
}

template <Target::TubeProcessor processor>
std::unique_ptr<Machine> machine(const Target &target, const ROMMachine::ROMFetcher &rom_fetcher) {
	if(target.has_1770dfs || target.has_adfs) {
		return machine<processor, true>(target, rom_fetcher);
	} else {
		return machine<processor, false>(target, rom_fetcher);
	}
}
}

std::unique_ptr<Machine> Machine::BBCMicro(
	const Analyser::Static::Target *target,
	const ROMMachine::ROMFetcher &rom_fetcher
) {
	const Target *const acorn_target = dynamic_cast<const Target *>(target);
	switch(acorn_target->tube_processor) {
		case TubeProcessor::None:		return machine<TubeProcessor::None>(*acorn_target, rom_fetcher);
		case TubeProcessor::WDC65C02:	return machine<TubeProcessor::WDC65C02>(*acorn_target, rom_fetcher);
		case TubeProcessor::Z80:		return machine<TubeProcessor::Z80>(*acorn_target, rom_fetcher);
		default:	return nullptr;
	}
}
