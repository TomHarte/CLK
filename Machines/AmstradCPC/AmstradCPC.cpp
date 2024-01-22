//
//  AmstradCPC.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 30/07/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "AmstradCPC.hpp"

#include "Keyboard.hpp"
#include "FDC.hpp"

#include "../../Processors/Z80/Z80.hpp"

#include "../../Components/6845/CRTC6845.hpp"
#include "../../Components/8255/i8255.hpp"
#include "../../Components/AY38910/AY38910.hpp"

#include "../Utility/MemoryFuzzer.hpp"
#include "../Utility/Typer.hpp"

#include "../../Activity/Source.hpp"
#include "../MachineTypes.hpp"

#include "../../Storage/Tape/Tape.hpp"
#include "../../Storage/Tape/Parsers/Spectrum.hpp"

#include "../../ClockReceiver/ForceInline.hpp"
#include "../../Outputs/Speaker/Implementation/LowpassSpeaker.hpp"
#include "../../Outputs/CRT/CRT.hpp"

#include "../../Analyser/Static/AmstradCPC/Target.hpp"

#include "../../Numeric/CRC.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace AmstradCPC {

/*!
	Models the CPC's interrupt timer. Inputs are vsync, hsync, interrupt acknowledge and reset, and its output
	is simply yes or no on whether an interupt is currently requested. Internally it uses a counter with a period
	of 52 and occasionally adjusts or makes decisions based on bit 4.

	Hsync and vsync signals are expected to come directly from the CRTC; they are not decoded from a composite stream.
*/
class InterruptTimer {
	public:
		/*!
			Indicates that a new hsync pulse has been recognised. This should be
			supplied on the falling edge of the CRTC HSYNC signal, which is the
			trailing edge because it is active high.
		*/
		inline void signal_hsync() {
			// Increment the timer and if it has hit 52 then reset it and
			// set the interrupt request line to true.
			++timer_;
			if(timer_ == 52) {
				timer_ = 0;
				interrupt_request_ = true;
			}

			// If a vertical sync has previously been indicated then after two
			// further horizontal syncs the timer should either (i) set the interrupt
			// line, if bit 4 is clear; or (ii) reset the timer.
			if(reset_counter_) {
				--reset_counter_;
				if(!reset_counter_) {
					if(timer_ & 32) {
						interrupt_request_ = true;
					}
					timer_ = 0;
				}
			}
		}

		/// Indicates the leading edge of a new vertical sync.
		inline void signal_vsync() {
			reset_counter_ = 2;
		}

		/// Indicates that an interrupt acknowledge has been received from the Z80.
		inline void signal_interrupt_acknowledge() {
			interrupt_request_ = false;
			timer_ &= ~32;
		}

		/// @returns @c true if an interrupt is currently requested; @c false otherwise.
		inline bool get_request() {
			return last_interrupt_request_ = interrupt_request_;
		}

		/// Asks whether the interrupt status has changed.
		inline bool request_has_changed() {
			return last_interrupt_request_ != interrupt_request_;
		}

		/// Resets the timer.
		inline void reset_count() {
			timer_ = 0;
			interrupt_request_ = false;
		}

	private:
		int reset_counter_ = 0;
		bool interrupt_request_ = false;
		bool last_interrupt_request_ = false;
		int timer_ = 0;
};

/*!
	Provides a holder for an AY-3-8910 and its current cycles-since-updated count.
	Therefore acts both to store an AY and to bookkeep this emulator's idiomatic
	deferred clocking for this component.
*/
class AYDeferrer {
	public:
		/// Constructs a new AY instance and sets its clock rate.
		AYDeferrer() : ay_(GI::AY38910::Personality::AY38910, audio_queue_), speaker_(ay_) {
			speaker_.set_input_rate(1000000);
			// Per the CPC Wiki:
			// "A is output to the right, channel C is output left, and channel B is output to both left and right".
			ay_.set_output_mixing(0.0, 0.5, 1.0, 1.0, 0.5, 0.0);
		}

		~AYDeferrer() {
			audio_queue_.flush();
		}

		/// Adds @c half_cycles half cycles to the amount of time that has passed.
		inline void run_for(HalfCycles half_cycles) {
			cycles_since_update_ += half_cycles;
		}

		/// Enqueues an update-to-now into the AY's deferred queue.
		inline void update() {
			speaker_.run_for(audio_queue_, cycles_since_update_.divide_cycles(Cycles(4)));
		}

		/// Issues a request to the AY to perform all processing up to the current time.
		inline void flush() {
			audio_queue_.perform();
		}

		/// @returns the speaker the AY is using for output.
		Outputs::Speaker::Speaker *get_speaker() {
			return &speaker_;
		}

		/// @returns the AY itself.
		GI::AY38910::AY38910<true> &ay() {
			return ay_;
		}

	private:
		Concurrency::AsyncTaskQueue<false> audio_queue_;
		GI::AY38910::AY38910<true> ay_;
		Outputs::Speaker::PullLowpass<GI::AY38910::AY38910<true>> speaker_;
		HalfCycles cycles_since_update_;
};

/*!
	Provides the mechanism of receipt for the CRTC outputs. In practice has the gate array's
	video fetching and serialisation logic built in. So this is responsible for all video
	generation and therefore owns details such as the current palette.
*/
class CRTCBusHandler {
	public:
		CRTCBusHandler(const uint8_t *ram, InterruptTimer &interrupt_timer) :
			crt_(1024, 1, Outputs::Display::Type::PAL50, Outputs::Display::InputDataType::Red2Green2Blue2),
			ram_(ram),
			interrupt_timer_(interrupt_timer) {
				establish_palette_hits();
				build_mode_table();
				crt_.set_visible_area(Outputs::Display::Rect(0.1072f, 0.1f, 0.842105263157895f, 0.842105263157895f));
				crt_.set_brightness(3.0f / 2.0f);	// As only the values 0, 1 and 2 will be used in each channel,
													// whereas Red2Green2Blue2 defines a range of 0-3.
			}

		/*!
			The CRTC entry function for the main part of each clock cycle; takes the current
			bus state and determines what output to produce based on the current palette and mode.
		*/
		forceinline void perform_bus_cycle_phase1(const Motorola::CRTC::BusState &state) {
			// The gate array waits 2us to react to the CRTC's vsync signal, and then
			// caps output at 4us. Since the clock rate is 1Mhz, that's 2 and 4 cycles,
			// respectively.
			if(state.hsync) {
				cycles_into_hsync_++;
			} else {
				cycles_into_hsync_ = 0;
			}

			const bool is_hsync = (cycles_into_hsync_ >= 2 && cycles_into_hsync_ < 6);
			const bool is_colour_burst = (cycles_into_hsync_ >= 7 && cycles_into_hsync_ < 11);

			// Sync is taken to override pixels, and is combined as a simple OR.
			const bool is_sync = is_hsync || state.vsync;
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
						case OutputMode::Blank:			crt_.output_blank(cycles_ * 16);				break;
						case OutputMode::Sync:			crt_.output_sync(cycles_ * 16);					break;
						case OutputMode::Border:		output_border(cycles_);							break;
						case OutputMode::ColourBurst:	crt_.output_default_colour_burst(cycles_ * 16);	break;
						case OutputMode::Pixels:
							crt_.output_data(cycles_ * 16, size_t(cycles_ * 16 / pixel_divider_));
							pixel_pointer_ = pixel_data_ = nullptr;
						break;
					}
				}

				cycles_ = 0;
				previous_output_mode_ = output_mode;
			}

			// increment cycles since state changed
			cycles_++;

			// collect some more pixels if output is ongoing
			if(previous_output_mode_ == OutputMode::Pixels) {
				if(!pixel_data_) {
					pixel_pointer_ = pixel_data_ = crt_.begin_data(320, 8);
				}
				if(pixel_pointer_) {
					// the CPC shuffles output lines as:
					//	MA13 MA12	RA2 RA1 RA0		MA9 MA8 MA7 MA6 MA5 MA4 MA3 MA2 MA1 MA0		CCLK
					// ... so form the real access address.
					const uint16_t address =
						uint16_t(
							((state.refresh_address & 0x3ff) << 1) |
							((state.row_address & 0x7) << 11) |
							((state.refresh_address & 0x3000) << 2)
						);

					// Fetch two bytes and translate into pixels. Guaranteed: the mode can change only at
					// hsync, so there's no risk of pixel_pointer_ overrunning 320 output pixels without
					// exactly reaching 320 output pixels.
					switch(mode_) {
						case 0:
							reinterpret_cast<uint16_t *>(pixel_pointer_)[0] = mode0_output_[ram_[address]];
							reinterpret_cast<uint16_t *>(pixel_pointer_)[1] = mode0_output_[ram_[address+1]];
							pixel_pointer_ += 2 * sizeof(uint16_t);
						break;

						case 1:
							reinterpret_cast<uint32_t *>(pixel_pointer_)[0] = mode1_output_[ram_[address]];
							reinterpret_cast<uint32_t *>(pixel_pointer_)[1] = mode1_output_[ram_[address+1]];
							pixel_pointer_ += 2 * sizeof(uint32_t);
						break;

						case 2:
							reinterpret_cast<uint64_t *>(pixel_pointer_)[0] = mode2_output_[ram_[address]];
							reinterpret_cast<uint64_t *>(pixel_pointer_)[1] = mode2_output_[ram_[address+1]];
							pixel_pointer_ += 2 * sizeof(uint64_t);
						break;

						case 3:
							reinterpret_cast<uint16_t *>(pixel_pointer_)[0] = mode3_output_[ram_[address]];
							reinterpret_cast<uint16_t *>(pixel_pointer_)[1] = mode3_output_[ram_[address+1]];
							pixel_pointer_ += 2 * sizeof(uint16_t);
						break;

					}

					// Flush the current buffer pixel if full; the CRTC allows many different display
					// widths so it's not necessarily possible to predict the correct number in advance
					// and using the upper bound could lead to inefficient behaviour.
					if(pixel_pointer_ == pixel_data_ + 320) {
						crt_.output_data(cycles_ * 16, size_t(cycles_ * 16 / pixel_divider_));
						pixel_pointer_ = pixel_data_ = nullptr;
						cycles_ = 0;
					}
				}
			}
		}

		/*!
			The CRTC entry function for phase 2 of each bus cycle, in which the next sync line state becomes
			visible early. The CPC uses changes in sync to clock the interrupt timer.
		*/
		void perform_bus_cycle_phase2(const Motorola::CRTC::BusState &state) {
			// Notify a leading hsync edge to the interrupt timer.
			// Per Interrupts in the CPC: "to be confirmed: does gate array count positive or negative edge transitions of HSYNC signal?";
			// if you take it as given that display mode is latched as a result of hsync then Pipe Mania seems to imply that the count
			// occurs on a leading edge and the mode lock on a trailing.
			if(was_hsync_ && !state.hsync) {
				interrupt_timer_.signal_hsync();
			}

			// Check for a trailing CRTC hsync; if one occurred then that's the trigger potentially to change modes.
			if(!was_hsync_ && state.hsync) {
				if(mode_ != next_mode_) {
					mode_ = next_mode_;
					switch(mode_) {
						default:
						case 0:		pixel_divider_ = 4;	break;
						case 1:		pixel_divider_ = 2;	break;
						case 2:		pixel_divider_ = 1;	break;
					}
					build_mode_table();
				}
			}

			// check for a leading vsync; that also needs to be communicated to the interrupt timer
			if(!was_vsync_ && state.vsync) {
				interrupt_timer_.signal_vsync();
			}

			// update current state for edge detection next time around
			was_vsync_ = state.vsync;
			was_hsync_ = state.hsync;
		}

		/// Sets the destination for output.
		void set_scan_target(Outputs::Display::ScanTarget *scan_target) {
			crt_.set_scan_target(scan_target);
		}

		/// @returns The current scan status.
		Outputs::Display::ScanStatus get_scaled_scan_status() const {
			return crt_.get_scaled_scan_status() / 4.0f;
		}

		/// Sets the type of display.
		void set_display_type(Outputs::Display::DisplayType display_type) {
			crt_.set_display_type(display_type);
		}

		/// Gets the type of display.
		Outputs::Display::DisplayType get_display_type() const {
			return crt_.get_display_type();
		}

		/*!
			Sets the next video mode. Per the documentation, mode changes take effect only at the end of line,
			not immediately. So next means "as of the end of this line".
		*/
		void set_next_mode(int mode) {
			next_mode_ = mode;
		}

		/// Palette management: selects a pen to modify.
		void select_pen(int pen) {
			pen_ = pen;
		}

		/// Palette management: sets the colour of the selected pen.
		void set_colour(uint8_t colour) {
			if(pen_ & 16) {
				// If border is[/was] currently being output, flush what should have been
				// drawn in the old colour.
				if(previous_output_mode_ == OutputMode::Border) {
					output_border(cycles_);
					cycles_ = 0;
				}
				border_ = mapped_palette_value(colour);
			} else {
				palette_[pen_] = mapped_palette_value(colour);
				patch_mode_table(size_t(pen_));
			}
		}

	private:
		void output_border(int length) {
			assert(length >= 0);

			// A black border can be output via crt_.output_blank for a minor performance
			// win; otherwise paint whatever the border colour really is.
			if(border_) {
				crt_.output_level<uint8_t>(length * 16, border_);
			} else {
				crt_.output_blank(length * 16);
			}
		}

#define Mode0Colour0(c) (((c & 0x80) >> 7) | ((c & 0x20) >> 3) | ((c & 0x08) >> 2) | ((c & 0x02) << 2))
#define Mode0Colour1(c) (((c & 0x40) >> 6) | ((c & 0x10) >> 2) | ((c & 0x04) >> 1) | ((c & 0x01) << 3))

#define Mode1Colour0(c) (((c & 0x80) >> 7) | ((c & 0x08) >> 2))
#define Mode1Colour1(c) (((c & 0x40) >> 6) | ((c & 0x04) >> 1))
#define Mode1Colour2(c) (((c & 0x20) >> 5) | ((c & 0x02) >> 0))
#define Mode1Colour3(c) (((c & 0x10) >> 4) | ((c & 0x01) << 1))

#define Mode3Colour0(c)	(((c & 0x80) >> 7) | ((c & 0x08) >> 2))
#define Mode3Colour1(c) (((c & 0x40) >> 6) | ((c & 0x04) >> 1))

		/*!
			Creates a lookup table from palette entry to list of affected entries in the value -> pixels lookup tables.
		*/
		void establish_palette_hits() {
			for(size_t c = 0; c < 256; c++) {
				assert(Mode0Colour0(c) < mode0_palette_hits_.size());
				assert(Mode0Colour1(c) < mode0_palette_hits_.size());
				mode0_palette_hits_[Mode0Colour0(c)].push_back(uint8_t(c));
				mode0_palette_hits_[Mode0Colour1(c)].push_back(uint8_t(c));

				assert(Mode1Colour0(c) < mode1_palette_hits_.size());
				assert(Mode1Colour1(c) < mode1_palette_hits_.size());
				assert(Mode1Colour2(c) < mode1_palette_hits_.size());
				assert(Mode1Colour3(c) < mode1_palette_hits_.size());
				mode1_palette_hits_[Mode1Colour0(c)].push_back(uint8_t(c));
				mode1_palette_hits_[Mode1Colour1(c)].push_back(uint8_t(c));
				mode1_palette_hits_[Mode1Colour2(c)].push_back(uint8_t(c));
				mode1_palette_hits_[Mode1Colour3(c)].push_back(uint8_t(c));

				assert(Mode3Colour0(c) < mode3_palette_hits_.size());
				assert(Mode3Colour1(c) < mode3_palette_hits_.size());
				mode3_palette_hits_[Mode3Colour0(c)].push_back(uint8_t(c));
				mode3_palette_hits_[Mode3Colour1(c)].push_back(uint8_t(c));
			}
		}

		void build_mode_table() {
			switch(mode_) {
				case 0:
					// Mode 0: abcdefgh -> [gcea] [hdfb]
					for(size_t c = 0; c < 256; c++) {
						// Prepare mode 0.
						uint8_t *const mode0_pixels = reinterpret_cast<uint8_t *>(&mode0_output_[c]);
						mode0_pixels[0] = palette_[Mode0Colour0(c)];
						mode0_pixels[1] = palette_[Mode0Colour1(c)];
					}
				break;

				case 1:
					for(size_t c = 0; c < 256; c++) {
						// Prepare mode 1.
						uint8_t *const mode1_pixels = reinterpret_cast<uint8_t *>(&mode1_output_[c]);
						mode1_pixels[0] = palette_[Mode1Colour0(c)];
						mode1_pixels[1] = palette_[Mode1Colour1(c)];
						mode1_pixels[2] = palette_[Mode1Colour2(c)];
						mode1_pixels[3] = palette_[Mode1Colour3(c)];
					}
				break;

				case 2:
					for(size_t c = 0; c < 256; c++) {
						// Prepare mode 2.
						uint8_t *const mode2_pixels = reinterpret_cast<uint8_t *>(&mode2_output_[c]);
						mode2_pixels[0] = palette_[((c & 0x80) >> 7)];
						mode2_pixels[1] = palette_[((c & 0x40) >> 6)];
						mode2_pixels[2] = palette_[((c & 0x20) >> 5)];
						mode2_pixels[3] = palette_[((c & 0x10) >> 4)];
						mode2_pixels[4] = palette_[((c & 0x08) >> 3)];
						mode2_pixels[5] = palette_[((c & 0x04) >> 2)];
						mode2_pixels[6] = palette_[((c & 0x03) >> 1)];
						mode2_pixels[7] = palette_[((c & 0x01) >> 0)];
					}
				break;

				case 3:
					for(size_t c = 0; c < 256; c++) {
						// Prepare mode 3.
						uint8_t *const mode3_pixels = reinterpret_cast<uint8_t *>(&mode3_output_[c]);
						mode3_pixels[0] = palette_[Mode3Colour0(c)];
						mode3_pixels[1] = palette_[Mode3Colour1(c)];
					}
				break;
			}
		}

		void patch_mode_table(size_t pen) {
			switch(mode_) {
				case 0: {
					for(uint8_t c : mode0_palette_hits_[pen]) {
						assert(c < mode0_output_.size());
						uint8_t *const mode0_pixels = reinterpret_cast<uint8_t *>(&mode0_output_[c]);
						mode0_pixels[0] = palette_[Mode0Colour0(c)];
						mode0_pixels[1] = palette_[Mode0Colour1(c)];
					}
				} break;
				case 1:
					if(pen >= mode1_palette_hits_.size()) return;
					for(uint8_t c : mode1_palette_hits_[pen]) {
						assert(c < mode1_output_.size());
						uint8_t *const mode1_pixels = reinterpret_cast<uint8_t *>(&mode1_output_[c]);
						mode1_pixels[0] = palette_[Mode1Colour0(c)];
						mode1_pixels[1] = palette_[Mode1Colour1(c)];
						mode1_pixels[2] = palette_[Mode1Colour2(c)];
						mode1_pixels[3] = palette_[Mode1Colour3(c)];
					}
				break;
				case 2:
					if(pen > 1) return;
					// Whichever pen this is, there's only one table entry it doesn't touch, so just
					// rebuild the whole thing.
					build_mode_table();
				break;
				case 3:
					if(pen >= mode3_palette_hits_.size()) return;
					// Same argument applies here as to case 1, as the unused bits aren't masked out.
					for(uint8_t c : mode3_palette_hits_[pen]) {
						assert(c < mode3_output_.size());
						uint8_t *const mode3_pixels = reinterpret_cast<uint8_t *>(&mode3_output_[c]);
						mode3_pixels[0] = palette_[Mode3Colour0(c)];
						mode3_pixels[1] = palette_[Mode3Colour1(c)];
					}
				break;
			}
		}

#undef Mode0Colour0
#undef Mode0Colour1

#undef Mode1Colour0
#undef Mode1Colour1
#undef Mode1Colour2
#undef Mode1Colour3

#undef Mode3Colour0
#undef Mode3Colour1

		uint8_t mapped_palette_value(uint8_t colour) {
#define COL(r, g, b) (r << 4) | (g << 2) | b
			constexpr uint8_t mapping[32] = {
				COL(1, 1, 1),	COL(1, 1, 1),	COL(0, 2, 1),	COL(2, 2, 1),
				COL(0, 0, 1),	COL(2, 0, 1),	COL(0, 1, 1),	COL(2, 1, 1),
				COL(2, 0, 1),	COL(2, 2, 1),	COL(2, 2, 0),	COL(2, 2, 2),
				COL(2, 0, 0),	COL(2, 0, 2),	COL(2, 1, 0),	COL(2, 1, 2),
				COL(0, 0, 1),	COL(0, 2, 1),	COL(0, 2, 0),	COL(0, 2, 2),
				COL(0, 0, 0),	COL(0, 0, 2),	COL(0, 1, 0),	COL(0, 1, 2),
				COL(1, 0, 1),	COL(1, 2, 1),	COL(1, 2, 0),	COL(1, 2, 2),
				COL(1, 0, 0),	COL(1, 0, 2),	COL(1, 1, 0),	COL(1, 1, 2),
			};
#undef COL
			return mapping[colour];
		}

		enum class OutputMode {
			Sync,
			Blank,
			ColourBurst,
			Border,
			Pixels
		} previous_output_mode_ = OutputMode::Sync;
		int cycles_ = 0;

		bool was_hsync_ = false, was_vsync_ = false;
		int cycles_into_hsync_ = 0;

		Outputs::CRT::CRT crt_;
		uint8_t *pixel_data_ = nullptr, *pixel_pointer_ = nullptr;

		const uint8_t *const ram_ = nullptr;

		int next_mode_ = 2, mode_ = 2;

		int pixel_divider_ = 1;
		std::array<uint16_t, 256> mode0_output_;
		std::array<uint32_t, 256> mode1_output_;
		std::array<uint64_t, 256> mode2_output_;
		std::array<uint16_t, 256> mode3_output_;

		std::array<std::vector<uint8_t>, 16> mode0_palette_hits_;
		std::array<std::vector<uint8_t>, 4> mode1_palette_hits_;
		std::array<std::vector<uint8_t>, 4> mode3_palette_hits_;

		int pen_ = 0;
		uint8_t palette_[16];
		uint8_t border_ = 0;

		InterruptTimer &interrupt_timer_;
};
using CRTC = Motorola::CRTC::CRTC6845<
	CRTCBusHandler,
	Motorola::CRTC::Personality::HD6845S,
	Motorola::CRTC::CursorType::None>;

/*!
	Holds and vends the current keyboard state, acting as the AY's port handler.
	Also owns the joysticks.
*/
class KeyboardState: public GI::AY38910::PortHandler {
	public:
		KeyboardState() {
			joysticks_.emplace_back(new Joystick(rows_[9]));
			joysticks_.emplace_back(new Joystick(joy2_state_));
		}

		/*!
			Sets the row currently being reported to the AY.
		*/
		void set_row(int row) {
			row_ = size_t(row);
		}

		/*!
			Reports the state of the currently-selected row as Port A to the AY.
		*/
		uint8_t get_port_input(bool port_b) {
			if(!port_b && row_ < sizeof(rows_)) {
				return (row_ == 6) ? rows_[row_] & joy2_state_ : rows_[row_];
			}

			return 0xff;
		}

		/*!
			Sets whether @c key on line @c line is currently pressed.
		*/
		void set_is_pressed(bool is_pressed, int line, int key) {
			int mask = 1 << key;
			assert(size_t(line) < sizeof(rows_));
			if(is_pressed) rows_[line] &= ~mask; else rows_[line] |= mask;
		}

		/*!
			Sets all keys as currently unpressed.
		*/
		void clear_all_keys() {
			memset(rows_, 0xff, sizeof(rows_));
		}

		const std::vector<std::unique_ptr<Inputs::Joystick>> &get_joysticks() {
			return joysticks_;
		}

	private:
		uint8_t joy2_state_ = 0xff;
		uint8_t rows_[10] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
		size_t row_ = 0;
		std::vector<std::unique_ptr<Inputs::Joystick>> joysticks_;

		class Joystick: public Inputs::ConcreteJoystick {
			public:
				Joystick(uint8_t &state) :
					ConcreteJoystick({
						Input(Input::Up),
						Input(Input::Down),
						Input(Input::Left),
						Input(Input::Right),
						Input(Input::Fire, 0),
						Input(Input::Fire, 1),
					}),
					state_(state) {}

				void did_set_input(const Input &input, bool is_active) final {
					uint8_t mask = 0;
					switch(input.type) {
						default: return;
						case Input::Up:		mask = 0x01;	break;
						case Input::Down:	mask = 0x02;	break;
						case Input::Left:	mask = 0x04;	break;
						case Input::Right:	mask = 0x08;	break;
						case Input::Fire:
							if(input.info.control.index >= 2) return;
							mask = input.info.control.index ? 0x20 : 0x10;
						break;
					}

					if(is_active) state_ &= ~mask; else state_ |= mask;
				}

			private:
				uint8_t &state_;
		};
};

/*!
	Provides the mechanism of receipt for input and output of the 8255's various ports.
*/
class i8255PortHandler : public Intel::i8255::PortHandler {
	public:
		i8255PortHandler(
			KeyboardState &key_state,
			const CRTC &crtc,
			AYDeferrer &ay,
			Storage::Tape::BinaryTapePlayer &tape_player) :
				ay_(ay),
				crtc_(crtc),
				key_state_(key_state),
				tape_player_(tape_player) {}

		/// The i8255 will call this to set a new output value of @c value for @c port.
		void set_value(int port, uint8_t value) {
			switch(port) {
				case 0:
					// Port A is connected to the AY's data bus.
					ay_.update();
					ay_.ay().set_data_input(value);
				break;
				case 1:
					// Port B is an input only. So output goes nowehere.
				break;
				case 2: {
					// The low four bits of the value sent to Port C select a keyboard line.
					int key_row = value & 15;
					key_state_.set_row(key_row);

					// Bit 4 sets the tape motor on or off.
					tape_player_.set_motor_control((value & 0x10) ? true : false);
					// Bit 5 sets the current tape output level
					tape_player_.set_tape_output((value & 0x20) ? true : false);

					// Bits 6 and 7 set BDIR and BC1 for the AY.
					ay_.ay().set_control_lines(
						(GI::AY38910::ControlLines)(
							((value & 0x80) ? GI::AY38910::BDIR : 0) |
							((value & 0x40) ? GI::AY38910::BC1 : 0) |
							GI::AY38910::BC2
						));
				} break;
			}
		}

		/// The i8255 will call this to obtain a new input for @c port.
		uint8_t get_value(int port) {
			switch(port) {
				case 0: return ay_.ay().get_data_output();	// Port A is wired to the AY
				case 1:	return
					(crtc_.get_bus_state().vsync ? 0x01 : 0x00) |	// Bit 0 returns CRTC vsync.
					(tape_player_.get_input() ? 0x80 : 0x00) |		// Bit 7 returns cassette input.
					0x7e;	// Bits unimplemented:
							//
							//	Bit 6: printer ready (1 = not)
							//	Bit 5: the expansion port /EXP pin, so depends on connected hardware
							//	Bit 4: 50/60Hz switch (1 = 50Hz)
							//	Bits 1-3: distributor ID (111 = Amstrad)
				default: return 0xff;
			}
		}

	private:
		AYDeferrer &ay_;
		const CRTC &crtc_;
		KeyboardState &key_state_;
		Storage::Tape::BinaryTapePlayer &tape_player_;
};

/*!
	The actual Amstrad CPC implementation; tying the 8255, 6845 and AY to the Z80.
*/
template <bool has_fdc> class ConcreteMachine:
	public MachineTypes::ScanProducer,
	public MachineTypes::AudioProducer,
	public MachineTypes::TimedMachine,
	public MachineTypes::MediaTarget,
	public MachineTypes::MappedKeyboardMachine,
	public MachineTypes::JoystickMachine,
	public Utility::TypeRecipient<CharacterMapper>,
	public CPU::Z80::BusHandler,
	public ClockingHint::Observer,
	public Configurable::Device,
	public Machine,
	public Activity::Source {
	public:
		ConcreteMachine(const Analyser::Static::AmstradCPC::Target &target, const ROMMachine::ROMFetcher &rom_fetcher) :
			z80_(*this),
			crtc_bus_handler_(ram_, interrupt_timer_),
			crtc_(crtc_bus_handler_),
			i8255_port_handler_(key_state_, crtc_, ay_, tape_player_),
			i8255_(i8255_port_handler_),
			tape_player_(8000000),
			crtc_counter_(HalfCycles(4))	// This starts the CRTC exactly out of phase with the CPU's memory accesses
		{
			// primary clock is 4Mhz
			set_clock_rate(4000000);

			// ensure memory starts in a random state
			Memory::Fuzz(ram_, sizeof(ram_));

			// register this class as the sleep observer for the FDC and tape
			fdc_.set_clocking_hint_observer(this);
			tape_player_.set_clocking_hint_observer(this);

			// install the keyboard state class as the AY port handler
			ay_.ay().set_port_handler(&key_state_);

			// construct the list of necessary ROMs
			bool has_amsdos = false;
			ROM::Name firmware, basic;

			using Model = Analyser::Static::AmstradCPC::Target::Model;
			switch(target.model) {
				case Model::CPC464:
					firmware = ROM::Name::CPC464Firmware;
					basic = ROM::Name::CPC464BASIC;
				break;
				case Model::CPC664:
					firmware = ROM::Name::CPC664Firmware;
					basic = ROM::Name::CPC664BASIC;
					has_amsdos = true;
				break;
				default:
					firmware = ROM::Name::CPC6128Firmware;
					basic = ROM::Name::CPC6128BASIC;
					has_amsdos = true;
				break;
			}

			ROM::Request request = ROM::Request(firmware) && ROM::Request(basic);
			if(has_amsdos) {
				request = request && ROM::Request(ROM::Name::AMSDOS);
			}

			// Fetch and verify the ROMs.
			auto roms = rom_fetcher(request);
			if(!request.validate(roms)) {
				throw ROMMachine::Error::MissingROMs;
			}

			if(has_amsdos) {
				roms_[ROMType::AMSDOS] = roms.find(ROM::Name::AMSDOS)->second;
			}
			roms_[ROMType::OS] = roms.find(firmware)->second;
			roms_[ROMType::BASIC] = roms.find(basic)->second;

			// Establish default memory map
			upper_rom_is_paged_ = true;
			upper_rom_ = ROMType::BASIC;

			write_pointers_[0] = &ram_[0x0000];
			write_pointers_[1] = &ram_[0x4000];
			write_pointers_[2] = &ram_[0x8000];
			write_pointers_[3] = &ram_[0xc000];

			read_pointers_[0] = roms_[ROMType::OS].data();
			read_pointers_[1] = write_pointers_[1];
			read_pointers_[2] = write_pointers_[2];
			read_pointers_[3] = roms_[upper_rom_].data();

			// Set total RAM available.
			has_128k_ = target.model == Model::CPC6128;

			// Type whatever is required.
			if(!target.loading_command.empty()) {
				type_string(target.loading_command);
			}

			insert_media(target.media);
		}

		/// The entry point for performing a partial Z80 machine cycle.
		forceinline HalfCycles perform_machine_cycle(const CPU::Z80::PartialMachineCycle &cycle) {
			// Amstrad CPC timing scheme: assert WAIT for three out of four cycles
			clock_offset_ = (clock_offset_ + cycle.length) & HalfCycles(7);
			z80_.set_wait_line(clock_offset_ >= HalfCycles(2));

			// Update the CRTC once every eight half cycles; aiming for half-cycle 4 as
			// per the initial seed to the crtc_counter_, but any time in the final four
			// will do as it's safe to conclude that nobody else has touched video RAM
			// during that whole window
			crtc_counter_ += cycle.length;
			const Cycles crtc_cycles = crtc_counter_.divide_cycles(Cycles(4));
			if(crtc_cycles > Cycles(0)) crtc_.run_for(crtc_cycles);

			// Check whether that prompted a change in the interrupt line. If so then date
			// it to whenever the cycle was triggered.
			if(interrupt_timer_.request_has_changed()) z80_.set_interrupt_line(interrupt_timer_.get_request(), -crtc_counter_);

			// TODO (in the player, not here): adapt it to accept an input clock rate and
			// run_for as HalfCycles
			if(!tape_player_is_sleeping_) tape_player_.run_for(cycle.length.as_integral());

			// Pump the AY
			ay_.run_for(cycle.length);

			if constexpr (has_fdc) {
				// Clock the FDC, if connected, using a lazy scale by two
				time_since_fdc_update_ += cycle.length;
			}

			// Update typing activity
			if(typer_) typer_->run_for(cycle.length);

			// Stop now if no action is strictly required.
			if(!cycle.is_terminal()) return HalfCycles(0);

			uint16_t address = cycle.address ? *cycle.address : 0x0000;
			switch(cycle.operation) {
				case CPU::Z80::PartialMachineCycle::ReadOpcode:

					// TODO: just capturing byte reads as below doesn't seem to do that much in terms of acceleration;
					// I'm not immediately clear whether that's just because the machine still has to sit through
					// pilot tone in real time, or just that almost no software uses the ROM loader.
					if(use_fast_tape_hack_ && address == tape_read_byte_address && read_pointers_[0] == roms_[ROMType::OS].data()) {
						using Parser = Storage::Tape::ZXSpectrum::Parser;
						Parser parser(Parser::MachineType::AmstradCPC);

						const auto speed = read_pointers_[tape_speed_value_address >> 14][tape_speed_value_address & 16383];
						parser.set_cpc_read_speed(speed);

						// Seed with the current pulse; the CPC will have finished the
						// preceding symbol and be a short way into the pulse that should determine the
						// first bit of this byte.
						parser.process_pulse(tape_player_.get_current_pulse());
						const auto byte = parser.get_byte(tape_player_.get_tape());
						auto flags = z80_.value_of(CPU::Z80::Register::Flags);

						if(byte) {
							// In A ROM-esque fashion, begin the first pulse after the final one
							// that was just consumed.
							tape_player_.complete_pulse();

							// Update in-memory CRC.
							auto crc_value =
								uint16_t(
									read_pointers_[tape_crc_address >> 14][tape_crc_address & 16383] |
									(read_pointers_[(tape_crc_address+1) >> 14][(tape_crc_address+1) & 16383] << 8)
								);

							tape_crc_.set_value(crc_value);
							tape_crc_.add(*byte);
							crc_value = tape_crc_.get_value();

							write_pointers_[tape_crc_address >> 14][tape_crc_address & 16383] = uint8_t(crc_value);
							write_pointers_[(tape_crc_address+1) >> 14][(tape_crc_address+1) & 16383] = uint8_t(crc_value >> 8);

							// Indicate successful byte read.
							z80_.set_value_of(CPU::Z80::Register::A, *byte);
							flags |= CPU::Z80::Flag::Carry;
						} else {
							// TODO: return tape player to previous state and decline to serve.
							z80_.set_value_of(CPU::Z80::Register::A, 0);
							flags &= ~CPU::Z80::Flag::Carry;
						}
						z80_.set_value_of(CPU::Z80::Register::Flags, flags);

						// RET.
						*cycle.value = 0xc9;
						break;
					}
				[[fallthrough]];

				case CPU::Z80::PartialMachineCycle::Read:
					*cycle.value = read_pointers_[address >> 14][address & 16383];
				break;

				case CPU::Z80::PartialMachineCycle::Write:
					write_pointers_[address >> 14][address & 16383] = *cycle.value;
				break;

				case CPU::Z80::PartialMachineCycle::Output:
					// Check for a gate array access.
					if((address & 0xc000) == 0x4000) {
						write_to_gate_array(*cycle.value);
					}

					// Check for an upper ROM selection
					if constexpr (has_fdc) {
						if(!(address&0x2000)) {
							upper_rom_ = (*cycle.value == 7) ? ROMType::AMSDOS : ROMType::BASIC;
							if(upper_rom_is_paged_) read_pointers_[3] = roms_[upper_rom_].data();
						}
					}

					// Check for a CRTC access
					if(!(address & 0x4000)) {
						switch((address >> 8) & 3) {
							case 0:	crtc_.select_register(*cycle.value);	break;
							case 1:	crtc_.set_register(*cycle.value);		break;
							default: break;
						}
					}

					// Check for an 8255 PIO access
					if(!(address & 0x800)) {
						i8255_.write((address >> 8) & 3, *cycle.value);
					}

					if constexpr (has_fdc) {
						// Check for an FDC access
						if((address & 0x580) == 0x100) {
							flush_fdc();
							fdc_.write(address & 1, *cycle.value);
						}

						// Check for a disk motor access
						if(!(address & 0x580)) {
							flush_fdc();
							fdc_.set_motor_on(!!(*cycle.value));
						}
					}
				break;

				case CPU::Z80::PartialMachineCycle::Input:
					// Default to nothing answering
					*cycle.value = 0xff;

					// Check for a PIO access
					if(!(address & 0x800)) {
						*cycle.value &= i8255_.read((address >> 8) & 3);
					}

					// Check for an FDC access
					if constexpr (has_fdc) {
						if((address & 0x580) == 0x100) {
							flush_fdc();
							*cycle.value &= fdc_.read(address & 1);
						}
					}

					// Check for a CRTC access; the below is not a typo, the CRTC can be selected
					// for writing via an input, and will sample whatever happens to be available
					if(!(address & 0x4000)) {
						switch((address >> 8) & 3) {
							case 0:	crtc_.select_register(*cycle.value);	break;
							case 1:	crtc_.set_register(*cycle.value);		break;
							case 2: *cycle.value &= crtc_.get_status();		break;
							case 3:	*cycle.value &= crtc_.get_register();	break;
						}
					}

					// As with the CRTC, the gate array will sample the bus if the address decoding
					// implies that it should, unaware of data direction
					if((address & 0xc000) == 0x4000) {
						write_to_gate_array(*cycle.value);
					}
				break;

				case CPU::Z80::PartialMachineCycle::Interrupt:
					// Nothing is loaded onto the bus during an interrupt acknowledge, but
					// the fact of the acknowledge needs to be posted on to the interrupt timer.
					*cycle.value = 0xff;
					interrupt_timer_.signal_interrupt_acknowledge();
				break;

				default: break;
			}

			// Check whether the interrupt signal has changed the other way.
			if(interrupt_timer_.request_has_changed()) z80_.set_interrupt_line(interrupt_timer_.get_request());

			// This implementation doesn't use time-stuffing; once in-phase waits won't be longer
			// than a single cycle so there's no real performance benefit to trying to find the
			// next non-wait when a wait cycle comes in, and there'd be no benefit to reproducing
			// the Z80's knowledge of where wait cycles occur here.
			return HalfCycles(0);
		}

		/// Fields requests to pump all output.
		void flush_output(int outputs) final {
			// Just flush the AY.
			if(outputs & Output::Audio) {
				ay_.update();
				ay_.flush();
			}

			// Always flush the FDC.
			flush_fdc();
		}

		/// A CRTMachine function; sets the destination for video.
		void set_scan_target(Outputs::Display::ScanTarget *scan_target) final {
			crtc_bus_handler_.set_scan_target(scan_target);
		}

		/// A CRTMachine function; returns the current scan status.
		Outputs::Display::ScanStatus get_scaled_scan_status() const final {
			return crtc_bus_handler_.get_scaled_scan_status();
		}

		/// A CRTMachine function; sets the output display type.
		void set_display_type(Outputs::Display::DisplayType display_type) final {
			crtc_bus_handler_.set_display_type(display_type);
		}

		/// A CRTMachine function; gets the output display type.
		Outputs::Display::DisplayType get_display_type() const final {
			return crtc_bus_handler_.get_display_type();
		}

		/// @returns the speaker in use.
		Outputs::Speaker::Speaker *get_speaker() final {
			return ay_.get_speaker();
		}

		/// Wires virtual-dispatched CRTMachine run_for requests to the static Z80 method.
		void run_for(const Cycles cycles) final {
			z80_.run_for(cycles);
		}

		bool insert_media(const Analyser::Static::Media &media) final {
			// If there are any tapes supplied, use the first of them.
			if(!media.tapes.empty()) {
				tape_player_.set_tape(media.tapes.front());
				set_use_fast_tape_hack();
			}

			// Insert up to four disks.
			int c = 0;
			for(auto &disk : media.disks) {
				fdc_.set_disk(disk, c);
				c++;
				if(c == 4) break;
			}

			return !media.tapes.empty() || (!media.disks.empty() && has_fdc);
		}

		void set_component_prefers_clocking(ClockingHint::Source *, ClockingHint::Preference) final {
			fdc_is_sleeping_ = fdc_.preferred_clocking() == ClockingHint::Preference::None;
			tape_player_is_sleeping_ = tape_player_.preferred_clocking() == ClockingHint::Preference::None;
		}

		// MARK: - Keyboard
		void type_string(const std::string &string) final {
			Utility::TypeRecipient<CharacterMapper>::add_typer(string);
		}

		bool can_type(char c) const final {
			return Utility::TypeRecipient<CharacterMapper>::can_type(c);
		}

		HalfCycles get_typer_delay(const std::string &) const final {
			return z80_.get_is_resetting() ? Cycles(3'400'000) : Cycles(0);
		}

		HalfCycles get_typer_frequency() const final {
			return Cycles(160'000);	// Perform one key transition per frame and a half.
		}

		// See header; sets a key as either pressed or released.
		void set_key_state(uint16_t key, bool isPressed) final {
			key_state_.set_is_pressed(isPressed, key >> 4, key & 7);
		}

		// See header; sets all keys to released.
		void clear_all_keys() final {
			key_state_.clear_all_keys();
		}

		KeyboardMapper *get_keyboard_mapper() final {
			return &keyboard_mapper_;
		}

		// MARK: - Activity Source
		void set_activity_observer([[maybe_unused]] Activity::Observer *observer) final {
			if constexpr (has_fdc) fdc_.set_activity_observer(observer);
			tape_player_.set_activity_observer(observer);
		}

		// MARK: - Configuration options.
		std::unique_ptr<Reflection::Struct> get_options() final {
			auto options = std::make_unique<Options>(Configurable::OptionsType::UserFriendly);
			options->output = get_video_signal_configurable();
			options->quickload = allow_fast_tape_hack_;
			return options;
		}

		void set_options(const std::unique_ptr<Reflection::Struct> &str) {
			const auto options = dynamic_cast<Options *>(str.get());
			set_video_signal_configurable(options->output);
			allow_fast_tape_hack_ = options->quickload;
			set_use_fast_tape_hack();
		}

		// MARK: - Joysticks
		const std::vector<std::unique_ptr<Inputs::Joystick>> &get_joysticks() final {
			return key_state_.get_joysticks();
		}

	private:
		inline void write_to_gate_array(uint8_t value) {
			switch(value >> 6) {
				case 0: crtc_bus_handler_.select_pen(value & 0x1f);		break;
				case 1: crtc_bus_handler_.set_colour(value & 0x1f);		break;
				case 2:
					// Perform ROM paging.
					read_pointers_[0] = (value & 4) ? write_pointers_[0] : roms_[ROMType::OS].data();

					upper_rom_is_paged_ = !(value & 8);
					read_pointers_[3] = upper_rom_is_paged_ ? roms_[upper_rom_].data() : write_pointers_[3];

					// Reset the interrupt timer if requested.
					if(value & 0x10) interrupt_timer_.reset_count();

					// Post the next mode.
					crtc_bus_handler_.set_next_mode(value & 3);
				break;
				case 3:
					// Perform RAM paging, if 128kb is permitted.
					if(has_128k_) {
						const bool adjust_low_read_pointer = read_pointers_[0] == write_pointers_[0];
						const bool adjust_high_read_pointer = read_pointers_[3] == write_pointers_[3];
#define RAM_BANK(x) &ram_[x * 16384]
#define RAM_CONFIG(a, b, c, d) write_pointers_[0] = RAM_BANK(a); write_pointers_[1] = RAM_BANK(b); write_pointers_[2] = RAM_BANK(c); write_pointers_[3] = RAM_BANK(d);
						switch(value & 7) {
							case 0:	RAM_CONFIG(0, 1, 2, 3);	break;
							case 1:	RAM_CONFIG(0, 1, 2, 7);	break;
							case 2:	RAM_CONFIG(4, 5, 6, 7);	break;
							case 3:	RAM_CONFIG(0, 3, 2, 7);	break;
							case 4:	RAM_CONFIG(0, 4, 2, 3);	break;
							case 5:	RAM_CONFIG(0, 5, 2, 3);	break;
							case 6:	RAM_CONFIG(0, 6, 2, 3);	break;
							case 7:	RAM_CONFIG(0, 7, 2, 3);	break;
						}
#undef RAM_CONFIG
#undef RAM_BANK
						if(adjust_low_read_pointer) read_pointers_[0] = write_pointers_[0];
						read_pointers_[1] = write_pointers_[1];
						read_pointers_[2] = write_pointers_[2];
						if(adjust_high_read_pointer) read_pointers_[3] = write_pointers_[3];
					}
				break;
			}
		}

		CPU::Z80::Processor<ConcreteMachine, false, true> z80_;

		CRTCBusHandler crtc_bus_handler_;
		CRTC crtc_;

		AYDeferrer ay_;
		i8255PortHandler i8255_port_handler_;
		Intel::i8255::i8255<i8255PortHandler> i8255_;

		Amstrad::FDC fdc_;
		HalfCycles time_since_fdc_update_;
		void flush_fdc() {
			if constexpr (has_fdc) {
				// Clock the FDC, if connected, using a lazy scale by two
				if(!fdc_is_sleeping_) {
					fdc_.run_for(Cycles(time_since_fdc_update_.as_integral()));
				}
				time_since_fdc_update_ = HalfCycles(0);
			}
		}

		InterruptTimer interrupt_timer_;
		Storage::Tape::BinaryTapePlayer tape_player_;

		// By luck these values are the same between the 664 and the 6128;
		// therefore the has_fdc template flag is sufficient to locate them.
		static constexpr uint16_t tape_read_byte_address = has_fdc ? 0x2b20 : 0x29b0;
		static constexpr uint16_t tape_speed_value_address = has_fdc ? 0xb1e7 : 0xbc8f;
		static constexpr uint16_t tape_crc_address = has_fdc ? 0xb1eb : 0xb8d3;
		CRC::CCITT tape_crc_;
		bool use_fast_tape_hack_ = false;
		bool allow_fast_tape_hack_ = false;
		void set_use_fast_tape_hack() {
			use_fast_tape_hack_ = allow_fast_tape_hack_ && tape_player_.has_tape();
		}

		HalfCycles clock_offset_;
		HalfCycles crtc_counter_;
		HalfCycles half_cycles_since_ay_update_;

		bool fdc_is_sleeping_ = false;
		bool tape_player_is_sleeping_ = false;
		bool has_128k_ = false;

		enum ROMType: int {
			AMSDOS = 0, OS = 1, BASIC = 2
		};
		std::vector<uint8_t> roms_[3];
		bool upper_rom_is_paged_ = false;
		ROMType upper_rom_;

		uint8_t *ram_pages_[4]{};
		const uint8_t *read_pointers_[4]{};
		uint8_t *write_pointers_[4]{};

		KeyboardState key_state_;
		AmstradCPC::KeyboardMapper keyboard_mapper_;

		bool has_run_ = false;
		uint8_t ram_[128 * 1024];
};

}

using namespace AmstradCPC;

// See header; constructs and returns an instance of the Amstrad CPC.
std::unique_ptr<Machine> Machine::AmstradCPC(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	using Target = Analyser::Static::AmstradCPC::Target;
	const Target *const cpc_target = dynamic_cast<const Target *>(target);
	switch(cpc_target->model) {
		default:					return std::make_unique<AmstradCPC::ConcreteMachine<true>>(*cpc_target, rom_fetcher);
		case Target::Model::CPC464:	return std::make_unique<AmstradCPC::ConcreteMachine<false>>(*cpc_target, rom_fetcher);
	}
}

Machine::~Machine() {}
