//
//  AmstradCPC.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 30/07/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "AmstradCPC.hpp"

#include "CharacterMapper.hpp"

#include "../../Processors/Z80/Z80.hpp"

#include "../../Components/6845/CRTC6845.hpp"
#include "../../Components/8255/i8255.hpp"
#include "../../Components/8272/i8272.hpp"
#include "../../Components/AY38910/AY38910.hpp"

#include "../MemoryFuzzer.hpp"
#include "../Typer.hpp"

#include "../../Storage/Tape/Tape.hpp"

#include "../../ClockReceiver/ForceInline.hpp"

namespace AmstradCPC {

/*!
	Models the CPC's interrupt timer. Inputs are vsync, hsync, interrupt acknowledge and reset, and its output
	is simply yes or no on whether an interupt is currently requested. Internally it uses a counter with a period
	of 52 and occasionally adjusts or makes decisions based on bit 4.

	Hsync and vsync signals are expected to come directly from the CRTC; they are not decoded from a composite stream.
*/
class InterruptTimer {
	public:
		InterruptTimer() : timer_(0), interrupt_request_(false) {}

		/*!
			Indicates that a new hsync pulse has been recognised. This should be
			supplied on the falling edge of the CRTC HSYNC signal, which is the
			trailing edge because it is active high.
		*/
		inline void signal_hsync() {
			// Increment the timer and if it has hit 52 then reset it and
			// set the interrupt request line to true.
			timer_++;
			if(timer_ == 52) {
				timer_ = 0;
				interrupt_request_ = true;
			}

			// If a vertical sync has previously been indicated then after two
			// further horizontal syncs the timer should either (i) set the interrupt
			// line, if bit 4 is clear; or (ii) reset the timer.
			if(reset_counter_) {
				reset_counter_--;
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
		int reset_counter_;
		bool interrupt_request_;
		bool last_interrupt_request_;
		int timer_;
};

/*!
	Provides a holder for an AY-3-8910 and its current cycles-since-updated count.
	Therefore acts both to store an AY and to bookkeep this emulator's idiomatic
	deferred clocking for this component.
*/
class AYDeferrer {
	public:
		/// Constructs a new AY instance and sets its clock rate.
		inline void setup_output() {
			ay_.reset(new GI::AY38910::AY38910);
			ay_->set_input_rate(1000000);
		}

		/// Destructs the AY.
		inline void close_output() {
			ay_.reset();
		}

		/// Adds @c half_cycles half cycles to the amount of time that has passed.
		inline void run_for(HalfCycles half_cycles) {
			cycles_since_update_ += half_cycles;
		}

		/// Enqueues an update-to-now into the AY's deferred queue.
		inline void update() {
			ay_->run_for(cycles_since_update_.divide_cycles(Cycles(4)));
		}

		/// Issues a request to the AY to perform all processing up to the current time.
		inline void flush() {
			ay_->flush();
		}

		/// @returns the speaker the AY is using for output.
		std::shared_ptr<Outputs::Speaker> get_speaker() {
			return ay_;
		}

		/// @returns the AY itself.
		GI::AY38910::AY38910 *ay() {
			return ay_.get();
		}

	private:
		std::shared_ptr<GI::AY38910::AY38910> ay_;
		HalfCycles cycles_since_update_;
};

/*!
	Provides the mechanism of receipt for the CRTC outputs. In practice has the gate array's
	video fetching and serialisation logic built in. So this is responsible for all video
	generation and therefore owns details such as the current palette.
*/
class CRTCBusHandler {
	public:
		CRTCBusHandler(uint8_t *ram, InterruptTimer &interrupt_timer) :
			cycles_(0),
			was_enabled_(false),
			was_sync_(false),
			pixel_data_(nullptr),
			pixel_pointer_(nullptr),
			was_hsync_(false),
			ram_(ram),
			interrupt_timer_(interrupt_timer),
			pixel_divider_(1),
			mode_(2),
			next_mode_(2),
			cycles_into_hsync_(0) {
				establish_palette_hits();
				build_mode_table();
			}

		/*!
			The CRTC entry function; takes the current bus state and determines what output 
			to produce based on the current palette and mode.
		*/
		forceinline void perform_bus_cycle(const Motorola::CRTC::BusState &state) {
			// The gate array waits 2µs to react to the CRTC's vsync signal, and then
			// caps output at 4µs. Since the clock rate is 1Mhz, that's 2 and 4 cycles,
			// respectively.
			if(state.hsync) {
				cycles_into_hsync_++;
			} else {
				cycles_into_hsync_ = 0;
			}

			bool is_hsync = (cycles_into_hsync_ >= 2 && cycles_into_hsync_ < 6);

			// Sync is taken to override pixels, and is combined as a simple OR.
			bool is_sync = is_hsync || state.vsync;

			// If a transition between sync/border/pixels just occurred, flush whatever was
			// in progress to the CRT and reset counting.
			if(state.display_enable != was_enabled_ || is_sync != was_sync_) {
				if(was_sync_) {
					crt_->output_sync(cycles_ * 16);
				} else {
					if(was_enabled_) {
						if(cycles_) {
							crt_->output_data(cycles_ * 16, pixel_divider_);
							pixel_pointer_ = pixel_data_ = nullptr;
						}
					} else {
						output_border(cycles_);
					}
				}

				cycles_ = 0;
				was_sync_ = is_sync;
				was_enabled_ = state.display_enable;
			}

			// increment cycles since state changed
			cycles_++;

			// collect some more pixels if output is ongoing
			if(!is_sync && state.display_enable) {
				if(!pixel_data_) {
					pixel_pointer_ = pixel_data_ = crt_->allocate_write_area(320);
				}
				if(pixel_pointer_) {
					// the CPC shuffles output lines as:
					//	MA13 MA12	RA2 RA1 RA0		MA9 MA8 MA7 MA6 MA5 MA4 MA3 MA2 MA1 MA0		CCLK
					// ... so form the real access address.
					uint16_t address =
						(uint16_t)(
							((state.refresh_address & 0x3ff) << 1) |
							((state.row_address & 0x7) << 11) |
							((state.refresh_address & 0x3000) << 2)
						);

					// fetch two bytes and translate into pixels
					switch(mode_) {
						case 0:
							((uint16_t *)pixel_pointer_)[0] = mode0_output_[ram_[address]];
							((uint16_t *)pixel_pointer_)[1] = mode0_output_[ram_[address+1]];
							pixel_pointer_ += 4;
						break;

						case 1:
							((uint32_t *)pixel_pointer_)[0] = mode1_output_[ram_[address]];
							((uint32_t *)pixel_pointer_)[1] = mode1_output_[ram_[address+1]];
							pixel_pointer_ += 8;
						break;

						case 2:
							((uint64_t *)pixel_pointer_)[0] = mode2_output_[ram_[address]];
							((uint64_t *)pixel_pointer_)[1] = mode2_output_[ram_[address+1]];
							pixel_pointer_ += 16;
						break;

						case 3:
							((uint16_t *)pixel_pointer_)[0] = mode3_output_[ram_[address]];
							((uint16_t *)pixel_pointer_)[1] = mode3_output_[ram_[address+1]];
							pixel_pointer_ += 4;
						break;

					}

					// flush the current buffer pixel if full; the CRTC allows many different display
					// widths so it's not necessarily possible to predict the correct number in advance
					// and using the upper bound could lead to inefficient behaviour
					if(pixel_pointer_ == pixel_data_ + 320) {
						crt_->output_data(cycles_ * 16, pixel_divider_);
						pixel_pointer_ = pixel_data_ = nullptr;
						cycles_ = 0;
					}
				}
			}

			// check for a trailing CRTC hsync; if one occurred then that's the trigger potentially to change
			// modes, and should also be sent on to the interrupt timer
			if(was_hsync_ && !state.hsync) {
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

				interrupt_timer_.signal_hsync();
			}

			// check for a leading vsync; that also needs to be communicated to the interrupt timer
			if(!was_vsync_ && state.vsync) {
				interrupt_timer_.signal_vsync();
			}

			// update current state for edge detection next time around
			was_vsync_ = state.vsync;
			was_hsync_ = state.hsync;
		}

		/// Constructs an appropriate CRT for video output.
		void setup_output(float aspect_ratio) {
			crt_.reset(new Outputs::CRT::CRT(1024, 16, Outputs::CRT::DisplayType::PAL50, 1));
			crt_->set_rgb_sampling_function(
				"vec3 rgb_sample(usampler2D sampler, vec2 coordinate, vec2 icoordinate)"
				"{"
					"uint sample = texture(texID, coordinate).r;"
					"return vec3(float((sample >> 4) & 3u), float((sample >> 2) & 3u), float(sample & 3u)) / 2.0;"
				"}");
			crt_->set_visible_area(Outputs::CRT::Rect(0.075f, 0.05f, 0.9f, 0.9f));
			crt_->set_output_device(Outputs::CRT::Monitor);
		}

		/// Destructs the CRT.
		void close_output() {
			crt_.reset();
		}

		/// @returns the CRT.
		std::shared_ptr<Outputs::CRT::CRT> get_crt() {
			return crt_;
		}

		/*!
			Sets the next video mode. Per the documentation, mode changes take effect only at the end of line,
			not immediately. So next means "as of the end of this line".
		*/
		void set_next_mode(int mode) {
			next_mode_ = mode;
		}

		/// @returns the current value of the CRTC's vertical sync output.
		bool get_vsync() const {
			return was_vsync_;
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
				if(!was_sync_ && !was_enabled_) {
					output_border(cycles_);
					cycles_ = 0;
				}
				border_ = mapped_palette_value(colour);
			} else {
				palette_[pen_] = mapped_palette_value(colour);
				patch_mode_table(pen_);
			}
		}

	private:
		void output_border(unsigned int length) {
			uint8_t *colour_pointer = (uint8_t *)crt_->allocate_write_area(1);
			if(colour_pointer) *colour_pointer = border_;
			crt_->output_level(length * 16);
		}

#define Mode0Colour0(c) ((c & 0x80) >> 7) | ((c & 0x20) >> 3) | ((c & 0x08) >> 2) | ((c & 0x02) << 2)
#define Mode0Colour1(c) ((c & 0x40) >> 6) | ((c & 0x10) >> 2) | ((c & 0x04) >> 1) | ((c & 0x01) << 3)

#define Mode1Colour0(c) ((c & 0x80) >> 7) | ((c & 0x08) >> 2)
#define Mode1Colour1(c) ((c & 0x40) >> 6) | ((c & 0x04) >> 1)
#define Mode1Colour2(c) ((c & 0x20) >> 5) | ((c & 0x02) >> 0)
#define Mode1Colour3(c) ((c & 0x10) >> 4) | ((c & 0x01) << 1)

#define Mode3Colour0(c)	((c & 0x80) >> 7) | ((c & 0x08) >> 2)
#define Mode3Colour1(c) ((c & 0x40) >> 6) | ((c & 0x04) >> 1)

		void establish_palette_hits() {
			for(int c = 0; c < 256; c++) {
				mode0_palette_hits_[Mode0Colour0(c)].push_back((uint8_t)c);
				mode0_palette_hits_[Mode0Colour1(c)].push_back((uint8_t)c);

				mode1_palette_hits_[Mode1Colour0(c)].push_back((uint8_t)c);
				mode1_palette_hits_[Mode1Colour1(c)].push_back((uint8_t)c);
				mode1_palette_hits_[Mode1Colour2(c)].push_back((uint8_t)c);
				mode1_palette_hits_[Mode1Colour3(c)].push_back((uint8_t)c);

				mode3_palette_hits_[Mode3Colour0(c)].push_back((uint8_t)c);
				mode3_palette_hits_[Mode3Colour1(c)].push_back((uint8_t)c);
			}
		}

		void build_mode_table() {
			switch(mode_) {
				case 0:
					// Mode 0: abcdefgh -> [gcea] [hdfb]
					for(int c = 0; c < 256; c++) {
						// prepare mode 0
						uint8_t *mode0_pixels = (uint8_t *)&mode0_output_[c];
						mode0_pixels[0] = palette_[Mode0Colour0(c)];
						mode0_pixels[1] = palette_[Mode0Colour1(c)];
					}
				break;

				case 1:
					for(int c = 0; c < 256; c++) {
						// prepare mode 1
						uint8_t *mode1_pixels = (uint8_t *)&mode1_output_[c];
						mode1_pixels[0] = palette_[Mode1Colour0(c)];
						mode1_pixels[1] = palette_[Mode1Colour1(c)];
						mode1_pixels[2] = palette_[Mode1Colour2(c)];
						mode1_pixels[3] = palette_[Mode1Colour3(c)];
					}
				break;

				case 2:
					for(int c = 0; c < 256; c++) {
						// prepare mode 2
						uint8_t *mode2_pixels = (uint8_t *)&mode2_output_[c];
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
					for(int c = 0; c < 256; c++) {
						// prepare mode 3
						uint8_t *mode3_pixels = (uint8_t *)&mode3_output_[c];
						mode3_pixels[0] = palette_[Mode3Colour0(c)];
						mode3_pixels[1] = palette_[Mode3Colour1(c)];
					}
				break;
			}
		}

		void patch_mode_table(int pen) {
			switch(mode_) {
				case 0: {
					for(uint8_t c : mode0_palette_hits_[pen]) {
						uint8_t *mode0_pixels = (uint8_t *)&mode0_output_[c];
						mode0_pixels[0] = palette_[Mode0Colour0(c)];
						mode0_pixels[1] = palette_[Mode0Colour1(c)];
					}
				} break;
				case 1:
					if(pen > 3) return;
					for(uint8_t c : mode1_palette_hits_[pen]) {
						uint8_t *mode1_pixels = (uint8_t *)&mode1_output_[c];
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
					if(pen > 3) return;
					// Same argument applies here as to case 1, as the unused bits aren't masked out.
					for(uint8_t c : mode3_palette_hits_[pen]) {
						uint8_t *mode3_pixels = (uint8_t *)&mode3_output_[c];
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
			static const uint8_t mapping[32] = {
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

		unsigned int cycles_;

		bool was_enabled_, was_sync_, was_hsync_, was_vsync_;
		int cycles_into_hsync_;

		std::shared_ptr<Outputs::CRT::CRT> crt_;
		uint8_t *pixel_data_, *pixel_pointer_;

		uint8_t *ram_;

		int next_mode_, mode_;

		unsigned int pixel_divider_;
		uint16_t mode0_output_[256];
		uint32_t mode1_output_[256];
		uint64_t mode2_output_[256];
		uint16_t mode3_output_[256];

		std::vector<uint8_t> mode0_palette_hits_[16];
		std::vector<uint8_t> mode1_palette_hits_[4];
		std::vector<uint8_t> mode3_palette_hits_[4];

		int pen_;
		uint8_t palette_[16];
		uint8_t border_;

		InterruptTimer &interrupt_timer_;
};

/*!
	Holds and vends the current keyboard state, acting as the AY's port handler.
*/
class KeyboardState: public GI::AY38910::PortHandler {
	public:
		KeyboardState() : rows_{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff} {}

		/*!
			Sets the row currently being reported to the AY.
		*/
		void set_row(int row) {
			row_ = row;
		}

		/*!
			Reports the state of the currently-selected row as Port A to the AY.
		*/
		uint8_t get_port_input(bool port_b) {
			if(!port_b && row_ < 10) {
				return rows_[row_];
			}

			return 0xff;
		}

		/*!
			Sets whether @c key on line @c line is currently pressed.
		*/
		void set_is_pressed(bool is_pressed, int line, int key) {
			int mask = 1 << key;
			assert(line < 10);
			if(is_pressed) rows_[line] &= ~mask; else rows_[line] |= mask;
		}

		/*!
			Sets all keys as currently unpressed.
		*/
		void clear_all_keys() {
			memset(rows_, 0xff, 10);
		}

	private:
		uint8_t rows_[10];
		int row_;
};

/*!
	Wraps the 8272 so as to provide proper clocking and RPM counts, and just directly
	exposes motor control, applying the same value to all drives.
*/
class FDC: public Intel::i8272::i8272 {
	private:
		Intel::i8272::BusHandler bus_handler_;

	public:
		FDC() : i8272(bus_handler_, Cycles(8000000), 16, 300) {}

		void set_motor_on(bool on) {
			Intel::i8272::i8272::set_motor_on(on);
		}
};

/*!
	Provides the mechanism of receipt for input and output of the 8255's various ports.
*/
class i8255PortHandler : public Intel::i8255::PortHandler {
	public:
		i8255PortHandler(
			KeyboardState &key_state,
			const CRTCBusHandler &crtc_bus_handler,
			AYDeferrer &ay,
			Storage::Tape::BinaryTapePlayer &tape_player) :
				key_state_(key_state),
				crtc_bus_handler_(crtc_bus_handler),
				ay_(ay),
				tape_player_(tape_player) {}

		/// The i8255 will call this to set a new output value of @c value for @c port.
		void set_value(int port, uint8_t value) {
			switch(port) {
				case 0:
					// Port A is connected to the AY's data bus.
					ay_.update();
					ay_.ay()->set_data_input(value);
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
					ay_.ay()->set_control_lines(
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
				case 0: return ay_.ay()->get_data_output();	// Port A is wired to the AY
				case 1:	return
					(crtc_bus_handler_.get_vsync() ? 0x01 : 0x00) |	// Bit 0 returns CRTC vsync.
					(tape_player_.get_input() ? 0x80 : 0x00) |		// Bit 7 returns cassette input.
					0x7e;	// Bits unimplemented:
							//
							//	Bit 6: printer ready (1 = not)
							//	Bit 5: the expansion port /EXP pin, so depends on connected hardware
							//	Bit 4: 50/60Hz switch (1 = 50Hz)
							//	Bits 1–3: distributor ID (111 = Amstrad)
				default: return 0xff;
			}
		}

	private:
		AYDeferrer &ay_;
		KeyboardState &key_state_;
		const CRTCBusHandler &crtc_bus_handler_;
		Storage::Tape::BinaryTapePlayer &tape_player_;
};

/*!
	The actual Amstrad CPC implementation; tying the 8255, 6845 and AY to the Z80.
*/
class ConcreteMachine:
	public Utility::TypeRecipient,
	public CPU::Z80::BusHandler,
	public Sleeper::SleepObserver,
	public Machine {
	public:
		ConcreteMachine() :
			z80_(*this),
			crtc_counter_(HalfCycles(4)),	// This starts the CRTC exactly out of phase with the CPU's memory accesses
			crtc_(Motorola::CRTC::HD6845S, crtc_bus_handler_),
			crtc_bus_handler_(ram_, interrupt_timer_),
			i8255_(i8255_port_handler_),
			i8255_port_handler_(key_state_, crtc_bus_handler_, ay_, tape_player_),
			tape_player_(8000000) {
			// primary clock is 4Mhz
			set_clock_rate(4000000);

			// ensure memory starts in a random state
			Memory::Fuzz(ram_, sizeof(ram_));

			// register this class as the sleep observer for the FDC and tape
			fdc_.set_sleep_observer(this);
			fdc_is_sleeping_ = fdc_.is_sleeping();

			tape_player_.set_sleep_observer(this);
			tape_player_is_sleeping_ = tape_player_.is_sleeping();
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
			Cycles crtc_cycles = crtc_counter_.divide_cycles(Cycles(4));
			if(crtc_cycles > Cycles(0)) crtc_.run_for(crtc_cycles);
			if(interrupt_timer_.request_has_changed()) z80_.set_interrupt_line(interrupt_timer_.get_request());

			// TODO (in the player, not here): adapt it to accept an input clock rate and
			// run_for as HalfCycles
			if(!tape_player_is_sleeping_) tape_player_.run_for(cycle.length.as_int());

			// Pump the AY
			ay_.run_for(cycle.length);

			// Clock the FDC, if connected, using a lazy scale by two
			if(has_fdc_ && !fdc_is_sleeping_) fdc_.run_for(Cycles(cycle.length.as_int()));

			// Update typing activity
			if(typer_) typer_->run_for(cycle.length);

			// Stop now if no action is strictly required.
			if(!cycle.is_terminal()) return HalfCycles(0);

			uint16_t address = cycle.address ? *cycle.address : 0x0000;
			switch(cycle.operation) {
				case CPU::Z80::PartialMachineCycle::ReadOpcode:
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
					if(has_fdc_ && !(address&0x2000)) {
						upper_rom_ = (*cycle.value == 7) ? ROMType::AMSDOS : rom_model_ + 1;
						if(upper_rom_is_paged_) read_pointers_[3] = roms_[upper_rom_].data();
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
						i8255_.set_register((address >> 8) & 3, *cycle.value);
					}

					// Check for an FDC access
					if(has_fdc_ && (address & 0x580) == 0x100) {
						fdc_.set_register(address & 1, *cycle.value);
					}

					// Check for a disk motor access
					if(has_fdc_ && !(address & 0x580)) {
						fdc_.set_motor_on(!!(*cycle.value));
					}
				break;
				case CPU::Z80::PartialMachineCycle::Input:
					// Default to nothing answering
					*cycle.value = 0xff;

					// Check for a PIO access
					if(!(address & 0x800)) {
						*cycle.value &= i8255_.get_register((address >> 8) & 3);
					}

					// Check for an FDC access
					if(has_fdc_ && (address & 0x580) == 0x100) {
						*cycle.value &= fdc_.get_register(address & 1);
					}

					// Check for a CRTC access; the below is not a typo — the CRTC can be selected
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

			// This implementation doesn't use time-stuffing; once in-phase waits won't be longer
			// than a single cycle so there's no real performance benefit to trying to find the
			// next non-wait when a wait cycle comes in, and there'd be no benefit to reproducing
			// the Z80's knowledge of where wait cycles occur here.
			return HalfCycles(0);
		}

		/// Another Z80 entry point; indicates that a partcular run request has concluded.
		void flush() {
			// Just flush the AY.
			ay_.update();
			ay_.flush();
		}

		/// A CRTMachine function; indicates that outputs should be created now.
		void setup_output(float aspect_ratio) override final {
			crtc_bus_handler_.setup_output(aspect_ratio);
			ay_.setup_output();
			ay_.ay()->set_port_handler(&key_state_);
		}

		/// A CRTMachine function; indicates that outputs should be destroyed now.
		void close_output() override final {
			crtc_bus_handler_.close_output();
			ay_.close_output();
		}

		/// @returns the CRT in use.
		std::shared_ptr<Outputs::CRT::CRT> get_crt() override final {
			return crtc_bus_handler_.get_crt();
		}

		/// @returns the speaker in use.
		std::shared_ptr<Outputs::Speaker> get_speaker() override final {
			return ay_.get_speaker();
		}

		/// Wires virtual-dispatched CRTMachine run_for requests to the static Z80 method.
		void run_for(const Cycles cycles) override final {
			z80_.run_for(cycles);
		}

		/// The ConfigurationTarget entry point; should configure this meachine as described by @c target.
		void configure_as_target(const StaticAnalyser::Target &target) override final  {
			switch(target.amstradcpc.model) {
				case StaticAnalyser::AmstradCPCModel::CPC464:
					rom_model_ = ROMType::OS464;
					has_128k_ = false;
					has_fdc_ = false;
				break;
				case StaticAnalyser::AmstradCPCModel::CPC664:
					rom_model_ = ROMType::OS664;
					has_128k_ = false;
					has_fdc_ = true;
				break;
				case StaticAnalyser::AmstradCPCModel::CPC6128:
					rom_model_ = ROMType::OS6128;
					has_128k_ = true;
					has_fdc_ = true;
				break;
			}

			// Establish default memory map
			upper_rom_is_paged_ = true;
			upper_rom_ = rom_model_ + 1;

			write_pointers_[0] = &ram_[0];
			write_pointers_[1] = &ram_[16384];
			write_pointers_[2] = &ram_[32768];
			write_pointers_[3] = &ram_[49152];

			read_pointers_[0] = roms_[rom_model_].data();
			read_pointers_[1] = write_pointers_[1];
			read_pointers_[2] = write_pointers_[2];
			read_pointers_[3] = roms_[upper_rom_].data();

			// Type whatever is required.
			if(target.loadingCommand.length()) {
				set_typer_for_string(target.loadingCommand.c_str());
			}

			insert_media(target.media);
		}

		bool insert_media(const StaticAnalyser::Media &media) override final {
			// If there are any tapes supplied, use the first of them.
			if(!media.tapes.empty()) {
				tape_player_.set_tape(media.tapes.front());
			}

			// Insert up to four disks.
			int c = 0;
			for(auto &disk : media.disks) {
				fdc_.set_disk(disk, c);
				c++;
				if(c == 4) break;
			}

			return !media.tapes.empty() || (!media.disks.empty() && has_fdc_);
		}

		// See header; provides the system ROMs.
		void set_rom(ROMType type, std::vector<uint8_t> data) override final {
			roms_[(int)type] = data;
		}

		void set_component_is_sleeping(void *component, bool is_sleeping) override final {
			fdc_is_sleeping_ = fdc_.is_sleeping();
			tape_player_is_sleeping_ = tape_player_.is_sleeping();
		}

#pragma mark - Keyboard

		void set_typer_for_string(const char *string) override final {
			std::unique_ptr<CharacterMapper> mapper(new CharacterMapper());
			Utility::TypeRecipient::set_typer_for_string(string, std::move(mapper));
		}

		HalfCycles get_typer_delay() override final {
			return Cycles(4000000);	// Wait 1 second before typing.
		}

		HalfCycles get_typer_frequency() override final {
			return Cycles(160000);	// Type one character per frame.
		}

		// See header; sets a key as either pressed or released.
		void set_key_state(uint16_t key, bool isPressed) override final {
			key_state_.set_is_pressed(isPressed, key >> 4, key & 7);
		}

		// See header; sets all keys to released.
		void clear_all_keys() override final {
			key_state_.clear_all_keys();
		}

	private:
		inline void write_to_gate_array(uint8_t value) {
			switch(value >> 6) {
				case 0: crtc_bus_handler_.select_pen(value & 0x1f);		break;
				case 1: crtc_bus_handler_.set_colour(value & 0x1f);		break;
				case 2:
					// Perform ROM paging.
					read_pointers_[0] = (value & 4) ? write_pointers_[0] : roms_[rom_model_].data();

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
						bool adjust_low_read_pointer = read_pointers_[0] == write_pointers_[0];
						bool adjust_high_read_pointer = read_pointers_[3] == write_pointers_[3];
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

		CPU::Z80::Processor<ConcreteMachine, false> z80_;

		CRTCBusHandler crtc_bus_handler_;
		Motorola::CRTC::CRTC6845<CRTCBusHandler> crtc_;

		AYDeferrer ay_;
		i8255PortHandler i8255_port_handler_;
		Intel::i8255::i8255<i8255PortHandler> i8255_;

		FDC fdc_;

		InterruptTimer interrupt_timer_;
		Storage::Tape::BinaryTapePlayer tape_player_;

		HalfCycles clock_offset_;
		HalfCycles crtc_counter_;
		HalfCycles half_cycles_since_ay_update_;

		uint8_t ram_[128 * 1024];

		std::vector<uint8_t> roms_[7];
		int rom_model_;
		bool has_fdc_, fdc_is_sleeping_;
		bool tape_player_is_sleeping_;
		bool has_128k_;
		bool upper_rom_is_paged_;
		int upper_rom_;

		uint8_t *ram_pages_[4];
		uint8_t *read_pointers_[4];
		uint8_t *write_pointers_[4];

		KeyboardState key_state_;
};

}

using namespace AmstradCPC;

// See header; constructs and returns an instance of the Amstrad CPC.
Machine *Machine::AmstradCPC() {
	return new AmstradCPC::ConcreteMachine;
}

Machine::~Machine() {}
