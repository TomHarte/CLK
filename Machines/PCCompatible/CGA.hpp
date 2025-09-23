//
//  CGA.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/12/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#pragma once

#include "Components/6845/CRTC6845.hpp"
#include "Outputs/CRT/CRT.hpp"
#include "Machines/Utility/ROMCatalogue.hpp"

namespace PCCompatible {

class CGA {
public:
	CGA() : crtc_(outputter_) {}

	static constexpr uint32_t BaseAddress = 0xb'8000;
	static constexpr auto FontROM = ROM::Name::PCCompatibleCGAFont;

	void set_source(const uint8_t *const ram, const std::vector<uint8_t> font) {
		outputter_.ram = ram;
		outputter_.font = font;
	}

	void run_for(const Cycles cycles) {
		// Input rate is the PIT rate of 1,193,182 Hz.
		// CGA is clocked at the real oscillator rate of 12 times that.
		// But there's also an internal divide by 8 to align to the 80-cfetch clock.
		// ... and 12/8 = 3/2.
		full_clock_ += 3 * cycles.as<int>();

		const int modulo = 2 * outputter_.clock_divider;
		crtc_.run_for(Cycles(full_clock_ / modulo));
		full_clock_ %= modulo;
	}

	template <int address>
	void write(const uint8_t value) {
		switch(address) {
			case 0:	case 2:	case 4:	case 6:
				crtc_.select_register(value);
			break;
			case 1:	case 3:	case 5:	case 7:
				crtc_.set_register(value);
			break;

			case 0x8:	outputter_.set_mode(value);		break;
			case 0x9:	outputter_.set_colours(value);	break;
		}
	}

	template <int address>
	uint8_t read() {
		switch(address) {
			case 1:	case 3:	case 5:	case 7:
				return crtc_.get_register();

			case 0xa:
				return
					// b3: 1 => in vsync; 0 => not;
					// b2: 1 => light pen switch is off;
					// b1: 1 => positive edge from light pen has set trigger;
					// b0: 1 => safe to write to VRAM now without causing snow.
					(crtc_.get_bus_state().vsync ? 0b1001 : 0b0000) |
					(crtc_.get_bus_state().display_enable ? 0b0000 : 0b0001) |
					0b0100;

			default: return 0xff;
		}
	}

	// MARK: - Display type configuration.

	void set_display_type(const Outputs::Display::DisplayType display_type) {
		outputter_.crt.set_display_type(display_type);
		outputter_.set_is_composite(Outputs::Display::is_composite(display_type));
	}
	Outputs::Display::DisplayType get_display_type() const {
		return outputter_.crt.get_display_type();
	}

	// MARK: - Call-ins for ScanProducer.

	void set_scan_target(Outputs::Display::ScanTarget *const scan_target) {
		outputter_.crt.set_scan_target(scan_target);
	}
	Outputs::Display::ScanStatus get_scaled_scan_status() const {
		// The CRT is always handed data at the full CGA pixel clock rate, so just
		// divide by 12 to get back to the rate that run_for is being called at.
		return outputter_.crt.get_scaled_scan_status() / 12.0f;
	}

private:
	struct CRTCOutputter {
		enum class OutputState {
			Sync, Pixels, Border, ColourBurst
		};

		CRTCOutputter() :
			crt(910, 8, Outputs::Display::Type::NTSC60, Outputs::Display::InputDataType::Red2Green2Blue2)
		{
			crt.set_visible_area(Outputs::Display::Rect(0.095f, 0.095f, 0.82f, 0.82f));
			crt.set_display_type(Outputs::Display::DisplayType::RGB);
		}

		void set_mode(const uint8_t control) {
			// b5: enable blink
			// b4: 1 => 640x200 graphics
			// b3: video enable
			// b2: 1 => monochrome
			// b1: 1 => 320x200 graphics; 0 => text
			// b0: 1 => 80-column text; 0 => 40

			control_ = control;	// To capture blink, monochrome and video enable bits.

			if(control & 0x2) {
				mode_ = (control & 0x10) ? Mode::Pixels640 : Mode::Pixels320;
				pixels_per_tick = (mode_ == Mode::Pixels640) ? 16 : 8;
			} else {
				mode_ = Mode::Text;
				pixels_per_tick = 8;
			}
			clock_divider = 1 + !(control & 0x01);

			// Both graphics mode and monochrome/colour may have changed, so update the palette.
			update_palette();
		}

		void set_is_composite(const bool is_composite) {
			is_composite_ = is_composite;
			update_palette();
		}

		void set_colours(uint8_t value) {
			colours_ = value;
			update_palette();
		}

		uint8_t control() const {
			return control_;
		}

		void update_hsync(const bool new_hsync) {
			if(new_hsync == previous_hsync) {
				cycles_since_hsync += clock_divider;
			} else {
				cycles_since_hsync = 0;
				previous_hsync = new_hsync;
			}
		}

		OutputState implied_state(const Motorola::CRTC::BusState &state) const {
			OutputState new_state;

			if(state.hsync || state.vsync) {
				new_state = OutputState::Sync;
			} else if(!state.display_enable || !(control_&0x08)) {
				new_state = OutputState::Border;

				// TODO: this isn't correct for colour burst positioning, though
				// it happens to fool the particular CRT I've implemented.
				if(!(control_&4) && cycles_since_hsync <= 6) {
					new_state = OutputState::ColourBurst;
				}
			} else {
				new_state = OutputState::Pixels;
			}

			return new_state;
		}

		void perform_bus_cycle(const Motorola::CRTC::BusState &state) {
			// Determine new output state.
			update_hsync(state.hsync);
			const OutputState new_state = implied_state(state);
			static constexpr uint8_t colour_phase = 200;

			// Upon either a state change or just having accumulated too much local time...
			if(
				new_state != output_state ||
				active_pixels_per_tick != pixels_per_tick ||
				active_clock_divider != clock_divider ||
				active_border_colour != border_colour ||
				count > 912
			) {
				// (1) flush preexisting state.
				if(count) {
					switch(output_state) {
						case OutputState::Sync:			crt.output_sync(count * active_clock_divider);							break;
						case OutputState::Border:
							if(active_border_colour) {
								crt.output_blank(count * active_clock_divider);
							} else {
								crt.output_level<uint8_t>(count * active_clock_divider, active_border_colour);
							}
						break;
						case OutputState::ColourBurst:	crt.output_colour_burst(count * active_clock_divider, colour_phase);	break;
						case OutputState::Pixels:		flush_pixels();															break;
					}
				}

				// (2) adopt new state.
				output_state = new_state;
				active_pixels_per_tick = pixels_per_tick;
				active_clock_divider = clock_divider;
				active_border_colour = border_colour;
				count = 0;
			}

			// Collect pixels if applicable.
			if(output_state == OutputState::Pixels) {
				if(!pixels) {
					pixel_pointer = pixels = crt.begin_data(DefaultAllocationSize);

					// Flush any period where pixels weren't recorded due to back pressure.
					if(pixels && count) {
						crt.output_blank(count * active_clock_divider);
						count = 0;
					}
				}

				if(pixels) {
					if(state.cursor) {
						std::fill(pixel_pointer, pixel_pointer + pixels_per_tick, 0x3f); // i.e. white.
					} else {
						if(mode_ == Mode::Text) {
							serialise_text(state);
						} else {
							serialise_pixels(state);
						}
					}
					pixel_pointer += active_pixels_per_tick;
				}
			}

			// Advance.
			count += 8;

			// Output pixel row prematurely if storage is exhausted.
			if(output_state == OutputState::Pixels && pixel_pointer == pixels + DefaultAllocationSize) {
				flush_pixels();
				count = 0;
			}
		}

		void flush_pixels() {
			crt.output_data(count * active_clock_divider, size_t((count * active_pixels_per_tick) / 8));
			pixels = pixel_pointer = nullptr;
		}

		void serialise_pixels(const Motorola::CRTC::BusState &state) {
			// Refresh address is shifted left and two bytes are fetched, just as if the fetch were for
			// character code + attributes, but producing two bytes worth of graphics.
			//
			// Meanwhile, row address is used as a substitute 14th address line.
			const auto base_address =
				((state.refresh.get() & 0xfff) << 1) +
				((state.line.get() & 1) << 13);
			const uint8_t bitmap[] = {
				ram[base_address],
				ram[base_address + 1],
			};

			if(mode_ == Mode::Pixels320) {
				pixel_pointer[0] = palette320[(bitmap[0] & 0xc0) >> 6];
				pixel_pointer[1] = palette320[(bitmap[0] & 0x30) >> 4];
				pixel_pointer[2] = palette320[(bitmap[0] & 0x0c) >> 2];
				pixel_pointer[3] = palette320[(bitmap[0] & 0x03) >> 0];
				pixel_pointer[4] = palette320[(bitmap[1] & 0xc0) >> 6];
				pixel_pointer[5] = palette320[(bitmap[1] & 0x30) >> 4];
				pixel_pointer[6] = palette320[(bitmap[1] & 0x0c) >> 2];
				pixel_pointer[7] = palette320[(bitmap[1] & 0x03) >> 0];
			} else {
				pixel_pointer[0x0] = palette640[(bitmap[0] & 0x80) >> 7];
				pixel_pointer[0x1] = palette640[(bitmap[0] & 0x40) >> 6];
				pixel_pointer[0x2] = palette640[(bitmap[0] & 0x20) >> 5];
				pixel_pointer[0x3] = palette640[(bitmap[0] & 0x10) >> 4];
				pixel_pointer[0x4] = palette640[(bitmap[0] & 0x08) >> 3];
				pixel_pointer[0x5] = palette640[(bitmap[0] & 0x04) >> 2];
				pixel_pointer[0x6] = palette640[(bitmap[0] & 0x02) >> 1];
				pixel_pointer[0x7] = palette640[(bitmap[0] & 0x01) >> 0];
				pixel_pointer[0x8] = palette640[(bitmap[1] & 0x80) >> 7];
				pixel_pointer[0x9] = palette640[(bitmap[1] & 0x40) >> 6];
				pixel_pointer[0xa] = palette640[(bitmap[1] & 0x20) >> 5];
				pixel_pointer[0xb] = palette640[(bitmap[1] & 0x10) >> 4];
				pixel_pointer[0xc] = palette640[(bitmap[1] & 0x08) >> 3];
				pixel_pointer[0xd] = palette640[(bitmap[1] & 0x04) >> 2];
				pixel_pointer[0xe] = palette640[(bitmap[1] & 0x02) >> 1];
				pixel_pointer[0xf] = palette640[(bitmap[1] & 0x01) >> 0];
			}
		}

		void serialise_text(const Motorola::CRTC::BusState &state) {
			const uint8_t attributes = ram[((state.refresh.get() << 1) + 1) & 0x3fff];
			const uint8_t glyph = ram[((state.refresh.get() << 1) + 0) & 0x3fff];
			const uint8_t row = font[(glyph * 8) + state.line.get()];

			uint8_t colours[2] = { rgb(attributes >> 4), rgbi(attributes) };

			// Apply blink or background intensity.
			if(control_ & 0x20) {
				// Set both colours to black if within a blink; otherwise consider a yellow-to-brown conversion.
				if((attributes & 0x80) && (state.field_count & 16)) {
					colours[0] = colours[1] = 0;
				} else {
					colours[0] = yellow_to_brown(colours[0]);
				}
			} else {
				if(attributes & 0x80) {
					colours[0] = bright(colours[0]);
				} else {
					// Yellow to brown definitely doesn't apply if the colour has been brightened.
					colours[0] = yellow_to_brown(colours[0]);
				}
			}

			// Draw according to ROM contents.
			pixel_pointer[0] = (row & 0x80) ? colours[1] : colours[0];
			pixel_pointer[1] = (row & 0x40) ? colours[1] : colours[0];
			pixel_pointer[2] = (row & 0x20) ? colours[1] : colours[0];
			pixel_pointer[3] = (row & 0x10) ? colours[1] : colours[0];
			pixel_pointer[4] = (row & 0x08) ? colours[1] : colours[0];
			pixel_pointer[5] = (row & 0x04) ? colours[1] : colours[0];
			pixel_pointer[6] = (row & 0x02) ? colours[1] : colours[0];
			pixel_pointer[7] = (row & 0x01) ? colours[1] : colours[0];
		}

		Outputs::CRT::CRT crt;
		static constexpr size_t DefaultAllocationSize = 320;

		// Current output stream.
		uint8_t *pixels = nullptr;
		uint8_t *pixel_pointer = nullptr;
		int active_pixels_per_tick = 8;
		int active_clock_divider = 1;
		uint8_t active_border_colour = 0;

		// Source data.
		const uint8_t *ram = nullptr;
		std::vector<uint8_t> font;

		// CRTC state tracking, for CRT serialisation.
		OutputState output_state = OutputState::Sync;
		int count = 0;

		bool previous_hsync = false;
		int cycles_since_hsync = 0;

		// Current Programmer-set parameters.
		int clock_divider = 1;
		int pixels_per_tick = 8;
		uint8_t colours_ = 0;
		uint8_t control_ = 0;
		bool is_composite_ = false;
		enum class Mode {
			Pixels640, Pixels320, Text,
		} mode_ = Mode::Text;

		uint8_t palette320[4]{};
		uint8_t palette640[2]{};
		uint8_t border_colour;

		void update_palette() {
			// b5: 320x200 palette, unless in monochrome mode.
			if(control_ & 0x04) {
				palette320[1] = DarkCyan;
				palette320[2] = DarkRed;
				palette320[3] = DarkGrey;
			} else {
				if(colours_ & 0x20) {
					palette320[1] = DarkCyan;
					palette320[2] = DarkMagenta;
					palette320[3] = DarkGrey;
				} else {
					palette320[1] = DarkGreen;
					palette320[2] = DarkRed;
					palette320[3] = DarkYellow;
				}
			}

			// b4: set 320x200 palette into high intensity.
			if(colours_ & 0x10) {
				palette320[1] = bright(palette320[1]);
				palette320[2] = bright(palette320[2]);
				palette320[3] = bright(palette320[3]);
			} else {
				// Remap dark yellow to brown if applicable.
				palette320[3] = yellow_to_brown(palette320[3]);
			}

			// b3–b0: set background, border, monochrome colour.
			palette640[1] = palette320[0] = rgbi(colours_);
			border_colour = (mode_ != Mode::Pixels640) ? palette320[0] : 0;
		}

		//
		// Named colours and mapping logic.
		//
		static constexpr uint8_t DarkCyan		= 0b00'10'10;
		static constexpr uint8_t DarkMagenta	= 0b10'00'10;
		static constexpr uint8_t DarkGrey		= 0b10'10'10;

		static constexpr uint8_t DarkGreen		= 0b00'10'00;
		static constexpr uint8_t DarkRed		= 0b10'00'00;
		static constexpr uint8_t DarkYellow		= 0b10'10'00;

		static constexpr uint8_t Brown			= 0b10'01'00;

		/// @returns @c Brown if @c source is @c DarkYellow and composite output is not enabled; @c source otherwise.
		constexpr uint8_t yellow_to_brown(const uint8_t source) {
			return (source == DarkYellow && !is_composite_) ? Brown : source;
		}

		/// @returns The brightened (i.e. high intensity) version of @c source.
		static constexpr uint8_t bright(const uint8_t source) {
			return source | (source >> 1);
		}

		/// Maps the RGB TTL triplet @c source to an appropriate output colour.
		static constexpr uint8_t rgb(const uint8_t source) {
			return uint8_t(
				((source & 0x01) << 1) |
				((source & 0x02) << 2) |
				((source & 0x04) << 3)
			);
		}

		/// Maps the RGBI value in @c source to an appropriate output colour, including potential yellow-to-brown conversion.
		constexpr uint8_t rgbi(const uint8_t source) {
			const uint8_t result = rgb(source);
			return (source & 0x10) ? bright(result) : yellow_to_brown(result);
		}
	} outputter_;
	Motorola::CRTC::CRTC6845<
		CRTCOutputter,
		Motorola::CRTC::Personality::HD6845S,
		Motorola::CRTC::CursorType::MDA> crtc_;

	int full_clock_ = 0;
};

}
